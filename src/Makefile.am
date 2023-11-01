bin_PROGRAMS = shelob
shelob_SOURCES = webserver.cc webserver.h socket.cc socket.h http.cc http.h filter.cc filter.h log.cc log.h mime.cc mime.h token.cc token.h cgi.cc cgi.h global.h
shelob_CXXFLAGS = -std=c++17 -O2 -march=native -g -Wall -Wextra -Werror -fstack-protector-strong -DGIT_HASH=\"$(shell git rev-parse HEAD)\"
