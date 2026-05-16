#include "client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <csignal>
#include <fstream>
#include <filesystem>

std::atomic<bool> keepRunning(true);

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
    serverAddress.sin_port = htons(8080);
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

std::string getFilePath(){
    std::string filePath;
    std::cout << "Enter file path:" << std::endl;
    std::getline(std::cin, filePath);

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
    std::cout << "File size: " << fileSize << " bytes" << std::endl;
    return fileSize;
}

int pathLengthSend(int clientSocket, const std::string& filePath){
    uint32_t pathLength = htonl(filePath.size());
    int pathLengthSend = send(clientSocket, &pathLength, sizeof(pathLength), 0);
    if (pathLengthSend == -1){
        std::cout << "Failed to send file path length." << std::endl;
        return -1;
    } else {
        std::cout << "File path length sent successfully." << std::endl;
        return pathLengthSend;
    }
}

int pathSend(int clientSocket, const std::string& filePath){
    int pathSend = send(clientSocket, filePath.c_str(), filePath.size(), 0);
    if (pathSend == -1){
        std::cout << "Failed to send file path." << std::endl;
        return -1;
    } else {
        std::cout << "File path sent successfully." << std::endl;
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
        std::cout << "File size sent successfully." << std::endl;
        return fileSizeSend;
    }
}

int fileDataSend(int clientSocket, std::ifstream& file, std::streamsize fileSize){
    const size_t bufferSize = 4096;
    char buffer[bufferSize];
    std::streamsize totalBytesSent = 0;

    while (totalBytesSent < fileSize) {
        file.read(buffer, bufferSize);
        std::streamsize bytesRead = file.gcount();

        if (bytesRead <= 0) {
            std::cout << "Failed to read from file." << std::endl;
            return -1;
        }
        
        std::streamsize bytesSentSoFar = 0;
        while(bytesSentSoFar < bytesRead) {
            int bytesSent = send(clientSocket, buffer + bytesSentSoFar, bytesRead - bytesSentSoFar, 0);

            if (bytesSent == -1) {
                std::cout << "Failed to send file data." << std::endl;
                return -1;
            }
            bytesSentSoFar += bytesSent;
        }
        totalBytesSent += bytesRead;
    }
    std::cout << "File data sent successfully." << std::endl;
    return totalBytesSent;
}

int main(int argc, char* argv[]){
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <server_ip>" << std::endl;
        return -1;
    }

    int clientSocket = createClient();
    if (clientSocket == -1){ return -1; }

int pathLengthSend(int clientSocket, const std::string& filePath){
    uint32_t pathLength = htonl(filePath.size());
    int pathLengthSend = send(clientSocket, &pathLength, sizeof(pathLength), 0);
    if (pathLengthSend == -1){
        std::cout << "Failed to send file path length." << std::endl;
        return -1;
    } else {
        std::cout << "File path length sent successfully." << std::endl;
        return pathLengthSend;
    }
}

    std::string filePath = getFilePath();
    if(filePath == "") { closeClient(clientSocket); return -1; }

    std::ifstream file(filePath, std::ios::binary);
    std::string fileName = std::filesystem::path(filePath).filename().string();
    std::cout << "File opened successfully: " << fileName << std::endl;

    std::streamsize fileSize = getFileSize(file);
    if(fileSize == -1) { file.close(); closeClient(clientSocket); return -1; }

    int pathLengthSendResult = pathLengthSend(clientSocket, filePath);
    if (pathLengthSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    int pathSendResult = pathSend(clientSocket, filePath);
    if (pathSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    int fileSizeSendResult = fileSizeSend(clientSocket, fileSize);
    if (fileSizeSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    int fileDataSendResult = fileDataSend(clientSocket, file, fileSize);
    if (fileDataSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    file.close();
    closeClient(clientSocket);

    int clientConnect = connectClient(clientSocket, argv[1]);
    if (clientConnect == -1){ return -1; }

    std::string filePath = getFilePath();
    if(filePath == "") { closeClient(clientSocket); return -1; }

    std::ifstream file(filePath, std::ios::binary);
    std::cout << "File opened successfully: " << filePath << std::endl;

    std::streamsize fileSize = getFileSize(file);
    if(fileSize == -1) { file.close(); closeClient(clientSocket); return -1; }

    int pathLengthSendResult = pathLengthSend(clientSocket, filePath);
    if (pathLengthSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    int pathSendResult = pathSend(clientSocket, filePath);
    if (pathSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    int fileSizeSendResult = fileSizeSend(clientSocket, fileSize);
    if (fileSizeSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    int fileDataSendResult = fileDataSend(clientSocket, file, fileSize);
    if (fileDataSendResult == -1){ file.close(); closeClient(clientSocket); return -1; }

    file.close();
    closeClient(clientSocket);
    return 0;
}