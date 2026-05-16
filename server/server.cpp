#include "server.h"
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>
#include <fstream>
#include <filesystem>


std::atomic<bool> keepRunning(true);
std::atomic<int> clientCount(0);

struct ClientInfo {
    int socket;
    std::string ip;
    int port;
};

int createServerSocket(){
    //TCP & IPv4
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1) {
        perror("Socket creation failed");
    }
    std::cout << "Server socket created." << std::endl;
    return serverSocket;
}

int bindServerSocket(int serverSocket, int port = 8080){
    // Define Server Address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    

    // Bind Socket
    int bindResult = bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (bindResult == -1) {
        perror("Bind failed");
    } else {
        char serverIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &serverAddress.sin_addr, serverIP, INET_ADDRSTRLEN);
        std::cout << "Server socket bound to all available interfaces on port: " << ntohs(serverAddress.sin_port) << std::endl;
    }

    return bindResult;
}

int listenConnection(int serverSocket){
    int listenResult = listen(serverSocket, 5);

    if (listenResult == -1) {
        std::cout << "Failed to listen on server socket." << std::endl;
        perror("Listen failed");

    } else {
        std::cout << "Server socket listening for connections..." << std::endl;
        // exit(1);   // STOP HERE
    }
    return listenResult;
}

ClientInfo acceptConnection(int serverSocket){
    // setup fd_set
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(serverSocket, &readfds);
    //setup timer
    timeval timeout;
    timeout.tv_sec = 10;   // seconds
    timeout.tv_usec = 0;  // microseconds

    int numFDs = select(serverSocket + 1, &readfds, nullptr, nullptr, &timeout);
    
    // if numFDs > 0
    if (numFDs > 0 && FD_ISSET(serverSocket, &readfds)) {
        sockaddr_in clientAddress;
        socklen_t clientAddressSize = sizeof(clientAddress);

        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressSize);
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, INET_ADDRSTRLEN);

        return ClientInfo{clientSocket, clientIP, ntohs(clientAddress.sin_port)};
    //Fallback timeout
    } else {
        return ClientInfo{-1, "Timeout", -1};
    }
}

bool receivePathLength(int clientSocket, uint32_t& pathLength){
    int bytesRecieved = 0;
    
    while(bytesRecieved < sizeof(uint32_t)){
        int bytes = recv(clientSocket, (char*)&pathLength + bytesRecieved, sizeof(uint32_t) - bytesRecieved, 0);
        if (bytes <= 0) {
            std::cout << "Failed to receive file path length." << std::endl;
            return false;
        }
        bytesRecieved += bytes;
    }
    pathLength = ntohl(pathLength);
    return true;
}

bool receivePath(int clientSocket, uint32_t pathLength, std::string& filePath){
    filePath.assign(pathLength, '\0');
    int bytesRecieved = 0;

    while(bytesRecieved < pathLength){
        int bytes = recv(clientSocket, &filePath[bytesRecieved], pathLength - bytesRecieved, 0);
        if (bytes <= 0) {
            std::cout << "Failed to receive file path." << std::endl;
            return false;
        }
        bytesRecieved += bytes;
    }
    std::cout << "Received file path: " << filePath << std::endl;
    return true;
}

bool receiveFileSize(int clientSocket, uint32_t& fileSize){
    int bytesRecieved = 0;

    while(bytesRecieved < sizeof(uint32_t)){
        int bytes = recv(clientSocket, (char*)&fileSize + bytesRecieved, sizeof(uint32_t) - bytesRecieved, 0);
        if (bytes <= 0) {
            std::cout << "Failed to receive file size." << std::endl;
            return false;
        }
        bytesRecieved += bytes;
    }
    fileSize = ntohl(fileSize);
    std::cout << "Received file size: " << fileSize << " bytes" << std::endl;
    return true;
}

bool recieveFileData(int clientSocket, int clientID, std::string& fileData, uint32_t fileSize){
    fileData.resize(fileSize);
    int bytesRecieved = 0;

    while(bytesRecieved < fileSize){
        int bytes = recv(clientSocket, &fileData[bytesRecieved], fileSize - bytesRecieved, 0);
        if (bytes == -1) {
            std::cout << "Failed to receive file data." << std::endl;
            return false;
        } else if (bytes == 0) {
            std::cerr << "Client " << clientID << " disconnected." << std::endl;
            return false;
        }
        bytesRecieved += bytes;
    }
    std::cout << "Received file data successfully." << std::endl;
    return true;
}

void handleClient(int clientSocket, int clientID){
    uint32_t pathLength;
    std::string filePath;
    uint32_t fileSize;
    std::string fileData;
    std::string fileName;

    if (!receivePathLength(clientSocket, pathLength)) {
        close(clientSocket);
        return;
    }
    if (!receivePath(clientSocket, pathLength, filePath)) {
        close(clientSocket);
        return;
    }
    if (!receiveFileSize(clientSocket, fileSize)) {
        close(clientSocket);
        return;
    }
    if (!recieveFileData(clientSocket, clientID, fileData, fileSize)) {
        close(clientSocket);
        return;
    }

    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile) {
        std::cout << "Failed to open file for writing: " << filePath << std::endl;
        close(clientSocket);
        return;
    }
    outFile.write(fileData.data(), fileSize);
    outFile.close();
    fileName = std::filesystem::path(filePath).filename().string();

    std::cout << "Client " << clientID << " successfully transferred file: " << fileName << std::endl;
    close(clientSocket);
}

void closeServer(int serverSocket){
    // Close Socket
    close(serverSocket);
    std::cout << "Server socket closed." << std::endl;
    return;
}

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal received.\n";
    keepRunning = false;
}

int main(){
    std::signal(SIGINT, signalHandler);
    
    int serverSocket = createServerSocket();
    if (serverSocket == -1){ return -1; }

    int serverBind = bindServerSocket(serverSocket);
    if (serverBind == -1){ return -1; }

    int serverListen = listenConnection(serverSocket);
    if (serverListen == -1){ return -1; }


    //Server loop
    while(keepRunning){
        ClientInfo client = acceptConnection(serverSocket);
        if (client.socket == -1){ continue; }
        clientCount++;

        int clientID = clientCount.load();
        std::cout << "\nClient " << clientID << " successfully connected from: " << client.ip << ":" << client.port << std::endl;

        std::thread(handleClient, client.socket, clientID).detach();
    }


    closeServer(serverSocket);

    return 0;
}
