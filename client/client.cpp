#include "client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <csignal>
#include <fstream>
#include <filesystem>
#include "../common.h"
#include <zlib.h>


std::atomic<bool> keepRunning(true);

// client configuration functions
int createClient(){
    //TCP & IPv4
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == -1) {
        perror("Socket creation failed");
    }
    std::cout << "Client socket created." << std::endl;
    return clientSocket;
}

int connectClient(int clientSocket, const std::string& ip){
    // Define Server Address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr);
    
    int result = connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (result == -1) {
        std::cout << "Failed to connect to server socket." << std::endl;
    } else {
        std::cout << "Client successfully connected to server.\n" << std::endl;
    }
    return result;
}

void closeClient(int clientSocket){
    close(clientSocket);
    std::cout << "Client socket closed." << std::endl;
}

// file handling functions
std::string getFilePath(){
    std::string filePath;
    std::cout << "Enter file path:" << std::endl;
    std::getline(std::cin, filePath);
    std::cout << std::endl;

    if(filePath.empty()) {
        std::cout << "File path cannot be empty." << std::endl;
        return "";
    }else if(filePath.size() > 255) {
        std::cout << "File path is too long." << std::endl;
        return "";
    }else if(!std::filesystem::exists(filePath)) {
        std::cout << "File path does not exist." << std::endl;
        return "";
    }else if(!std::filesystem::is_regular_file(filePath)) {
        std::cout << "File path is not a regular file." << std::endl;
        return "";
    }
    return filePath;
}

std::streamsize getFileSize(std::ifstream& file){
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if(fileSize == -1) {
        std::cout << "Failed to determine file size." << std::endl;
        return -1;
    }
    return fileSize;
}

// header send functions
int pathLengthSend(int clientSocket, const std::string& filePath){
    uint32_t pathLength = htonl(filePath.size());
    int pathLengthSend = send(clientSocket, &pathLength, sizeof(pathLength), 0);
    if (pathLengthSend == -1){
        std::cout << "Failed to send file path length." << std::endl;
        return -1;
    } else {
        return pathLengthSend;
    }
}

int pathSend(int clientSocket, const std::string& filePath){
    int pathSend = send(clientSocket, filePath.c_str(), filePath.size(), 0);
    if (pathSend == -1){
        std::cout << "Failed to send file path." << std::endl;
        return -1;
    } else {
        return pathSend;
    }
}

int fileSizeSend(int clientSocket, std::streamsize fileSize){
    uint32_t fileSize32 = htonl((uint32_t)fileSize);
    int fileSizeSend = send(clientSocket, &fileSize32, sizeof(fileSize32), 0);
    if (fileSizeSend == -1){
        std::cout << "Failed to send file size." << std::endl;
        return -1;
    } else {
        return fileSizeSend;
    }
}

int chunkCountSend(int clientSocket, std::streamsize fileSize){
    uint32_t chunkCount32 = htonl((fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE);
    int chunkCountSend = send(clientSocket, &chunkCount32, sizeof(chunkCount32), 0);
    if (chunkCountSend == -1){
        std::cout << "Failed to send chunk count." << std::endl;
        return -1;
    } else {
        return chunkCountSend;
    }
}

// data send function
int chunkDataSend(int clientSocket, std::ifstream& file, std::streamsize fileSize){
    uint32_t chunkIndex = 0;
    std::vector<char> buffer(CHUNK_SIZE);
    std::streamsize totalBytesSent = 0;
    uint32_t chunkCount = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

    while (totalBytesSent < fileSize) {
        file.read(buffer.data(), CHUNK_SIZE);
        std::streamsize bytesRead = file.gcount();

        if (bytesRead <= 0) {
            std::cout << "Failed to read from file." << std::endl;
            return -1;
        }
        // Calculate the CRC32 checksum of the chunk
        uLong checksum = crc32(0L, (const Bytef*)buffer.data(), bytesRead);

        // Send the chunk index
        uint32_t chunkIndex32 = htonl(chunkIndex);
        int chunkIndexSend = send(clientSocket, &chunkIndex32, sizeof(chunkIndex32), 0);
        if (chunkIndexSend == -1){
            std::cout << "Failed to send chunk index." << std::endl;
            return -1;
        }

        // Send the chunk size
        uint32_t chunkSize = htonl((uint32_t)bytesRead);
        int chunkSizeSend = send(clientSocket, &chunkSize, sizeof(chunkSize), 0);
        if (chunkSizeSend == -1){
            std::cout << "Failed to send chunk size." << std::endl;
            return -1;
        }

        // Send the chunk data
        std::streamsize bytesSentSoFar = 0;
        while(bytesSentSoFar < bytesRead) {
            int bytesSent = send(clientSocket, buffer.data() + bytesSentSoFar, bytesRead - bytesSentSoFar, 0);
            if (bytesSent == -1) {
                std::cout << "Failed to send chunk data." << std::endl;
                return -1;
            }
            bytesSentSoFar += bytesSent;
        }
        totalBytesSent += bytesRead;


        // Send the chunk checksum
        uint32_t checksum32 = htonl((uint32_t)checksum);
        int checksumSend = send(clientSocket, &checksum32, sizeof(checksum32), 0);
        if (checksumSend == -1){
            std::cout << "Sending chunk " << chunkIndex + 1 << "/ " << chunkCount << " [" << totalBytesSent << " bytes]... ✗ (checksum send failed)" << std::endl;
            return -1;
        }
        std::cout << "Sending chunk " << chunkIndex + 1 << "/" << chunkCount << " [" << totalBytesSent << " bytes]... ✓ (checksum sent)" << std::endl;

        // Increment the chunk index
        chunkIndex++;
    }
    if (chunkCount == 1) {
        std::cout << "Transfer complete: " << totalBytesSent << " bytes in " << chunkCount << " chunk\n" << std::endl;
    } else {
        std::cout << "Transfer complete: " << totalBytesSent << " bytes in " << chunkCount << " chunks\n" << std::endl;
    }
    return totalBytesSent;
}

int main(int argc, char* argv[]){
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <server_ip>" << std::endl;
        return -1;
    }
    
    // Configure client socket
    int clientSocket = createClient();
    if (clientSocket == -1){ return -1; }

    int clientConnect = connectClient(clientSocket, argv[1]);
    if (clientConnect == -1){ return -1; }

    // file handling
    std::string filePath = getFilePath();
    if(filePath == "") { closeClient(clientSocket); return -1; }

    std::ifstream file(filePath, std::ios::binary);
    std::string fileName = std::filesystem::path(filePath).filename().string();

    std::streamsize fileSize = getFileSize(file);
    if(fileSize == -1) { file.close(); closeClient(clientSocket); return -1; }

    // header send
    int pathLengthSendResult = pathLengthSend(clientSocket, filePath);
    if (pathLengthSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    int pathSendResult = pathSend(clientSocket, filePath);
    if (pathSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    int fileSizeSendResult = fileSizeSend(clientSocket, fileSize);
    if (fileSizeSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }
    
    int chunkCountSendResult = chunkCountSend(clientSocket, fileSize);
    if (chunkCountSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    int chunkCount = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

    // chunked data send (chunk: index, size, data, CRC32)
    int chunkDataSendResult = chunkDataSend(clientSocket, file, fileSize);
    if (chunkDataSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    file.close();
    closeClient(clientSocket);
    return 0;
}