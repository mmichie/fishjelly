#include "webserver.h"
#include "asio_server.h"
#include "asio_ssl_server.h"
#include "ssl_context.h"
#include <argparse.hpp>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

/**
 * Handles fatal errors by printing the error message and terminating the
 * program.
 * @param message Error message to be displayed.
 */
void fatalError(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
    std::exit(1);
}

/**
 * Initialize the server as a daemon.
 */
void initializeDaemon() {
    pid_t pid, sid;

    // Fork off the parent process
    pid = fork();

    // Check for fork error
    if (pid < 0) {
        fatalError("Failed to fork daemon");
    }

    // Exit the parent process if fork was successful
    if (pid > 0) {
        std::exit(EXIT_SUCCESS);
    }

    // Change the file mode mask
    umask(0);

    // Open any logs here if needed

    // Create a new SID for the child process
    sid = setsid();
    if (sid < 0) {
        fatalError("Failed to create a new SID");
    }

    // Change the current working directory
    if ((chdir("/")) < 0) {
        fatalError("Failed to change directory");
    }

    // Close out the standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

/**
 * Creates a PID file with the given filename and writes the process ID to it.
 * @param filename The name of the PID file.
 * @param pid The process ID to write to the PID file.
 * @return True if successful, false otherwise.
 */
bool createPidFile(const std::string& filename, int pid) {
    std::ofstream pidfile(filename, std::ios::out | std::ios::trunc);

    if (!pidfile.is_open()) {
        std::cerr << "Error: Unable to open PID file (" << filename << ")" << std::endl;
        return false;
    }

    pidfile << pid << std::endl;
    return true;
}

/**
 * Signal handler for cleaning up child processes.
 * @param sigNo The signal number.
 */
void reapChildren(int sigNo) {
    (void)sigNo; // Suppress unused parameter warning

    while (waitpid(-1, nullptr, WNOHANG) > 0)
        ;
}

/**
 * Signal handler for handling interrupt signals like Ctrl+C.
 * @param sigNo The signal number.
 */
void controlBreak(int sigNo) {
    (void)sigNo; // Suppress unused parameter warning

    std::cout << "Exiting program now..." << std::endl;
    std::exit(0);
}

/**
 * Parses command line options using argparse.
 */
struct CommandLineArgs {
    int port;
    bool daemon;
    int test_requests;           // Exit after N requests (0 = run forever)
    bool use_asio;               // Use ASIO instead of fork model
    int read_timeout;            // Read timeout in seconds
    int write_timeout;           // Write timeout in seconds
    int workers;                 // Number of worker processes (0 = traditional fork model)
    int max_requests_per_worker; // Max requests per worker before restart
    bool use_ssl;                // Enable SSL/TLS (requires --asio)
    int ssl_port;                // SSL port (default: 443)
    std::string ssl_cert;        // Path to SSL certificate
    std::string ssl_key;         // Path to SSL private key
    std::string ssl_dh;          // Path to DH parameters (optional)
};

CommandLineArgs parseCommandLineOptions(int argc, char* argv[]) {
    argparse::ArgumentParser program("shelob", GIT_HASH);

    program.add_description("A lightweight C++ web server");
    program.add_epilog("Example: shelob -p 8080 -d");

    program.add_argument("-p", "--port")
        .help("specify the port to listen on")
        .default_value(8080)
        .scan<'i', int>()
        .metavar("PORT");

    program.add_argument("-d", "--daemon")
        .help("run the server in daemon mode")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("-t", "--test")
        .help("test mode: exit after N requests")
        .default_value(0)
        .scan<'i', int>()
        .metavar("N");

    program.add_argument("-a", "--asio")
        .help("use ASIO coroutines instead of fork model")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--read-timeout")
        .help("read timeout in seconds (0 = no timeout)")
        .default_value(30)
        .scan<'i', int>()
        .metavar("SECONDS");

    program.add_argument("--write-timeout")
        .help("write timeout in seconds (0 = no timeout)")
        .default_value(30)
        .scan<'i', int>()
        .metavar("SECONDS");

    program.add_argument("--workers")
        .help("number of worker processes for connection pooling (0 = traditional fork model)")
        .default_value(0)
        .scan<'i', int>()
        .metavar("N");

    program.add_argument("--max-requests-per-worker")
        .help("maximum requests per worker before restart (prevents memory leaks)")
        .default_value(1000)
        .scan<'i', int>()
        .metavar("N");

    program.add_argument("--ssl")
        .help("enable SSL/TLS (requires --asio)")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--ssl-port")
        .help("SSL port to listen on")
        .default_value(443)
        .scan<'i', int>()
        .metavar("PORT");

    program.add_argument("--ssl-cert")
        .help("path to SSL certificate file (PEM format)")
        .default_value(std::string("ssl/server-cert.pem"))
        .metavar("FILE");

    program.add_argument("--ssl-key")
        .help("path to SSL private key file (PEM format)")
        .default_value(std::string("ssl/server-key.pem"))
        .metavar("FILE");

    program.add_argument("--ssl-dh")
        .help("path to DH parameters file (optional)")
        .default_value(std::string("ssl/dhparam.pem"))
        .metavar("FILE");

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    // Handle version flag (argparse automatically handles -v/--version)

    return {.port = program.get<int>("--port"),
            .daemon = program.get<bool>("--daemon"),
            .test_requests = program.get<int>("--test"),
            .use_asio = program.get<bool>("--asio"),
            .read_timeout = program.get<int>("--read-timeout"),
            .write_timeout = program.get<int>("--write-timeout"),
            .workers = program.get<int>("--workers"),
            .max_requests_per_worker = program.get<int>("--max-requests-per-worker"),
            .use_ssl = program.get<bool>("--ssl"),
            .ssl_port = program.get<int>("--ssl-port"),
            .ssl_cert = program.get<std::string>("--ssl-cert"),
            .ssl_key = program.get<std::string>("--ssl-key"),
            .ssl_dh = program.get<std::string>("--ssl-dh")};
}

/**
 * Sets up the signal handlers for the program.
 */
void setupSignals() {
    if (std::signal(SIGCHLD, reapChildren) == SIG_ERR ||
        std::signal(SIGINT, controlBreak) == SIG_ERR) {
        fatalError("Problem setting signals");
    }
}

/**
 * Entry point for the web server program.
 */
int main(int argc, char* argv[]) {
    const int pid = getpid();
    auto args = parseCommandLineOptions(argc, argv);

    // Output the port and PID before potentially going into daemon mode.
    std::cout << "Starting on port " << args.port << " process ID: " << pid << std::endl;

    if (args.daemon) {
        initializeDaemon();
    }

    setupSignals();

    // Change to base directory if it exists and we're not already in it
    try {
        auto current = std::filesystem::current_path();
        // Check if we're not already in the base directory
        if (current.filename() != "base" && std::filesystem::exists("base")) {
            std::filesystem::current_path("base");
        } else if (current.filename() != "base" && !std::filesystem::exists("htdocs")) {
            // Not in base and no base subdirectory exists
            std::cerr << "Error: Cannot find base directory. Please run from project root or base "
                         "directory."
                      << std::endl;
            return 1;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error changing directory: " << e.what() << std::endl;
        return 1;
    }

    createPidFile("fishjelly.pid", pid);

    if (args.use_asio) {
        if (args.use_ssl) {
            // Use ASIO SSL server
            try {
                SSLContext ssl_context;
                ssl_context.load_certificate(args.ssl_cert);
                ssl_context.load_private_key(args.ssl_key);

                // Load DH parameters if file exists
                if (std::filesystem::exists(args.ssl_dh)) {
                    ssl_context.load_dh_params(args.ssl_dh);
                }

                AsioSSLServer server(args.ssl_port, ssl_context, args.test_requests);
                server.run();
            } catch (const std::exception& e) {
                std::cerr << "SSL Error: " << e.what() << std::endl;
                std::cerr << "\nTo generate test certificates, run:" << std::endl;
                std::cerr << "  scripts/generate-ssl-cert.sh" << std::endl;
                return 1;
            }
        } else {
            // Use ASIO-based server (HTTP)
            AsioServer server(args.port, args.test_requests);
            server.run();
        }
    } else {
        if (args.use_ssl) {
            std::cerr << "Error: SSL/TLS requires --asio flag" << std::endl;
            std::cerr << "Usage: shelob --asio --ssl --ssl-port 8443" << std::endl;
            return 1;
        }
        // Use traditional fork-based server
        Http webserver;
        webserver.setTestMode(args.test_requests);
        webserver.setMaxRequestsPerWorker(args.max_requests_per_worker);
        webserver.start(args.port, args.read_timeout, args.write_timeout, args.workers);
    }

    return 0;
}
