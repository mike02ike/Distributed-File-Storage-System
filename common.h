#ifndef COMMON_H
#define COMMON_H
#include <string>

const size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4MB
struct Server {
    std::string ip;
    int port;
};

#endif // COMMON_H