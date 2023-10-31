// webserver.cc
#include "webserver.h"
#include <csignal>
#include <fstream>
#include <iostream>
#include <unistd.h>

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

void controlBreak(int sigNo) {
    std::cout << "Exiting program now..." << std::endl;
    std::exit(0);
}

void reapChildren(int sigNo) {
    while (waitpid(-1, nullptr, WNOHANG) > 0)
        ;
}

Http webserver;

int main(int argc, char *argv[]) {
    int port;
    const int pid = getpid();

    if (argc < 2) {
        std::cerr << "Error: must specify port!" << std::endl;
        return 1;
    }

    int c;
    static const char optstring[] = "hVp:";

    while ((c = getopt(argc, argv, optstring)) != -1) {
        switch (c) {
        case 'h':
            std::cout << "Help me Elvis, help me!" << std::endl;
            return 0;
        case 'V':
            std::cout << "Shelob Version foo" << std::endl;
            return 0;
        case 'p':
            port = std::stoi(optarg);
            std::cout << "Starting on port " << port << " process ID: " << pid
                      << std::endl;
            break;
        case '?':
            std::cerr << "Unknown option" << std::endl;
        }
    }

    if (std::signal(SIGCHLD, reapChildren) == SIG_ERR) {
        std::cerr << "Error: Problem setting SIGCHLD, exiting with error 1!"
                  << std::endl;
        return 1;
    }

    if (std::signal(SIGINT, controlBreak) == SIG_ERR) {
        std::cerr << "Error: Problem setting SIGINT, exiting with error 1!"
                  << std::endl;
        return 1;
    }

    if (chdir("base") == -1) {
        std::perror("chdir");
        return 1;
    }

    createPidFile("fishjelly.pid", pid);
    webserver.start(port);

    return 0;
}
