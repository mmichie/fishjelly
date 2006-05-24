#if HAVE_CONFIG_H
#include <config.h>
#endif

#define DEBUG 0

#include <iostream>
#include <string>
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


class Message {

public:
    bool isLoggedOn(string clientIP);
    void login(string clientIP);
    void logout();
    void readMessage(string clientIP);
    void sendMessage(string clientIP, string message);
    string checkForMessages();
    string listClients();
};

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
    void sendFile(string filename, bool keep_alive = false, 
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

extern Http webserver;



