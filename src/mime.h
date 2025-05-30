#ifndef SHELOB_MIME_H
#define SHELOB_MIME_H 1

#include <map>
#include <string>
#include <string_view>

#include "global.h"

class Mime {
  public:
    bool readMimeConfig(std::string_view filename);
    std::string getMimeFromExtension(std::string_view filename);

  private:
    std::map<std::string, std::string> mimemap;
};

#endif /* !SHELOB_MIME_H */
