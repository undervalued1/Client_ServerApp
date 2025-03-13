#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <string>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

std::vector<SOCKET> clients;
std::map<SOCKET, std::string> clientNames;
std::mutex clientsMutex;

void BroadcastMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (SOCKET client : clients) {
        send(client, message.c_str(), static_cast<int>(message.length()), 0);
    }
}

void LogMessage(const std::string& message) {
    HANDLE hFile = CreateFile(
        "chat_log.txt",
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        SetFilePointer(hFile, 0, NULL, FILE_END); // Перемещаем указатель в конец файла
        DWORD bytesWritten;
        WriteFile(hFile, message.c_str(), static_cast<DWORD>(message.length()), &bytesWritten, NULL);
        CloseHandle(hFile);
    }
}

void HandleClient(SOCKET clientSocket, std::string username) {
    std::vector<char> recvbuf(DEFAULT_BUFLEN);
    int iResult;

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.push_back(clientSocket);
        clientNames[clientSocket] = username;
    }

    std::string joinMessage = "[SERVER]: user \"" + username + "\" has joined\n";
    BroadcastMessage(joinMessage);
    LogMessage(joinMessage);

    while (true) {
        ZeroMemory(recvbuf.data(), recvbuf.size());
        iResult = recv(clientSocket, recvbuf.data(), static_cast<int>(recvbuf.size()), 0);

        if (iResult > 0) {
            std::string message(recvbuf.begin(), recvbuf.begin() + iResult);

            // Удаляем лишние символы перевода строки вручную
            std::string cleanedMessage;
            for (char c : message) {
                if (c != '\n' && c != '\r') {
                    cleanedMessage.push_back(c);
                }
            }

            if (cleanedMessage == "/users") {
                std::string userList = "[SERVER]: Active users: ";
                std::lock_guard<std::mutex> lock(clientsMutex);

                for (const auto& client : clientNames) {
                    userList += client.second + ", ";
                }

                if (clientNames.empty()) userList += "none";
                else userList.pop_back(), userList.pop_back(); // Убираем ", " в конце

                userList += "\n";
                send(clientSocket, userList.c_str(), static_cast<int>(userList.length()), 0);
            }
            else {
                std::string formattedMessage = "[" + username + "]: " + cleanedMessage + "\n";
                BroadcastMessage(formattedMessage);
                LogMessage(formattedMessage);
            }
        }
        else {
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
        clientNames.erase(clientSocket);
    }

    std::string leaveMessage = "[SERVER]: user \"" + username + "\" left the chat\n";
    BroadcastMessage(leaveMessage);
    LogMessage(leaveMessage);

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    struct addrinfo* result = NULL, hints;
    SOCKET ListenSocket = INVALID_SOCKET;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, DEFAULT_PORT, &hints, &result) != 0) {
        printf("getaddrinfo failed\n");
        WSACleanup();
        return 1;
    }

    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    if (bind(ListenSocket, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR) {
        printf("bind failed: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("listen failed: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    printf("Server started. Waiting for connections...\n");

    while (true) {
        SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket != INVALID_SOCKET) {
            char nameBuffer[DEFAULT_BUFLEN];
            int nameLength = recv(ClientSocket, nameBuffer, DEFAULT_BUFLEN, 0);

            if (nameLength > 0) {
                std::string username(nameBuffer, nameLength);
                // Убираем лишние символы перевода строки вручную
                std::string cleanedUsername;
                for (char c : username) {
                    if (c != '\n' && c != '\r') {
                        cleanedUsername.push_back(c);
                    }
                }
                username = cleanedUsername;

                std::thread(HandleClient, ClientSocket, username).detach();
            }
            else {
                closesocket(ClientSocket);
            }
        }
    }

    closesocket(ListenSocket);
    WSACleanup();
    return 0;
}
