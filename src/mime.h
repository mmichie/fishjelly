#ifndef SHELOB_MIME_H
#define SHELOB_MIME_H 1

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "global.h"

class Mime {
  public:
    bool readMimeConfig(string filename);
    string getMimeFromExtension(string filename);

  private:
    map<string, string> mimemap;
};

#endif /* !SHELOB_MIME_H */
