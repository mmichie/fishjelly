#include "filter.h"

/**
 * Takes unfiltered string and appends footer before the end body tag.
 */
string Filter::addFooter(string unfiltered) {
    string filtered;
    unsigned int filter_index;

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

    filtered.append("<hr><p><h1>The spice is vital to space travel.</h1></p>");

    filtered.append("</ul><a href=\"/index.html\">Return to Main Page</a>");
    filtered.append("</body></html>");

    return filtered;
}
