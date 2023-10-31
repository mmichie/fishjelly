#include "cgi.h"

/* See: http://www.ietf.org/rfc/rfc3875 */
/* meta-variable-name = "AUTH_TYPE" | "CONTENT_LENGTH" |
                           "CONTENT_TYPE" | "GATEWAY_INTERFACE" |
                           "PATH_INFO" | "PATH_TRANSLATED" |
                           "QUERY_STRING" | "REMOTE_ADDR" |
                           "REMOTE_HOST" | "REMOTE_IDENT" |
                           "REMOTE_USER" | "REQUEST_METHOD" |
                           "SCRIPT_NAME" | "SERVER_NAME" |
                           "SERVER_PORT" | "SERVER_PROTOCOL" |
                           "SERVER_SOFTWARE" | scheme |*/
void Cgi::setupEnv(map<string, string> headermap) {

    if (headermap.find("AUTH_TYPE") != headermap.end()) {
        setenv("AUTH_TYPE", headermap["AUTH_TYPE"].c_str(), 1);
    }

    if (headermap.find("CONTENT_LENGTH") != headermap.end()) {
        setenv("CONTENT_LENGTH", headermap["CONTENT_LENGTH"].c_str(), 1);
    }

    setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);

    if (headermap.find("QUERY_STRING") != headermap.end()) {
        setenv("QUERY_STRING", headermap["QUERY_STRING"].c_str(), 1);
    }

    setenv("REQUEST_METHOD", "GET", 1);
    setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
    setenv("SERVER_SOFTWARE", "SHELOB/3.14", 1);
}

bool Cgi::executeCGI(std::string filename, int accept_fd,
                     std::map<std::string, std::string> headermap) {
    int pid;
    char current_path[MAXPATHLEN];
    std::ostringstream buffer;
    std::string fullpath;

    pid = fork();

    if (pid < 0) {
        perror("ERROR on fork");
        return false;  // Fork failed, returning false.
    }

    /* Child */
    if (pid == 0) {
        setupEnv(headermap);

        dup2(accept_fd, STDOUT_FILENO);

        filename = filename.substr(7);
        printf("HTTP/1.1 200 OK\r\n");

        if (getcwd(current_path, MAXPATHLEN) == nullptr) {
            perror("getcwd failed");
            exit(1);  // Terminate the child process
        }

        buffer << current_path << "/htdocs/" << filename;
        fullpath = buffer.str();

        // Added nullptr as sentinel
        execlp(fullpath.c_str(), filename.c_str(), nullptr);

        perror("CGI error");
        exit(1);  // If exec fails, terminate the child process.
    }

    return true;  // Successful execution
}
