#ifndef SHELOB_CGI_H
#define SHELOB_CGI_H 1

#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>

#include <map>
#include <sstream>
#include <string>

#include "global.h"

class Cgi {
  public:
    void setupEnv(map<string, string> headermap);
    bool executeCGI(string filename, int accept_fd,
                    map<string, string> headermap);
};

#endif /* !SHELOB_CGI_H */
