#include "webserver.h"
#include <csignal>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <optional>

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

/**
 * Creates a PID file with the given filename and writes the process ID to it.
 * @param filename The name of the PID file.
 * @param pid The process ID to write to the PID file.
 * @return True if successful, false otherwise.
 */
bool createPidFile(const std::string &filename, int pid) {
    std::ofstream pidfile(filename, std::ios::out | std::ios::trunc);

    if (!pidfile.is_open()) {
        std::cerr << "Error: Unable to open PID file (" << filename << ")"
                  << std::endl;
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
 * Handles fatal errors by printing the error message and terminating the program.
 * @param message Error message to be displayed.
 */
void fatalError(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
    std::exit(1);
}

/**
 * Displays the help text.
 */
void showHelp() {
    std::cout << "Usage: webserver [options]\n"
              << "Options:\n"
              << "  -h, --help       Show this help message and exit\n"
              << "  -V, --version    Show version information and exit\n"
              << "  -p, --port PORT  Specify the port to listen on\n";
}


/**
 * Parses command line options and returns the specified port if available.
 */
std::optional<int> parseCommandLineOptions(int argc, char *argv[]) {
    int port;
    int c;
    static const char optstring[] = "hVp:";

    while ((c = getopt(argc, argv, optstring)) != -1) {
        switch (c) {
        case 'h':
            showHelp();
            std::exit(0);
        case 'V':
            std::cout << "Webserver Version: " << GIT_HASH << std::endl;
            std::exit(0);
        case 'p':
            port = std::stoi(optarg);
            return port;
        case '?':
            showHelp();
            std::exit(1);
        }
    }
    return std::nullopt;
}

/**
 * Sets up the signal handlers for the program.
 */
void setupSignals() {
    if (std::signal(SIGCHLD, reapChildren) == SIG_ERR || std::signal(SIGINT, controlBreak) == SIG_ERR) {
        fatalError("Problem setting signals");
    }
}

/**
 * Entry point for the web server program.
 */
int main(int argc, char *argv[]) {
    const int pid = getpid();
    auto portOpt = parseCommandLineOptions(argc, argv);

    if (!portOpt.has_value()) {
        fatalError("Must specify port!");
    }

    int port = portOpt.value();
    std::cout << "Starting on port " << port << " process ID: " << pid << std::endl;

    setupSignals();

    if (chdir("base") == -1) {
        perror("chdir");
        return 1;
    }

    Http webserver;

    createPidFile("fishjelly.pid", pid);
    webserver.start(port);

    return 0;
}
