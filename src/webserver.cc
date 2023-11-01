#include "webserver.h"
#include <sys/stat.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <optional>
#include <unistd.h>

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

/**
 * Handles fatal errors by printing the error message and terminating the
 * program.
 * @param message Error message to be displayed.
 */
void fatalError(const std::string &message) {
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
 * Displays the help text.
 */
void showHelp() {
    std::cout << "Usage: webserver [options]\n"
              << "Options:\n"
              << "  -h, --help       Show this help message and exit\n"
              << "  -V, --version    Show version information and exit\n"
              << "  -d, --daemon     Run the server in daemon mode\n"
              << "  -p, --port PORT  Specify the port to listen on\n";
}

/**
 * Parses command line options and returns the specified port if available.
 */
std::optional<int> parseCommandLineOptions(int argc, char *argv[], bool &daemonMode) {
    int port;
    int c;
    static const char optstring[] = "hVdp:";

    while ((c = getopt(argc, argv, optstring)) != -1) {
        switch (c) {
        case 'd':
            daemonMode = true;
            break;
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
    if (std::signal(SIGCHLD, reapChildren) == SIG_ERR ||
        std::signal(SIGINT, controlBreak) == SIG_ERR) {
        fatalError("Problem setting signals");
    }
}

/**
 * Entry point for the web server program.
 */
int main(int argc, char *argv[]) {
    bool daemonMode = false;

    const int pid = getpid();
    auto portOpt = parseCommandLineOptions(argc, argv, daemonMode);

    if (!portOpt.has_value()) {
        fatalError("Must specify port!");
    }

    // Output the port and PID before potentially going into daemon mode.
    int port = portOpt.value();
    std::cout << "Starting on port " << port << " process ID: " << pid << std::endl;

    if (daemonMode) {
        initializeDaemon();
    }

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
