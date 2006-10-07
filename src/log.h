#include "global.h"

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

class Log {
public:
    bool openLogFile(string filename);
    bool closeLogFile();
    bool writeLogLine(string ip, string request, int code, int size, string referrer, string agent);

private:
    string makeDate();
    ofstream logfile;
};