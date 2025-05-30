# fishjelly
C++ Web Server

## Build

```bash
# Install meson if needed (e.g., brew install meson on macOS)
meson setup builddir
cd builddir
meson compile

# Run the server
./shelob -p 8080
```

## Usage

```bash
./shelob [options]
  -h, --help       Show help message and exit
  -V, --version    Show version information and exit
  -d, --daemon     Run the server in daemon mode
  -p, --port PORT  Specify the port to listen on (required)
```

Example: `./shelob -p 8080 -d` (run on port 8080 in daemon mode)