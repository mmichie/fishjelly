#include "log.h"
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

bool Log::openLogFile(std::string_view filename) {
    if (logfile.is_open()) {
        if constexpr (DEBUG) {
            std::cout << "Log file already open!\n";
        }
        return true;
    }

    // Ensure the directory exists
    std::filesystem::path logPath(filename);
    if (!std::filesystem::exists(logPath.parent_path())) {
        std::filesystem::create_directories(logPath.parent_path());
    }

    logfile.open(std::string(filename), std::ios::out | std::ios::app);
    if (logfile.is_open()) {
        if constexpr (DEBUG) {
            std::cout << "Opened log file\n";
        }
        return true;
    } else {
        std::cerr << "Error: Unable to open log file (" << filename << ")\n";
        return false;
    }
}

bool Log::closeLogFile() {
    if (logfile.is_open()) {
        logfile.close();
        return true;
    } else {
        return false;
    }
}

std::string Log::makeDate() {
    auto t = std::time(nullptr);
    auto tm = *std::gmtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "[%d/%b/%Y:%H:%M:%S %z]");
    return oss.str();
}

bool Log::writeLogLine(std::string_view ip, std::string_view request,
                       int code, int size, std::string_view referrer,
                       std::string_view agent) {
    if (logfile.is_open()) {
        logfile << ip << " - - " << makeDate() << " \"" << request << "\" "
                << code << ' ' << size << " \"" << referrer << "\" \"" << agent
                << "\"\n";
        return true;
    } else {
        std::cerr << "Unable to write to logfile\n";
        return false;
    }
}
