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
        fprintf(stderr, "Error: You must specify which port to bind to!\n");
        exit(1);
    }

    /* Setup signal handler */
    if (signal(SIGCHLD, reapChildren) == SIG_ERR) {
        printf("Error setting SIGCHLD exit with error 1!\n");
        exit(1);
    }

    // Start the webserver with port given on commandline
    webserver.start(atoi(argv[1]));
}


