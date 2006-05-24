#include "webserv.h"

string Message::listClients()
{
    DIR *DirPtr;
    struct direct *DirEntryPtr; 
    DirPtr = opendir("clients/");
    string buffer;

    while (1)  {  
        DirEntryPtr = readdir(DirPtr);

        if (DirEntryPtr == 0) 
            break; 

        if (strcmp(DirEntryPtr->d_name,".") != 0 &&
                strcmp(DirEntryPtr->d_name,"..") != 0)  {
            /* print file name */
            buffer.append("<li>");
            buffer.append("<a href=\"sendmessage?to=");
            buffer.append(DirEntryPtr->d_name);
            buffer.append("\">");
            buffer.append("Send a message to ");
            buffer.append(DirEntryPtr->d_name);
            buffer.append("</a>");
            buffer.append("</li>");
        }
    }
    return buffer;
}

bool Message::isLoggedOn(string clientIP)
{
    FILE *login_fp = NULL;
    string filename = "clients/";
    filename.append(clientIP);

    login_fp = fopen(filename.c_str(), "r");

    if (login_fp == NULL) {
        cerr << "could not find" << endl;
        return false;
    }
    else
        return true;
}

void Message::login(string clientIP)
{
    Filter filter;
    FILE *login_fp;
    string filename = "clients/";
    string buffer, filtered;

    //printf("Socket name is IP address %s port %d\n", inet_ntoa(webserver.sock->client.sin_addr), webserver.sock->client.sin_port);
    buffer.append("<html><body><p>You have requested to login!");

    filename.append(inet_ntoa(webserver.sock->client.sin_addr));
    login_fp = fopen(filename.c_str(), "w");
    fclose(login_fp);

    string login_message = "<p>I have logged you in as: ";
    login_message.append(inet_ntoa(webserver.sock->client.sin_addr));
    buffer.append(login_message);
    buffer.append("</body>");	
    buffer.append("</html>");

    filtered = filter.addFooter(buffer);
    webserver.sendHeader(200, 0, "text/html");
    webserver.sock->writeLine(filtered);
}


void Message::logout()
{
    Filter filter;
    string buffer, filtered;

    string filename = "clients/";
    filename.append(inet_ntoa(webserver.sock->client.sin_addr));
    if (unlink(filename.c_str()) != 0) {
        perror("Logout delete");
    }

    buffer.append("<html><body><p>You have been logged out!</p></body></html>");
    filtered = filter.addFooter(buffer);
    webserver.sendHeader(200, 0, "text/html");
    webserver.sock->writeLine(filtered);
}


string Message::checkForMessages()
{
    DIR *DirPtr;
    struct direct *DirEntryPtr; 
    DirPtr = opendir("messages/");
    string buffer;

    while (1)  {  
        DirEntryPtr = readdir(DirPtr);

        if (DirEntryPtr == 0) 
            break; 

        if (strcmp(DirEntryPtr->d_name,".") != 0 &&
                strcmp(DirEntryPtr->d_name,"..") != 0 &&
                strcmp(DirEntryPtr->d_name,inet_ntoa(webserver.sock->client.sin_addr)) == 0)  {
            /* print file name */
            buffer.append("<li>");
            buffer.append("<a href=\"getmessages?client=");
            buffer.append(DirEntryPtr->d_name);
            buffer.append("\">");
            buffer.append(DirEntryPtr->d_name);
            buffer.append("</a>");
            buffer.append("</li>");
        }
    }
    return buffer;
}


void Message::readMessage(string clientIP)
{
    Filter filter;
    char *buffer;
    string filtered, sbuffer, filename;
    unsigned long size;

    filename.append("messages/");
    filename.append(clientIP);
    cout << "CLIENTIP: " << clientIP << endl;
    sbuffer.append("<html><body><p>You have requested to read your messages!");


    ifstream file(filename.c_str(), ios::in|ios::binary);
    if (file.is_open()) {
        cout << "READING FILE" << endl;
        file.seekg (0, ios::end);
        size = file.tellg();

        buffer = new char[size+1];
        if (buffer == NULL) {
            cerr << "Error allocating buffer!" << endl;
        }

        file.seekg(0, ios::beg);
        file.read(buffer, size);

        if (file.gcount() != size) {
            cerr << "Error with read!" << endl;	    
        }

        buffer[size] = 0;
        unlink(filename.c_str());
    }
    cout << "!" << buffer << "!" << endl;
    sbuffer.append("<p>Your message was:<p>");
    sbuffer.append(buffer);
    sbuffer.append("</body></html>");
    filtered = filter.addFooter(sbuffer);
    webserver.sendHeader(200, 0, "text/html");
    webserver.sock->writeLine(filtered);
}


void Message::sendMessage(string clientIP, string message)
{ 
    Filter filter;
    FILE *message_fp;
    string filtered, buffer, filename;


    if (message == "") {
        buffer.append("<html><body>");
        buffer.append("<form ACTION=\"/sendmessage\">");
        buffer.append("<input TYPE=HIDDEN NAME=to VALUE=\"");
        buffer.append(clientIP);
        buffer.append("\">");
        buffer.append("<center>");
        buffer.append("<textarea NAME=message COLS=40 ROWS=6></textarea><br>");
        buffer.append("<input TYPE=SUBMIT VALUE=\"Send Message\">");
        buffer.append("</center>");
        buffer.append("</form>");
        buffer.append("</body></html");
        filtered = filter.addFooter(buffer);
        webserver.sendHeader(200, 0, "text/html");
        webserver.sock->writeLine(filtered);    
    } else {
        buffer.append("<html><body>");
        buffer.append("Message sent!");

        /* Write the message */
        filename.append("messages/");
        filename.append(clientIP);
        message_fp = fopen(filename.c_str(), "a+");
        fprintf(message_fp, "<p>%s sent you: </p><p><b>%s</b></p>\n", inet_ntoa(webserver.sock->client.sin_addr), message.c_str());
        fclose(message_fp);

        buffer.append("</body></html");
        filtered = filter.addFooter(buffer);
        webserver.sendHeader(200, 0, "text/html");
        webserver.sock->writeLine(filtered);    
    }
}
