#include "token.h"

/**
 * Break a string into a vector of tokens.
 * Modernized to use string_view for better performance.
 */
void Token::tokenize(std::string_view str, std::vector<std::string>& tokens,
                     std::string_view delimiters) {
    if (str.empty()) {
        return;
    }
    
    std::string_view::size_type start = 0;
    std::string_view::size_type end = 0;
    
    // Check if delimiter is multi-character
    if (delimiters.length() > 1 && str.find(delimiters) != std::string_view::npos) {
        // Multi-character delimiter handling
        while (start < str.length()) {
            end = str.find(delimiters, start);
            
            if (end == std::string_view::npos) {
                // Last token
                tokens.emplace_back(str.substr(start));
                break;
            } else {
                // Token before delimiter
                tokens.emplace_back(str.substr(start, end - start));
                start = end + delimiters.length();
            }
        }
    } else {
        // Single character delimiter(s) handling
        while (end != std::string_view::npos) {
            end = str.find_first_of(delimiters, start);
            
            if (end == start) {
                // Empty token between consecutive delimiters
                tokens.emplace_back("");
            } else if (end == std::string_view::npos) {
                // Last token
                tokens.emplace_back(str.substr(start));
            } else {
                // Normal token
                tokens.emplace_back(str.substr(start, end - start));
            }
            
            start = end + 1;
        }
    }
}
