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
	
	setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
	
}

bool Cgi::executeCGI(string filename, FILE *socket_fp, map<string, string> headermap) {
    int pid;

    pid = fork();

    if (pid < 0) {
        perror("ERROR on fork");
	}

    /* Child */
    if (pid == 0)  {
    	setupEnv(headermap);
		//dup2(client->_file, STDOUT_FILENO);

		printf("HTTP/1.1 200 OK\r\n");
		//http_headers_print(server_headers, stdout);

		//execlp(app_name, app_name, filename, NULL);
		//perror(program_name);
		exit(1);
	}

}

