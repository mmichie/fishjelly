#include "webserv.h"



class Client {



};

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
    /* Check command line */
    if (argc != 2) {
        cerr << "Error: You must specify which port to bind to!" << endl;
        exit(1);
    }

    /* Setup signal handler */
    if (signal(SIGCHLD, reapChildren) == SIG_ERR) {
        cerr << "Error: Problem setting SIGCHLD exiting with error 1!" << endl;
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
	if (chdir("htdocs") == -1) {
		perror("chdir");
	}
	
    // Start the webserver with port given on commandline
    webserver.start(atoi(argv[1]));

}
