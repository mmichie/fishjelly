#include <stdlib.h>
#include <map>
#include <string>

#include "global.h"

class Cgi {
	public:
		void setupEnv(map<string, string> headermap);
		bool executeCGI(string filename, int accept_fd, map<string, string> headermap);
};