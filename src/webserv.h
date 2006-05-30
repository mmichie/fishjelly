#if HAVE_CONFIG_H
#include <config.h>
#endif

#define DEBUG 0

#include <iostream>
#include <string>
#include <map>
#include <memory>

#include <algorithm>
#include <iterator>
#include <vector>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <sstream>


#include <cstdio>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <sys/stat.h>

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#else
#include <sys/dir.h>
#endif

//#include <sys/dir.h>

using namespace std;

class Socket {

private:
    int socket_fd, sin_size;    
    FILE *socket_fp;    
    struct sockaddr_in server;     
    const static int NUM_CLIENTS_TO_QUEUE = 10; 
    void serverBind(int server_port);
  
public:  
    int accept_fd, pid;

    struct sockaddr_in client;

    void acceptClient();
    bool readLine(string *buffer);
    void writeLine(string line);
    void closeSocket();

    // Constructor
    Socket(int server_port) {
		//Bind the port
		serverBind(server_port);	
    }
};

class Http {

private:
    void printDate(void);
    void printServer(void);
    void printContentType(string type);
    void printContentLength(int size);
    void printConnectionType(bool keep_alive=false);
    void sendFile(map<string, string> headermap, string request_line, bool keep_alive = false, 
                  bool head_cmd = false);

public:
    void sendHeader(int code, int size, string file_type = "text/plain", 
		    bool keep_alive = false);
    string getHeader();
    void start(int server_port);
    void parseHeader(string header);

    Socket *sock;        
};

class Filter {
public:
    string addFooter(string unfiltered);


};

class Log {
public:
    bool openLogFile(string filename);
    bool closeLogFile();
    bool writeLogLine(string ip, string request, int code, int size, string referrer, string agent);

private:
    string makeDate();
    ofstream logfile;
};

extern Http webserver;
