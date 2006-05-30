#include "webserv.h"

bool Log::openLogFile(string filename) 
{
    if (logfile.is_open()) {
        if (DEBUG)
            cout << "Log file already open!\n";
        return true;
    }

	/* TODO: Check when opening files - can an attacker redirect it (via symlinks),
       force the opening of special file type (e.g., device files), move
       things around to create a race condition, control its ancestors, or change
       its contents?
    */ 
    logfile.open(filename.c_str(), ios::out | ios::app);
    if (logfile.is_open()) { 
        if (DEBUG) {
            cout << "Opened log file\n";
        }

        return true;

    } else {
        cerr << "Error: Unable to open log file (" << filename << ")" << endl;
        return false;
    }
}

bool Log::closeLogFile() 
{
    if (!logfile.is_open()) {
        logfile.close(); 
        return true;
    } else
        return false;
}

string Log::makeDate()
{
    ostringstream date;
    ostringstream cdate;

    char buf[50];
    time_t ltime;
    struct tm *today;
    ltime = time(NULL);
    today = gmtime(&ltime);

    strftime(buf, sizeof(buf), "[%d/%b/%Y:%H:%M:%S %z]", today);
    
    string buffer(buf);

    return buffer;
}


/*
198.7.247.203 - - [24/May/2006:13:07:19 -0600] "GET / HTTP/1.1" 200 9669 "http://hivearchive.com/" "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.0; .NET
CLR 1.0.3705)"
*/
bool Log::writeLogLine(string ip, string request, int code, int size, string referrer, string agent) 
{
    if (logfile.is_open()) {
        logfile << ip << " - - " << " " << this->makeDate() << " \"" << request << "\"" << " " << code 
                << " " << size << " \"" << referrer << "\" \"" << agent << "\"\n";
        return true;
    } else {
        cerr << "Unable to write to logfile\n";
        return false;
    }
}
