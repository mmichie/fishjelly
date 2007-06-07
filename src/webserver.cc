#include "webserver.h"

bool createPidFile(string filename, int pid) 
{
	ofstream pidfile;
    pidfile.open(filename.c_str(), ios::out | ios::trunc);

    if (!pidfile.is_open()) { 
        cerr << "Error: Unable to open PID file (" << filename << ")" << endl;
        return false;
    }

	if (pidfile.is_open()) {
		pidfile << pid << endl;
	}
	
    pidfile.close(); 
    return true;
 }

void controlBreak(int sigNo)
{
    /* Reset signal handler */
    if (signal(SIGINT, controlBreak) == SIG_ERR) {
        printf("Error setting SIGINT exit with error 1!\n");
        exit(1);
    }

	cout << "Exiting program now..." << endl;
	exit(0);
}

void reapChildren(int sigNo)
{
    /* Reset signal handler */
    if (signal(SIGCHLD, reapChildren) == SIG_ERR) {
        printf("Error setting SIGCHLD exit with error 1!\n");
        exit(1);
    }

    /* Make sure we don't end up with zombies */
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

Http webserver;

int main(int argc, char *argv[])
{

	int port;
	
	static const int pid = getpid();
	extern char *optarg;
	extern int  opterr;

    int c;
    static char optstring[] = "hVp:";
    opterr=0;

	if (argc < 2) {
		cerr << "Error: must specify port!" << endl;
		exit(1);
	}

	while ((c=getopt(argc, argv, optstring)) != -1) {
		switch(c) {
			case 'h':
				cout << "Help me Elvis, help me!" << endl;
				return 0;
			break;
			case 'V':
				cout << "Shelob Version foo" << endl;
				return 0;
			break;
			case 'p':
				port = atoi(optarg);
				cout << "Starting on port " << port << " process ID: " << pid << endl;
			break;
			case '?':
				cerr << "Unknown option" << endl;
		}
	}
	
    /* Setup signal handlers */
    if (signal(SIGCHLD, reapChildren) == SIG_ERR) {
        cerr << "Error: Problem setting SIGCHLD, exiting with error 1!" << endl;
        exit(1);
    }

    if (signal(SIGINT, controlBreak) == SIG_ERR) {
        cerr << "Error: Problem setting SIGINT, exiting with error 1!" << endl;
        exit(1);
    }

	//Change the root to htdocs for security
/*	if (chroot("htdocs") == -1) {
		perror("chroot");
		cerr << "Could not change to htdocs" << endl;
	} else {
		cout << "Changed root to htdocs" << endl;
	}
*/
	if (chdir("base") == -1) {
		perror("chdir");
		exit(1);
	}
	
	createPidFile("fishjelly.pid", pid);
    // Start the webserver with port given on commandline
    webserver.start(port);

	return 0;

}
