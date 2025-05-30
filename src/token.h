#ifndef SHELOB_TOKEN_H
#define SHELOB_TOKEN_H 1

#include "global.h"
#include <string>
#include <string_view>
#include <vector>

class Token {
  public:
    void tokenize(std::string_view str, std::vector<std::string>& tokens,
                  std::string_view delimiters);
};

#endif /* !SHELOB_TOKEN_H */
