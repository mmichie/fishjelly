#include "mime.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

bool Mime::readMimeConfig(std::string_view filename) {
    std::vector<std::string> tokens;

    std::ifstream file{std::string(filename)};
    if (!file.is_open()) {
        std::cerr << "Error: can't open " << filename << std::endl;
        return false;
    }

    std::string tmp_line;
    while (std::getline(file, tmp_line)) {
        tokens.clear();

        // if not a comment, process
        if (tmp_line[0] != '#') {

            // tokenize the line
            std::string buf;
            std::stringstream ss(tmp_line);
            while (ss >> buf)
                tokens.push_back(buf);

            for (std::size_t j = tokens.size(); j > 1; --j) {
                this->mimemap[tokens[j - 1]] = tokens[0];
            }
        }
    }

    file.close();
    return true; // Indicate successful execution
}

std::string Mime::getMimeFromExtension(std::string_view filename) {
    // Find the extension (assumes the extension is whatever follows the last '.')
    auto dot_pos = filename.rfind('.');
    if (dot_pos == std::string_view::npos) {
        return "text/plain";
    }
    std::string file_extension(filename.substr(dot_pos + 1));

    if (this->mimemap.find(file_extension) == this->mimemap.end())
        return "text/plain";
    else
        return this->mimemap[file_extension];
}

