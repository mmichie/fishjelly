#include "cgi.h"
#include <filesystem>
#include <iostream>
#include <sstream>

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
void Cgi::setupEnv(const std::map<std::string, std::string>& headermap) {
    auto auth_it = headermap.find("AUTH_TYPE");
    if (auth_it != headermap.end()) {
        setenv("AUTH_TYPE", auth_it->second.c_str(), 1);
    }

    auto content_len_it = headermap.find("CONTENT_LENGTH");
    if (content_len_it != headermap.end()) {
        setenv("CONTENT_LENGTH", content_len_it->second.c_str(), 1);
    }

    setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);

    auto query_it = headermap.find("QUERY_STRING");
    if (query_it != headermap.end()) {
        setenv("QUERY_STRING", query_it->second.c_str(), 1);
    }

    setenv("REQUEST_METHOD", "GET", 1);
    setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
    setenv("SERVER_SOFTWARE", "SHELOB/3.14", 1);
}

bool Cgi::executeCGI(std::string_view filename, int accept_fd,
                     const std::map<std::string, std::string>& headermap) {
    int pid;
    std::string fullpath;

    pid = fork();

    if (pid < 0) {
        perror("ERROR on fork");
        return false; // Fork failed, returning false.
    }

    /* Child */
    if (pid == 0) {
        setupEnv(headermap);

        dup2(accept_fd, STDOUT_FILENO);

        std::string filename_str(filename.substr(7));
        printf("HTTP/1.1 200 OK\r\n");

        try {
            auto current_path = std::filesystem::current_path();
            fullpath = (current_path / "htdocs" / filename_str).string();
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            exit(1);
        }

        // Added nullptr as sentinel
        execlp(fullpath.c_str(), filename_str.c_str(), nullptr);

        perror("CGI error");
        exit(1); // If exec fails, terminate the child process.
    }

    return true; // Successful execution
}
