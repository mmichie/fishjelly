#ifndef SHELOB_MIME_H
#define SHELOB_MIME_H 1

#include <map>
#include <string>
#include <string_view>

#include "global.h"

class Mime {
  public:
    // Get singleton instance
    static Mime& getInstance();

    // Delete copy constructor and assignment operator
    Mime(const Mime&) = delete;
    Mime& operator=(const Mime&) = delete;

    bool readMimeConfig(std::string_view filename);
    std::string getMimeFromExtension(std::string_view filename);

  private:
    Mime(); // Private constructor for singleton - loads mime.types
    std::map<std::string, std::string> mimemap;
};

#endif /* !SHELOB_MIME_H */
