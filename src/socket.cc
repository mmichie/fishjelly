#include "socket.h"

/**
 * Closes the socket.
 */
void Socket::closeSocket()
{
    if (DEBUG)
        cout << "Closing socket" << endl;

    fclose(socket_fp);
}

/**
 * Accepts the client on the socket file descriptor.
 */
void Socket::acceptClient()
{
    bool interrupted;
    sin_size = sizeof(struct sockaddr_in);

    interrupted = false;

    while (!interrupted) {
        accept_fd = accept(socket_fd, (struct sockaddr *) &client, 
                (socklen_t *)&sin_size);
        if (-1 == accept_fd) {
            if (EINTR == errno) {
                continue; /* Restart accept */
            } else {
                perror("Accept");
                interrupted = true;
            }
        } else {
            break;
        }
    }
    /*
       if (accept_fd < 0) {
       perror("Accept");
       exit(1);
       }
     */

    if (DEBUG) {
        fprintf(stdout, "Client accepted from %s... my pid is %d\n", inet_ntoa(client.sin_addr), getpid());
    }

    socket_fp = fdopen(accept_fd, "r"); 
}

/**
 * Writes a string to the current socket.
*/
void Socket::writeLine(string line)
{
    if (send(accept_fd, line.data(), line.size(), 0) == -1) {
        perror("writeLine");
    }	
}

/**
 *  Reads one line from socket.                                               
 *  NOTE: Casting EOF to a char is probably an unsafe operation
 *  TODO: Refactor this code, double check overflow conditions  
 */
bool Socket::readLine(string *buffer)
{
    char c;
    //    string buffer;

    c = fgetc(socket_fp);

    while ( (c != '\n') && (c != (char)EOF) && (c != '\r') ) {
        *buffer += c;
        c = fgetc(socket_fp);
    }

    if (c == '\n')
        *buffer += c;

    //    cout << &buffer << endl;

    if (c == (char)EOF)
        return false;
    else
        return true;
}


/**
 *  Sets up and binds the socket to the correct port.  
 */
void Socket::serverBind(int server_port)
{
    int yes = 1; // for setsockopt() SO_REUSEADDR, below

    // Setup the socket for Internet
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket SFD");
        exit(1);
    }

    // lose the pesky "address already in use" error message
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(server_port); // Bind to port specified
    server.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(socket_fd, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    // Listen to that socket with NUM CLIENTS possible pending connections
    if (listen(socket_fd, NUM_CLIENTS_TO_QUEUE) == -1) {
        perror("listen");
        exit(1);
    }
}

