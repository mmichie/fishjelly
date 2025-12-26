# Fishjelly Security Architecture Proposal

This document proposes a Postfix-inspired security architecture for fishjelly,
based on the design principles developed by Wietse Venema for the Postfix mail
server.

## Background: Postfix Security Model

Postfix (1998) was designed as a secure replacement for sendmail. Its security
architecture is built on principles that have proven effective over 25+ years:

1. **Process Separation** - Each function runs in its own process
2. **Least Privilege** - Only one process (master) runs as root
3. **Chroot Isolation** - Most processes run in restricted filesystem views
4. **Untrusting IPC** - Processes communicate via well-defined protocols
5. **Queue-Based Flow** - Work passes through queues, not direct calls
6. **Minimal Attack Surface** - Network-facing code separated from privileges

Venema's key insight: "You don't make a system secure by patching the holes -
if the system wasn't built to be secure then it never will be."

## Current Architecture

Fishjelly currently uses a single-process, async coroutine model:

```
┌─────────────────────────────────────────────────────────────┐
│                    Single Process                           │
│                                                             │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐       │
│  │ Accept  │─▶│  SSL    │─▶│  HTTP   │─▶│  File   │       │
│  │         │  │         │  │ Parse   │  │ Serve   │       │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘       │
│       │            │            │            │             │
│       └────────────┴────────────┴────────────┘             │
│                 Shared Address Space                        │
│                 Single Privilege Level                      │
│                 No Isolation                                │
└─────────────────────────────────────────────────────────────┘
```

**Characteristics:**
- Single process with Boost.ASIO coroutines
- All components share memory and privileges
- Compromise of any component = full server compromise
- No privilege separation or chroot isolation

## Proposed Architecture

### Process Overview

```
                                    ┌─────────────────────┐
                                    │   master (root)     │
                                    │                     │
                                    │ - Binds ports 80/443│
                                    │ - Spawns workers    │
                                    │ - Monitors health   │
                                    │ - Restarts failed   │
                                    └──────────┬──────────┘
                                               │
          ┌────────────────┬───────────────────┼───────────────┬────────────────┐
          │                │                   │               │                │
          ▼                ▼                   ▼               ▼                ▼
   ┌─────────────┐  ┌─────────────┐    ┌─────────────┐  ┌──────────┐    ┌──────────┐
   │   acceptd   │  │    ssld     │    │    httpd    │  │  filed   │    │   logd   │
   │             │  │             │    │             │  │          │    │          │
   │ User: nobody│  │ User: nobody│    │ User: www   │  │ User: www│    │ User: log│
   │ Chroot: yes │  │ Chroot: yes │    │ Chroot: yes │  │ Chroot:  │    │ Chroot:  │
   │             │  │             │    │             │  │ htdocs/  │    │ logs/    │
   │ Accept TCP  │  │ TLS decrypt │    │ Parse HTTP  │  │ Read     │    │ Append   │
   │ Pass fd     │  │ Pass clear  │    │ Route req   │  │ files    │    │ logs     │
   └──────┬──────┘  └──────┬──────┘    └──────┬──────┘  └────┬─────┘    └────┬─────┘
          │                │                   │              │               │
          └────────────────┴───────────────────┴──────────────┴───────────────┘
                                   Unix Domain Sockets
                                   File Descriptor Passing
```

### Daemon Specifications

#### master

The supervisor daemon, similar to Postfix's master(8).

| Attribute | Value |
|-----------|-------|
| Runs as | root |
| Chroot | No |
| Network | Binds 80, 443 |
| Files | PID file only |
| Function | Spawn/monitor workers, pass listening sockets |

Responsibilities:
- Bind privileged ports before dropping privileges
- Fork worker processes with appropriate credentials
- Monitor worker health, restart on failure
- Handle SIGHUP for configuration reload
- Graceful shutdown on SIGTERM

#### acceptd

Connection acceptor daemon.

| Attribute | Value |
|-----------|-------|
| Runs as | nobody |
| Chroot | /var/empty |
| Network | Accept on passed socket |
| Files | None |
| Function | Accept connections, pass fd to ssld/httpd |

Responsibilities:
- Accept incoming TCP connections
- Implement connection limits per IP
- Pass file descriptor to next stage via Unix socket
- No access to any files or data

#### ssld

TLS termination daemon.

| Attribute | Value |
|-----------|-------|
| Runs as | nobody |
| Chroot | /var/empty (certs loaded before chroot) |
| Network | Receives encrypted fd, outputs cleartext |
| Files | SSL certs (read at startup, before chroot) |
| Function | TLS handshake and decryption only |

Responsibilities:
- Perform TLS handshake with timeout
- Decrypt traffic, pass cleartext to httpd
- ALPN negotiation for HTTP/2
- No access to request content semantics

#### httpd

HTTP protocol handler daemon.

| Attribute | Value |
|-----------|-------|
| Runs as | www-data |
| Chroot | /var/empty |
| Network | Receives cleartext from ssld |
| Files | None |
| Function | Parse HTTP, route requests |

Responsibilities:
- Parse HTTP/1.1 and HTTP/2 frames
- Validate request syntax and headers
- Enforce size limits (headers, body)
- Route requests to appropriate handler
- Request filed for static content
- Request authd for authentication checks

#### filed

Static file server daemon.

| Attribute | Value |
|-----------|-------|
| Runs as | www-data |
| Chroot | htdocs/ |
| Network | Receives requests from httpd |
| Files | Read-only access to htdocs/ |
| Function | Serve static files |

Responsibilities:
- Receive file path from httpd
- Validate path is within chroot (defense in depth)
- Read file, return content
- Handle byte-range requests
- Check If-Modified-Since

#### authd

Authentication daemon.

| Attribute | Value |
|-----------|-------|
| Runs as | auth |
| Chroot | /var/empty (credentials loaded at start) |
| Network | Receives auth requests from httpd |
| Files | Password database (read at startup) |
| Function | Validate credentials |

Responsibilities:
- Receive authentication requests
- Validate Basic/Digest credentials
- Return allow/deny decision
- Never expose password hashes to other daemons

#### logd

Logging daemon.

| Attribute | Value |
|-----------|-------|
| Runs as | log |
| Chroot | logs/ |
| Network | Receives log entries from all daemons |
| Files | Write-only access to log files |
| Function | Append access logs |

Responsibilities:
- Receive structured log entries via Unix socket
- Append to access.log
- Handle log rotation signals
- Buffer writes for performance

### Inter-Process Communication

All daemons communicate via Unix domain sockets using well-defined protocols.

#### File Descriptor Passing

Used between acceptd -> ssld -> httpd for connection handoff:

```c
// Send file descriptor over Unix socket
struct msghdr msg = {0};
struct cmsghdr *cmsg;
char buf[CMSG_SPACE(sizeof(int))];

msg.msg_control = buf;
msg.msg_controllen = sizeof(buf);
cmsg = CMSG_FIRSTHDR(&msg);
cmsg->cmsg_level = SOL_SOCKET;
cmsg->cmsg_type = SCM_RIGHTS;
cmsg->cmsg_len = CMSG_LEN(sizeof(int));
*(int *)CMSG_DATA(cmsg) = client_fd;

sendmsg(unix_socket, &msg, 0);
```

#### Request Protocol

Simple line-based protocol for daemon requests:

```
# httpd -> filed
REQUEST <request-id>
PATH /index.html
RANGE bytes=0-1023
END

# filed -> httpd
RESPONSE <request-id>
STATUS 200
CONTENT-TYPE text/html
CONTENT-LENGTH 4096
END
<file data follows>
```

#### Log Protocol

Structured log entries to logd:

```
LOG <timestamp> <client-ip> <method> <path> <status> <bytes> <referer> <user-agent>
```

### Privilege Matrix

| Daemon | Root | Network In | Network Out | Read Files | Write Files | Chroot |
|--------|------|------------|-------------|------------|-------------|--------|
| master | Yes | Bind only | No | PID file | PID file | No |
| acceptd | No | Accept | No | No | No | Yes |
| ssld | No | Encrypted | Cleartext | Certs* | No | Yes |
| httpd | No | Cleartext | Responses | No | No | Yes |
| filed | No | Requests | Responses | htdocs/ | No | Yes |
| authd | No | Auth req | Auth resp | Creds* | No | Yes |
| logd | No | Log entries | No | No | logs/ | Yes |

*Loaded at startup before chroot

### Compromise Scenarios

#### Scenario 1: HTTP Parser Vulnerability

Attacker exploits bug in HTTP parsing (httpd).

**Current architecture:** Full server compromise. Attacker can read SSL keys,
modify files, access all memory.

**Proposed architecture:** Attacker controls httpd process which:
- Is chrooted to /var/empty (no files)
- Runs as www-data (unprivileged)
- Cannot read htdocs (that's filed)
- Cannot read SSL keys (loaded by ssld before chroot)
- Cannot write logs (that's logd)
- Cannot bind new ports (that's master)

Blast radius: One daemon, no persistent access.

#### Scenario 2: SSL Library Vulnerability

Attacker exploits bug in OpenSSL (ssld).

**Current architecture:** Full server compromise.

**Proposed architecture:** Attacker controls ssld which:
- Is chrooted to /var/empty
- Has SSL keys in memory (unavoidable)
- Cannot access htdocs or logs
- Cannot modify httpd behavior
- Cannot escalate to root

Blast radius: TLS interception for that connection only.

#### Scenario 3: Path Traversal in File Server

Attacker bypasses path validation (filed).

**Current architecture:** Can read any file as server user.

**Proposed architecture:** filed is chrooted to htdocs/:
- Cannot escape chroot even with path traversal
- Cannot read /etc/passwd, SSL keys, etc.
- Defense in depth: validation + chroot

Blast radius: Read access limited to htdocs/ (intended anyway).

### Implementation Phases

#### Phase 1: Master/Worker Split

Separate privileged startup from request handling.

```
master (root)
    │
    ├── bind ports 80, 443
    ├── load SSL certificates
    ├── fork()
    │
    └── worker (www-data)
            │
            ├── drop privileges
            ├── enter chroot
            └── handle requests (current architecture)
```

Benefits:
- Port binding separated from request handling
- Worker runs unprivileged
- Minimal code change

#### Phase 2: Separate Logging

Extract logging to dedicated daemon.

```
master
    │
    ├── worker (www-data)
    │       │
    │       └── send log entries via Unix socket
    │
    └── logd (log)
            │
            └── chrooted to logs/, write-only
```

Benefits:
- Log injection attacks contained
- Worker cannot tamper with logs
- Append-only audit trail

#### Phase 3: Separate File Serving

Extract static file serving to dedicated daemon.

```
master
    │
    ├── httpd (www-data, chroot /var/empty)
    │       │
    │       └── request files via Unix socket
    │
    ├── filed (www-data, chroot htdocs/)
    │       │
    │       └── read-only file access
    │
    └── logd (log, chroot logs/)
```

Benefits:
- HTTP parser cannot access filesystem
- Path traversal contained by chroot
- Clear separation of concerns

#### Phase 4: Full Separation

Complete Postfix-style architecture with all daemons.

Benefits:
- Maximum isolation
- Minimum privilege per component
- Defense in depth at every layer

### Configuration

Proposed configuration file format:

```ini
[master]
pid_file = /var/run/fishjelly.pid
user = root

[acceptd]
user = nobody
chroot = /var/empty
max_connections = 10000
max_connections_per_ip = 100

[ssld]
user = nobody
chroot = /var/empty
cert_file = /etc/fishjelly/ssl/server.crt
key_file = /etc/fishjelly/ssl/server.key
protocols = TLSv1.2 TLSv1.3

[httpd]
user = www-data
chroot = /var/empty
max_header_size = 8192
max_body_size = 10485760
timeout_header = 10
timeout_body = 30
timeout_response = 60

[filed]
user = www-data
chroot = /var/www/htdocs
index_files = index.html index.htm

[authd]
user = auth
chroot = /var/empty
password_file = /etc/fishjelly/passwd
hash_algorithm = argon2id

[logd]
user = log
chroot = /var/log/fishjelly
access_log = access.log
error_log = error.log
buffer_size = 4096
```

### Platform Considerations

#### Linux

- Use `prctl(PR_SET_NO_NEW_PRIVS)` to prevent privilege escalation
- Use seccomp-bpf to restrict system calls per daemon
- Use namespaces for additional isolation (optional)
- Use capabilities instead of full root where possible

#### macOS

- Use sandbox-exec for process sandboxing
- Chroot requires root; consider App Sandbox instead
- Use Gatekeeper and code signing

#### FreeBSD

- Use Capsicum for capability-based sandboxing
- Native chroot support
- Use jail(8) for additional isolation

### Monitoring and Health Checks

Master daemon monitors worker health:

```
[health_check]
interval = 30
timeout = 5
max_failures = 3
restart_delay = 1

# Per-daemon checks
acceptd_check = connections_accepted > 0
ssld_check = handshakes_completed > 0
httpd_check = requests_processed > 0
filed_check = files_served > 0
logd_check = entries_written > 0
```

### Metrics Export

Each daemon exports metrics via Unix socket to master:

```
# acceptd metrics
acceptd_connections_total 123456
acceptd_connections_active 42
acceptd_connections_rejected 100

# httpd metrics
httpd_requests_total 100000
httpd_requests_2xx 95000
httpd_requests_4xx 4500
httpd_requests_5xx 500
httpd_request_duration_seconds_bucket{le="0.01"} 80000
```

## References

- [Postfix Architecture Overview](http://www.postfix.org/OVERVIEW.html)
- [Catching up with Wietse Venema](https://www.postfix.org/linuxsecurity-200407.html)
- [Q&A: Wietse Venema](https://www.postfix.org/sendmail.200004/interviewvenema.html)
- [Anatomy of Postfix - Linux Journal](https://www.linuxjournal.com/article/9454)
- Venema, W. "The Postfix mail server as a secure programming example"
