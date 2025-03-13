#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

void receiveMessages(SOCKET ConnectSocket) {
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    int iResult;

    while (true) {
        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            std::cout << recvbuf << std::endl;
        }
        else {
            std::cout << "Connection closed by server." << std::endl;
            break;
        }
    }
    closesocket(ConnectSocket);
    WSACleanup();
    exit(0);
}

int main() {
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL, * ptr = NULL, hints;

    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed." << std::endl;
            WSACleanup();
            return 1;
        }

        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        std::cerr << "Unable to connect to server." << std::endl;
        WSACleanup();
        return 1;
    }

    std::cout << "Enter your name: ";
    std::string username;
    std::getline(std::cin, username);
    send(ConnectSocket, username.c_str(), username.length(), 0);

    std::thread recvThread(receiveMessages, ConnectSocket);
    recvThread.detach();

    std::string message;
    while (true) {
        std::getline(std::cin, message);
        if (message == "/exit") {
            send(ConnectSocket, message.c_str(), message.length(), 0);
            break;
        }
        send(ConnectSocket, message.c_str(), message.length(), 0);
    }

    closesocket(ConnectSocket);
    WSACleanup();
    return 0;
}