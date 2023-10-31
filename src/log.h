#ifndef SHELOB_LOG_H
#define SHELOB_LOG_H 1

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "global.h"

class Log {
  public:
    bool closeLogFile();
    bool openLogFile(const std::string& filename);
    bool writeLogLine(const std::string& ip, const std::string& request, int code, int size, const std::string& referrer, const std::string& agent);


  private:
    string makeDate();
    ofstream logfile;
};

#endif /* !SHELOB_LOG_H */
