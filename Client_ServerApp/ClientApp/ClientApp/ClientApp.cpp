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
    int iResult;

    while (true) {
        iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            std::cout << recvbuf;
        }
        else {
            std::cout << "Соединение закрыто сервером.\n";
            break;
        }
    }
    closesocket(ConnectSocket);
    WSACleanup();
    exit(0);
}

int main() {
    setlocale(LC_ALL, "RU");
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL, * ptr = NULL, hints;

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    getaddrinfo("192.168.0.150", DEFAULT_PORT, &hints, &result);

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            WSACleanup();
            return 1;
        }

        if (connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        std::cerr << "Unable to connect to server.\n";
        WSACleanup();
        return 1;
    }

    std::string username;
    while (true) {
        std::cout << "Введите имя: ";
        std::getline(std::cin, username);
        send(ConnectSocket, username.c_str(), username.length(), 0);

        char response[DEFAULT_BUFLEN] = { 0 };
        int respLength = recv(ConnectSocket, response, DEFAULT_BUFLEN, 0);

        if (respLength > 0) {
            std::string serverResponse(response, respLength);
            std::cout << serverResponse;
            if (serverResponse.find("[SERVER]: Имя уже занято или зарезервировано") != std::string::npos) {
                continue;
            }
        }
        break;
    }

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
