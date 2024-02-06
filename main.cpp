#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include <sstream>

#include "base64.h"

const std::string SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 6666;

std::string generate_nonce() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }

    return ss.str();
}

int main() {
    int clientSocket;
    struct sockaddr_in serverAddr;
    
    // Create a socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Error creating socket\n";
        return 1;
    }

    // Set up server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error connecting to server\n";
        return 1;
    }

    std::string nonce = generate_nonce();
    std::string base64_nonce = base64_encode(nonce.c_str(), nonce.length());

    // Send WebSocket handshake directly
    const std::string WS_HANDSHAKE = "GET / HTTP/1.1\r\n" +
                                     std::string("Host: " + SERVER_IP + "\r\n") +
                                     "Upgrade: websocket\r\n" +
                                     "Connection: Upgrade\r\n" +
                                     "Sec-WebSocket-Key: " + base64_nonce + "\r\n" +
                                     "Sec-WebSocket-Version: 13\r\n\r\n";
    if (send(clientSocket, WS_HANDSHAKE.c_str(), WS_HANDSHAKE.length(), 0) == -1) {
        std::cerr << "Error sending WebSocket handshake request\n";
        return 1;
    }

    // Receive and print response from the server
    char buffer[1024];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead == -1) {
        std::cerr << "Error receiving response from server\n";
        return 1;
    }
    buffer[bytesRead] = '\0';
    std::cout << "Received response from server:\n" << buffer << std::endl;

    // Close the socket
    close(clientSocket);

    return 0;
}

