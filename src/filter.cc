#include "filter.h"
#include <iostream>

/**
 * Takes unfiltered string and appends footer before the end body tag.
 */
std::string Filter::addFooter(std::string_view unfiltered) {
    std::string filtered;

    auto filter_index = unfiltered.find("</body>");

    if (DEBUG) {
        if (filter_index != std::string_view::npos)
            std::cout << "Found </body> at " << filter_index << std::endl;
        else {
            std::cout << "Didn't find </body>" << std::endl;
        }
    }

    if (filter_index != std::string_view::npos) {
        // Copy everything before </body>
        filtered.append(unfiltered.substr(0, filter_index));

        // Add footer content
        filtered.append("<hr><p><h1>The spice is vital to space travel.</h1></p>");
        filtered.append("</ul><a href=\"/index.html\">Return to Main Page</a>");

        // Add the rest of the original content
        filtered.append(unfiltered.substr(filter_index));
    } else {
        // No </body> found, just return the original
        filtered = std::string(unfiltered);
    }

    return filtered;
}
