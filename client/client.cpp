#include "client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <csignal>

std::atomic<bool> keepRunning(true);

int createClientSocket(){
    //TCP & IPv4
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == -1) {
        perror("Socket creation failed");
    }
    std::cout << "Client socket created." << std::endl;
    return clientSocket;
}

int connectClient(int clientSocket){
    // Define Server Address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    // inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);
    inet_pton(AF_INET, "192.168.0.207", &serverAddress.sin_addr);
    
    int result = connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (result == -1) {
        std::cout << "Failed to connect to server socket." << std::endl;
    } else {
        std::cout << "Client successfully connected to server." << std::endl;

        // sending data

    }
    return result;
}

int sendData(int clientSocket){
    const char* message = "Hello, server!";
    int msgSend = send(clientSocket, message, strlen(message), 0);

    if (msgSend == -1){
        std::cout << "Message failed to send to server." << std::endl;
    } else {
        std::cout << "Message sent to server." << std::endl;
    }
    return msgSend;
}

void closeClient(int clientSocket){
    close(clientSocket);
    std::cout << "Client socket closed." << std::endl;
}

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal received.\n";
    keepRunning = false;
}

int main(){
    std::signal(SIGINT, signalHandler);

    int clientSocket = createClientSocket();
    if (clientSocket == -1){ return -1; }

    int clientConnect = connectClient(clientSocket);
    if (clientConnect == -1){ return -1; }

    while (keepRunning){
        int clientSend = sendData(clientSocket);
        if (clientSend == -1){
            keepRunning = false;
        }
        sleep(55);
    }

    closeClient(clientSocket);

    return 0;
}