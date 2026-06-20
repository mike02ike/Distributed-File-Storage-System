#ifndef COMMON_H
#define COMMON_H
#include <string>

const size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4MB
struct Server {
    std::string ip;
    int port;
};

inline std::string formatBytes(double bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unitIndex = 0;
    while (bytes >= 1024 && unitIndex < 3) {
        bytes /= 1024;
        unitIndex++;
    }
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f %s", bytes, units[unitIndex]);
    return std::string(buffer);
}

#endif // COMMON_H