#include "webserver.h"
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

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    // Handle version flag (argparse automatically handles -v/--version)

    return {.port = program.get<int>("--port"), .daemon = program.get<bool>("--daemon")};
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

    try {
        std::filesystem::current_path("base");
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error changing directory: " << e.what() << std::endl;
        return 1;
    }

    Http webserver;

    createPidFile("fishjelly.pid", pid);
    webserver.start(args.port);

    return 0;
}
