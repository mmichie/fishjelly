#ifndef SHELOB_FILTER_H
#define SHELOB_FILTER_H 1

#include <string>
#include <string_view>

#include "global.h"

class Filter {
  public:
    std::string addFooter(std::string_view unfiltered);
};

#endif /* !SHELOB_FILTER_H */
