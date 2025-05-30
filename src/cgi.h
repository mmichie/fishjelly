#ifndef SHELOB_CGI_H
#define SHELOB_CGI_H 1

#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>

#include <map>
#include <string>
#include <string_view>

#include "global.h"

class Cgi {
  public:
    void setupEnv(const std::map<std::string, std::string>& headermap);
    bool executeCGI(std::string_view filename, int accept_fd,
                    const std::map<std::string, std::string>& headermap);
};

#endif /* !SHELOB_CGI_H */
