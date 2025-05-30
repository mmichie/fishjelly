#include "token.h"

/**
 * Break a string into a vector of tokens.
 * Modernized to use string_view for better performance.
 */
void Token::tokenize(std::string_view str, std::vector<std::string> &tokens,
                     std::string_view delimiters) {
    // Skip delimiters at beginning.
    std::string_view::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    std::string_view::size_type pos = str.find_first_of(delimiters, lastPos);

    while (std::string_view::npos != pos || std::string_view::npos != lastPos) {
        // Found a token, add it to the vector.
        tokens.emplace_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}
