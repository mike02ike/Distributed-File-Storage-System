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

bool parseServerList(std::vector<Server>& servers){
    std::ifstream serverListFile("serverList.txt");

    if (!serverListFile) {
        std::cout << "Failed to open server list file." << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(serverListFile, line)) {
        if (line.empty()) continue;

        size_t delimiterPos = line.find(':');
        if (delimiterPos == std::string::npos) {
            std::cout << "Invalid server entry: " << line << std::endl;
            continue;
        }

        std::string ip = line.substr(0, delimiterPos);
        try{
            int port = std::stoi(line.substr(delimiterPos + 1));
            servers.push_back({ip, port});
        } catch (const std::exception& e) {
            std::cout << "Invalid port number in server entry: " << line << std::endl;
            continue;
        }
    }
    if (servers.size() == 1) {
        std::cout << "Parsed 1 server from server list." << std::endl;
    } else {
        std::cout << "Parsed " << servers.size() << " server from server list." << std::endl;
    }
    return true;
}

int connectClients(std::vector<int>& clientSockets, std::vector<Server>& servers){
    for (const Server& server : servers) {
        int clientSocket = createClient();
        if (clientSocket == -1) {
            perror("Failed to create client socket");
            return -1;
        }
        clientSockets.push_back(clientSocket);

        // Define Server Address
        sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(server.port);
        inet_pton(AF_INET, server.ip.c_str(), &serverAddress.sin_addr);

        int result = connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
        if (result == -1) {
            perror("Failed to connect to server");
                close(clientSocket);
                return -1;
        } else {
            continue; // Successfully connected to a server, continue to the next one
            std::cout << "Connected to server " << server.ip << " on port " << server.port << "..." << std::endl;
        }
    }
    return 0;
}

void closeClients(std::vector<int>& clientSockets){
    for (int clientSocket : clientSockets) {
        close(clientSocket);
        std::cout << "Client socket closed." << std::endl;
    }
}

// file handling functions
std::string getFilePath(){
    std::string filePath;
    std::cout << "\nEnter file path:" << std::endl;
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
int pathLengthSend(std::vector<int>& clientSockets, const std::string& filePath){
    uint32_t pathLength = htonl(filePath.size());

    for (int clientSocket : clientSockets) {
        int pathLengthSend = send(clientSocket, &pathLength, sizeof(pathLength), 0);
        if (pathLengthSend == -1){
            std::cout << "Failed to send file path length." << std::endl;
            return -1;
        }
    }
    return 0;
}

int pathSend(std::vector<int>& clientSockets, const std::string& filePath){

    for (int clientSocket : clientSockets) {
        int pathSend = send(clientSocket, filePath.c_str(), filePath.size(), 0);
        if (pathSend == -1){
            std::cout << "Failed to send file path." << std::endl;
            return -1;
        }
    }
    return 0;
}

int fileSizeSend(std::vector<int>& clientSockets, std::streamsize fileSize){
    uint32_t fileSize32 = htonl((uint32_t)fileSize);
    
    for (int clientSocket : clientSockets) {
        int fileSizeSend = send(clientSocket, &fileSize32, sizeof(fileSize32), 0);
        if (fileSizeSend == -1){
            std::cout << "Failed to send file size." << std::endl;
            return -1;
        }
    }
    return 0;
}

int chunkCountSend(std::vector<int>& clientSockets, std::streamsize fileSize){
    int serverCount = clientSockets.size();
    uint32_t chunkCount = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if (chunkCount == 0) {
        std::cout << "File size is zero, no chunks to send." << std::endl;
        return -1;
    }
    
    //Round robin distribution
    for (int i = 0; i < serverCount; i++) {
        int chunkForServer = chunkCount / serverCount;
        if (i < chunkCount % serverCount) {
            chunkForServer++;
        }

        uint32_t chunkForServer32 = htonl((uint32_t)chunkForServer);
        int chunkCountSend = send(clientSockets[i], &chunkForServer32, sizeof(chunkForServer32), 0);
        if (chunkCountSend == -1){
            std::cout << "Failed to send chunk count." << std::endl;
            return -1;
        }
    }
    return 0;
}

// data send function
int chunkDataSend(std::vector<int>& clientSockets, std::ifstream& file, std::streamsize fileSize){
    uint32_t chunkIndex = 0;
    std::vector<char> buffer(CHUNK_SIZE);
    std::streamsize totalBytesSent = 0;
    uint32_t chunkCount = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if (chunkCount == 0) {
        std::cout << "File size is zero, no chunks to send." << std::endl;
        return -1;
    }

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
        // Round robin distribution of chunks to servers
        int socketIndex = chunkIndex % clientSockets.size();
        int targetSocket = clientSockets[socketIndex];
        int chunkIndexSend = send(targetSocket, &chunkIndex32, sizeof(chunkIndex32), 0);
        if (chunkIndexSend == -1){
            std::cout << "Failed to send chunk index." << std::endl;
            return -1;
        }

        // Send the chunk size
        uint32_t chunkSize = htonl((uint32_t)bytesRead);
        int chunkSizeSend = send(targetSocket, &chunkSize, sizeof(chunkSize), 0);
        if (chunkSizeSend == -1){
            std::cout << "Failed to send chunk size." << std::endl;
            return -1;
        }

        // Send the chunk data
        std::streamsize bytesSentSoFar = 0;
        while(bytesSentSoFar < bytesRead) {
            int bytesSent = send(targetSocket, buffer.data() + bytesSentSoFar, bytesRead - bytesSentSoFar, 0);
            if (bytesSent == -1) {
                std::cout << "Failed to send chunk data." << std::endl;
                return -1;
            }
            bytesSentSoFar += bytesSent;
        }
        totalBytesSent += bytesRead;


        // Send the chunk checksum
        uint32_t checksum32 = htonl((uint32_t)checksum);
        int checksumSend = send(targetSocket, &checksum32, sizeof(checksum32), 0);
        if (checksumSend == -1){
            std::cout << "Sending chunk " << chunkIndex + 1 << "/ " << chunkCount << " [" << formatBytes(totalBytesSent) << " bytes]... ✗ (checksum send failed)" << std::endl;
            return -1;
        }
        std::cout << "Sending chunk " << chunkIndex + 1 << "/" << chunkCount << " [" << formatBytes(bytesRead) << " bytes]... ✓ (checksum sent)" << std::endl;

        // Increment the chunk index
        chunkIndex++;
    }
    if (chunkCount == 1) {
        std::cout << "Transfer complete: " << formatBytes(totalBytesSent) << " in " << chunkCount << " chunk\n" << std::endl;
    } else {
        std::cout << "Transfer complete: " << formatBytes(totalBytesSent) << " in " << chunkCount << " chunks\n" << std::endl;
    }
    return totalBytesSent;
}

int main(int argc, char* argv[]){
    std::vector<int> clientSockets;
    std::vector<Server> servers;
    bool parseResult = parseServerList(servers);
    if (!parseResult) { return -1; }

    int clientConnect = connectClients(clientSockets, servers);
    if (clientConnect == -1){ return -1; }

    // file handling
    std::string filePath = getFilePath();
    if(filePath == "") { closeClients(clientSockets); return -1; }

    std::ifstream file(filePath, std::ios::binary);
    std::string fileName = std::filesystem::path(filePath).filename().string();

    std::streamsize fileSize = getFileSize(file);
    if(fileSize == -1) { file.close(); closeClients(clientSockets); return -1; }

    // header send
    int pathLengthSendResult = pathLengthSend(clientSockets, filePath);
    if (pathLengthSendResult == -1){ file.close(); closeClients(clientSockets); return -1; }

    int pathSendResult = pathSend(clientSockets, filePath);
    if (pathSendResult == -1){ file.close(); closeClients(clientSockets); return -1; }

    int fileSizeSendResult = fileSizeSend(clientSockets, fileSize);
    if (fileSizeSendResult == -1){ file.close(); closeClients(clientSockets); return -1; }
    
    int chunkCountSendResult = chunkCountSend(clientSockets, fileSize);
    if (chunkCountSendResult == -1){ file.close(); closeClients(clientSockets); return -1; }

    int chunkCount = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

    // chunked data send (chunk: index, size, data, CRC32)
    int chunkDataSendResult = chunkDataSend(clientSockets, file, fileSize);
    if (chunkDataSendResult == -1){ file.close(); closeClients(clientSockets); return -1; }

    file.close();
    closeClients(clientSockets);
    return 0;
}