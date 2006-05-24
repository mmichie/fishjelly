#include "webserv.h"

void Http::printDate(void)
{
    ostringstream date;
    ostringstream cdate;

    char buf[50];
    time_t ltime;
    struct tm *today;
    ltime = time(NULL);
    today = gmtime( &ltime );
    //Date: Fri, 16 Jul 2004 21:31:04 GMT
    //Date: Fri, 16 Jul 2004 15:37:18 GMT

    strftime( buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", today);

    date << "Date: " << buf << "\r\n";

    sock->writeLine(date.str());
}

void Http::printServer(void)
{
    //sock->writeLine("Server: Shelob - Server for HTTP enviroment and Logging Outgoing Bits (Unix)\n");
    sock->writeLine("Server: SHELOB/0.5 (Unix)\r\n");
}

void Http::printContentType(string type)
{
    ostringstream typebuffer;
    typebuffer << "Content-Type: " << type << "\r\n";
    sock->writeLine(typebuffer.str());
}

void Http::printContentLength(int size)
{ 
    ostringstream clbuffer;
    clbuffer << "Content-Length: " << size << "\r\n";
    if (DEBUG) {
        cout << clbuffer.str() << endl;
    }
    sock->writeLine(clbuffer.str());
}

/* http://www.tldp.org/HOWTO/C++Programming-HOWTO-7.html#ss7.3 */
void Tokenize(const string& str,
        vector<string>& tokens,
        const string& delimiters = " ")
{
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos) {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}


void Http::printConnectionType(bool keep_alive)
{
    if (keep_alive)
        sock->writeLine("Connection: Keep-Alive\n");
    else
        sock->writeLine("Connection: close\n");
}

void Http::start(int server_port)
{	
    int pid;

    sock = new Socket(server_port);

    // Loop to handle clients
    while(1) {
        sock->acceptClient();
        pid = fork();
        if (pid < 0)
            perror("ERROR on fork");

        /* Child */
        if (pid == 0)  {
            //close(sockfd);
            parseHeader(getHeader());
            //sock->closeSocket();
            exit(0);
        }
        /* Parent */
        else 
            close(sock->accept_fd);
    }    
}

// FIXME breakup into multiple functions
void Http::sendFile(string filename, bool keep_alive, bool head_cmd)
{     
    unsigned long size;
    string file_extension;

    // Remove leading '/'
    filename = filename.substr(1,filename.size()).c_str();

    // Default page
    if (filename == "")
        filename = "index.html";

    // Remove any trailing newlines
    std::string::size_type pos = filename.find('\n');
    if (pos != std::string::npos)
        filename.erase(pos);

    // Open file
    ifstream file(filename.c_str(), ios::in|ios::binary);

    // can't find file, 404 it
    if (!file.is_open()) {
        sendHeader(404, 0, "text/html", false);
        sock->writeLine("<html><head><title>404</title></head><body>404 not found</body></html>");
        sock->closeSocket();
        return;
    } else { // Let's throw it down the socket

        // Read file
        file.seekg (0, ios::end);
        size = file.tellg();

        char *buffer;
        buffer = new char[size+1];
        if (buffer == NULL) {
            cerr << "Error allocating buffer!" << endl;
        }

        file.seekg(0, ios::beg);
        file.read(buffer, size);

        if (file.gcount() != size) {
            cerr << "Error with read!" << endl;	    
        }

        // Find the extension FIXME BUG
        file_extension = filename.substr(filename.find_first_of("."), 
                filename.length());

        string filtered;
        if (file_extension == ".shtml" || file_extension == ".shtm") {
            buffer[size] = 0;
            // Add the footer
            string s_buffer(buffer);
            Filter *filter;
            filter = new Filter();

            filtered = filter->addFooter(s_buffer);
            size = filtered.length();
        }

        // Send the header TODO expand MIME configuration
        if (file_extension == ".html" || file_extension == ".htm")
            sendHeader(200, size, "text/html", keep_alive);
        else if (file_extension == ".shtml" || file_extension == ".shtm")
            sendHeader(200, size, "text/html", keep_alive);
        else if (file_extension == ".jpg" || file_extension == ".jpeg")
            sendHeader(200, size, "image/jpeg", keep_alive);
        else if (file_extension == ".tar")
            sendHeader(200, size, "application/x-tar", keep_alive);
        else
            sendHeader(200, size, "text/plain", keep_alive);


        // Something of a hack FIXME if time permits
        //	sock->writeLine(buffer);
        if (!head_cmd) {
            if (file_extension == ".shtml" || file_extension == ".shtm") {
                if (send(sock->accept_fd, filtered.data(), size, 0) == -1) {
                    perror("writeLine");
                    //return (-1);
                }	
            } else {
                if (send(sock->accept_fd, buffer, size, 0) == -1) {
                    perror("writeLine");
                    //return (-1);
                }	
            }
        }

        // cleanup
        file.close();
        //free buffer;

        if (DEBUG) 
            cout << "Done with send..." << endl;
    }
    if (!keep_alive)
        sock->closeSocket();
    else
        parseHeader(getHeader());
}

/* TODO Improve tokenization/parsing */
void Http::parseHeader(string header)
{
    vector<string> tokens, cleanup, cleanup2;

    bool keep_alive = false;

    unsigned int i;
    unsigned int loc, loc2;

    Tokenize(header, tokens, " ");   

    for (i = 0; i < tokens.size(); i++) {
        //tokens[i] = tokens[i].upcase();
        loc = tokens[i].find("keep-alive", 0);
        if (loc != string::npos) {
            keep_alive = true;
            //break;
        }
    }

    // Remove any trailing newlines
    std::string::size_type pos = tokens[1].find('\n');
    if (pos != std::string::npos)
        tokens[1].erase(pos);

    if (tokens.size() > 0) {
        if (tokens[0] == "GET") {
            sendFile(tokens[1], keep_alive, false);
            if (DEBUG) {
                cout << "Get requested of " << tokens[1];
                if (keep_alive)
                    cout << " with keep alive";

                cout << endl;
            }
        } else if (tokens[0] == "HEAD") {
           sendFile(tokens[1], keep_alive, true); 
        }
    }

    //    copy(tokens.begin(), tokens.end(), ostream_iterator<string>(cout, ", "));
}

string Http::getHeader()
{
    string clientBuffer;

    // read until EOF, break when done with header
    while ( (sock->readLine(&clientBuffer)) ) {
        if ( strstr(clientBuffer.c_str(), "\n\n") != NULL)
            break;
    }

    //    cout << clientBuffer << "header ends" << endl;

    return clientBuffer;
}

void Http::sendHeader(int code, int size, string file_type, bool keep_alive) 
{
    switch (code) {
        case 200:
            sock->writeLine("HTTP/1.0 200 OK\r\n");
            break;
        case 404:
            sock->writeLine("HTTP/1.0 404 NOT FOUND\r\n");

            break;
        default:
            cerr << "Wrong HTTP CODE!" << endl;
    }

    printDate();
    printServer();

    if (size != 0)
        printContentLength(size);

    printConnectionType(keep_alive);
    printContentType(file_type);

    sock->writeLine("\n");
}