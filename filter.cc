#include "webserv.h"

string Filter::addFooter(string unfiltered)
{
    Message message;
    string filtered;
    unsigned int filter_index;

    //cout << unfiltered << endl;
    //    s_toupper(unfiltered);
    filter_index = unfiltered.find("</body>", 0);    

    if (DEBUG) {
        if (filter_index != string::npos)
            cout << "Found </body> at " << filter_index << endl;
        else {
            cout << "Didn't find </body>" << endl;
        } 
    }

    unsigned int i;

    for (i = 0; i < filter_index; i++) {
        filtered.append(1, unfiltered.at(i));
    }

    //cout << filtered << endl;  
    filtered.append("<hr><p><h1>The spice is vital to space travel.</h1></p>");

    /*
       if (!message.isLoggedOn(inet_ntoa(webserver.sock->client.sin_addr))) {
       filtered.append("<p>You can <a href=\"login?client=");
       filtered.append(inet_ntoa(webserver.sock->client.sin_addr));
       filtered.append("\">Log in</a></p>");
       }
       else {
       filtered.append("<p>You can <a href=\"logout\">log out</a></p>");

       filtered.append("List of users:<ul>");
       filtered.append(message.listClients());
       filtered.append("</ul>");

       filtered.append("Messages ready for pickup:<ul>");
       filtered.append(message.checkForMessages());
       }
     */
    filtered.append("</ul><a href=\"/index.html\">Return to Main Page</a>");
    filtered.append("</body></html>");

    //cout << filtered << endl;

    return filtered;
}
