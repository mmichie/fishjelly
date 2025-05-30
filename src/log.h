#ifndef SHELOB_LOG_H
#define SHELOB_LOG_H 1

#include <fstream>
#include <string>
#include <string_view>

#include "global.h"

class Log {
  public:
    bool closeLogFile();
    bool openLogFile(std::string_view filename);
    bool writeLogLine(std::string_view ip, std::string_view request, int code, int size,
                      std::string_view referrer, std::string_view agent);

  private:
    std::string makeDate();
    std::ofstream logfile;
};

#endif /* !SHELOB_LOG_H */
