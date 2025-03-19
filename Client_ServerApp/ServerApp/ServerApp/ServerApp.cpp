#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define PORT "27015"
#define BUFLEN 8192

#include <iostream>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <map>
#include <string>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

WSADATA wsaData;
SOCKET listeningSocket = INVALID_SOCKET;
struct addrinfo* addressInfo = NULL, * ptr = NULL, hints;

map<int, string> activeClients;
map<int, time_t> lastMessage;
HANDLE mutexHandle;

void logMessage(const string& message) {
    FILE* logFile = fopen("chat_log.txt", "a");
    if (logFile) {
        fprintf(logFile, "%s\n", message.c_str());
        fclose(logFile);
    }
    else {
        printf("Сервер: невозможно открыть файл\n");
    }
}

void logToConsoleAndFile(const string& message) {
    printf("%s\n", message.c_str());
    logMessage(message);
}

bool isUsernameTaken(const string& username) {
    for (const auto& client : activeClients) {
        if (client.second == username) return true;
    }
    return false;
}

void sendToAll(const string& message, int senderSocket = -1) {
    WaitForSingleObject(mutexHandle, INFINITE);
    for (const auto& client : activeClients) {
        if (client.first != senderSocket) {
            send(client.first, message.c_str(), message.size(), 0);
        }
    }
    ReleaseMutex(mutexHandle);
}

DWORD WINAPI ClientSession(LPVOID lpParam) {
    SOCKET clientSocket = (SOCKET)lpParam;
    char buffer[BUFLEN];
    int bytesReceived;
    string clientName;


    while (true) {
        const char* prompt = "Введи имя: ";
        send(clientSocket, prompt, (int)strlen(prompt), 0);
        logToConsoleAndFile("Сервер: Введи имя");

        bytesReceived = recv(clientSocket, buffer, BUFLEN, 0);
        if (bytesReceived <= 0) {
            closesocket(clientSocket);
            return 0;
        }

        clientName = string(buffer, bytesReceived);
        clientName.erase(clientName.find_last_not_of(" \n\r") + 1);

        logToConsoleAndFile("Пользователь: " + clientName);


        WaitForSingleObject(mutexHandle, INFINITE);
        if (!isUsernameTaken(clientName)) {
            activeClients[clientSocket] = clientName;
            ReleaseMutex(mutexHandle);
            break;
        }
        ReleaseMutex(mutexHandle);

        const char* takenMessage = "Это имя уже занято.";
        send(clientSocket, takenMessage, (int)strlen(takenMessage), 0);
        logToConsoleAndFile("Сервер: Пользователь с таким именем уже существует.");
    }

    string joinMessage = "Сервер: " + clientName + " вошел в чат.";
    logToConsoleAndFile(joinMessage);
    sendToAll(joinMessage, clientSocket);

    while (true) {
        bytesReceived = recv(clientSocket, buffer, BUFLEN, 0);
        if (bytesReceived <= 0) break;

        string message(buffer, bytesReceived);
        message.erase(message.find_last_not_of(" \n\r") + 1);

        if (!message.empty() && message[0] == '/') {
            string commandLog = clientName + ": " + message;
            logToConsoleAndFile(commandLog);

            if (message == "/users") {
                string userList = "Пользователи: ";
                WaitForSingleObject(mutexHandle, INFINITE);
                for (const auto& c : activeClients) userList += c.second + ", ";
                ReleaseMutex(mutexHandle);
                if (!activeClients.empty()) userList.erase(userList.end() - 2);

                string response = "Сервер: " + userList;
                logToConsoleAndFile(response);
                send(clientSocket, userList.c_str(), userList.length(), 0);
            }
            else {
                string unknownCommand = "Сервер: Неизвестная команда";
                logToConsoleAndFile(unknownCommand);
                send(clientSocket, "Неизвестная команда.\n", 18, 0);
            }
        }
        else {
            string msg = clientName + ": " + message;
            logToConsoleAndFile(msg);
            sendToAll(msg, clientSocket);
        }
    }

    string leaveMessage = "Сервер: " + clientName + " покинул чат.";
    sendToAll(leaveMessage, clientSocket);
    logToConsoleAndFile(leaveMessage);
    closesocket(clientSocket);


    WaitForSingleObject(mutexHandle, INFINITE);
    activeClients.erase(clientSocket);
    ReleaseMutex(mutexHandle);

    return 0;
}

int main() {
    setlocale(0, "rus");

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Winsock initialization failed\n");
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &addressInfo) != 0) {
        printf("Error resolving address\n");
        WSACleanup();
        return 2;
    }

    listeningSocket = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol);
    if (listeningSocket == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 3;
    }

    if (bind(listeningSocket, addressInfo->ai_addr, (int)addressInfo->ai_addrlen) == SOCKET_ERROR) {
        printf("Bind failed\n");
        closesocket(listeningSocket);
        WSACleanup();
        return 4;
    }

    freeaddrinfo(addressInfo);

    if (listen(listeningSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Listen failed\n");
        closesocket(listeningSocket);
        WSACleanup();
        return 5;
    }

    mutexHandle = CreateMutex(NULL, FALSE, NULL);

    printf("Порт сервера: %s...\n", PORT);

    while (true) {
        SOCKET clientSocket = accept(listeningSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            printf("Accept failed: %d\n", WSAGetLastError());
            continue;
        }

        HANDLE clientThread = CreateThread(NULL, 0, ClientSession, (LPVOID)clientSocket, 0, NULL);
        if (clientThread) CloseHandle(clientThread);
    }

    closesocket(listeningSocket);
    CloseHandle(mutexHandle);
    WSACleanup();
    return 0;
}