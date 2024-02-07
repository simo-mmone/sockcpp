#include <iostream>
#include <string>
#include <random>
#include <sstream>

#include "base64.h"

#ifdef WINDOWS
    #pragma comment(lib, "ws2_32.lib")
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

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

// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-------+-+-------------+-------------------------------+
// |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
// |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
// |N|V|V|V|       |S|             |   (if payload len==126/127)   |
// | |1|2|3|       |K|             |                               |
// +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
// |     Extended payload length continued, if payload len == 127  |
// + - - - - - - - - - - - - - - - +-------------------------------+
// |                               |Masking-key, if MASK set to 1  |
// +-------------------------------+-------------------------------+
// | Masking-key (continued)       |          Payload Data         |
// +-------------------------------- - - - - - - - - - - - - - - - +
// :                     Payload Data continued ...                :
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// |                     Payload Data continued ...                |
// +---------------------------------------------------------------+

std::string composeWebSocketMessage(std::string & message)
{
    // Generate a random mask key
    std::vector<char> maskKey(4);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (int i = 0; i < 4; ++i) {
        maskKey[i] = static_cast<char>(dis(gen));
    }

    // Apply the mask to the payload data
    for (size_t i = 0; i < message.length(); ++i) {
        message[i] = message[i] ^ maskKey[i % 4];
    }

    // Construct the WebSocket frame with the mask
    std::string frame;
    frame.push_back(0x81); // FIN bit set + Opcode for text frame
    if (message.length() <= 125) {
        frame.push_back(static_cast<char>(message.length() | 0x80)); // Mask bit set
    } else if (message.length() <= 65535) {
        frame.push_back(126 | 0x80); // Mask bit set
        frame.push_back((message.length() >> 8) & 0xFF);
        frame.push_back(message.length() & 0xFF);
    } else {
        frame.push_back(127 | 0x80); // Mask bit set
        for (int i = 7; i >= 0; --i) {
            frame.push_back((message.length() >> (8 * i)) & 0xFF);
        }
    }
    frame.append(maskKey.data(), maskKey.size());
    frame.append(message);
    return frame;
}

std::string receiveWebSocketMessage(int clientSocket) {
    char buffer[1024];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead == -1) {
        // std::cerr << "Error receiving response from server\n";
        return "";
    }
    buffer[bytesRead] = '\0';

    // TODO: 
    //  - handle the case when the message is fragmented
    //  - handle the case when the message is masked
    //  - handle the case when the message is longer than 125 bytes

    // fin + rsv1 + rsv2 + rsv3 + opcode
    unsigned char firstByte = buffer[0];
    std::cout << "First byte: " << std::hex << (int)firstByte << std::endl;
    // mask + payload length
    unsigned char secondByte = buffer[1];
    std::cout << "Second byte: " << std::hex << (int)secondByte << std::endl;


    // print the rest of the message
    std::string msg = buffer + 2;
    return msg;
}

int main() {
    int clientSocket;
    struct sockaddr_in serverAddr;

#ifdef WINDOWS
    WSDATA wsaData;
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Error initializing Winsock\n";
        return 1;
    }
    // Create a socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket\n";
        return 1;
    }
#else
    // Create a socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Error creating socket\n";
        return 1;
    }
#endif

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

    // Construct WebSocket frame for sending a message
    std::string message = "{\"type\": \"play\"}";
    
    std::string frame = composeWebSocketMessage(message);
    // Send the WebSocket message frame
    if (send(clientSocket, frame.c_str(), frame.length(), 0) == -1) {
        std::cerr << "Error sending WebSocket message\n";
        return 1;
    }

#ifdef WINDOWS
    // Set the socket to non-blocking mode
    u_long mode = 1;
    if (ioctlsocket(clientSocket, FIONBIO, &mode) == SOCKET_ERROR) {
        std::cerr << "Error setting socket to non-blocking mode\n";
        return 1;
    }
#else
    // Set the socket to non-blocking mode
    int flags = fcntl(clientSocket, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "Error getting socket flags\n";
        return 1;
    }
    if (fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "Error setting socket to non-blocking mode\n";
        return 1;
    }
#endif

    bool received = false;
    while (!received) {
        std::string msg = receiveWebSocketMessage(clientSocket);
        if (!msg.empty()) {
            std::cout << "Received message from server:\n" << msg << std::endl;
            received = true;
        }
    }

#ifdef WINDOWS
    // Close the socket
    closesocket(clientSocket);
    WSACleanup();
#else
    // Close the socket
    close(clientSocket);
#endif

    return 0;
}