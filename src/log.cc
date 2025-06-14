#include "log.h"
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

// Singleton instance getter - thread safe in C++11
Log& Log::getInstance() {
    static Log instance;
    return instance;
}

// Destructor - close log file if open
Log::~Log() {
    if (logfile.is_open()) {
        logfile.close();
    }
}

bool Log::openLogFile(std::string_view filename) {
    std::lock_guard<std::mutex> lock(log_mutex);

    // If already open with the same filename, just return true
    if (logfile.is_open() && current_filename == filename) {
        return true;
    }

    // If open with different filename, close current file
    if (logfile.is_open()) {
        logfile.close();
    }

    // Ensure the directory exists
    std::filesystem::path logPath(filename);
    if (!std::filesystem::exists(logPath.parent_path())) {
        std::filesystem::create_directories(logPath.parent_path());
    }

    logfile.open(std::string(filename), std::ios::out | std::ios::app);
    if (logfile.is_open()) {
        current_filename = filename;
        if constexpr (DEBUG) {
            std::cout << "Opened log file: " << filename << "\n";
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

bool Log::writeLogLine(std::string_view ip, std::string_view request, int code, int size,
                       std::string_view referrer, std::string_view agent) {
    std::lock_guard<std::mutex> lock(log_mutex);

    if (logfile.is_open()) {
        logfile << ip << " - - " << makeDate() << " \"" << request << "\" " << code << ' ' << size
                << " \"" << referrer << "\" \"" << agent << "\"\n";
        logfile.flush(); // Ensure data is written immediately
        return true;
    } else {
        std::cerr << "Unable to write to logfile\n";
        return false;
    }
}
