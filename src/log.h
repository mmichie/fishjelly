#ifndef SHELOB_LOG_H
#define SHELOB_LOG_H 1

#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

#include "global.h"

class Log {
  public:
    // Get the singleton instance
    static Log& getInstance();

    // Public methods
    bool openLogFile(std::string_view filename);
    bool closeLogFile();
    bool writeLogLine(std::string_view ip, std::string_view request, int code, int size,
                      std::string_view referrer, std::string_view agent);

    // Delete copy constructor and assignment operator
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

  private:
    // Private constructor
    Log() = default;
    ~Log();

    std::string makeDate();
    std::ofstream logfile;
    std::mutex log_mutex;         // For thread safety
    std::string current_filename; // Track the current log file
};

#endif /* !SHELOB_LOG_H */
