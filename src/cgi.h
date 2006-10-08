#include <stdlib.h>
#include <map>
#include <string>

#include "global.h"

class Cgi {
	void setupEnv(map<string, string> headermap);
	bool executeCGI(string filename, FILE *socket_fp, map<string, string> headermap);
};