#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <conio.h>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

// Структура для хранения клиента
struct Client {
    SOCKET socket;
    std::string username;
};

std::vector<Client> clients;
std::mutex clientsMutex;

// Функция отправки сообщений всем клиентам
void BroadcastMessage(const std::string& message, SOCKET sender) {
    setlocale(LC_ALL, "RU");
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const Client& client : clients) {
        if (client.socket != sender) {
            send(client.socket, message.c_str(), message.length(), 0);
        }
    }
}

// Функция обработки клиента
void HandleClient(SOCKET ClientSocket) {
    setlocale(LC_ALL, "RU");
    char recvbuf[DEFAULT_BUFLEN];
    std::string username;

    // Получаем имя клиента
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const Client& client : clients) {
            if (client.socket == ClientSocket) {
                username = client.username;
                break;
            }
        }
    }

    std::string welcomeMsg = "[SERVER] " + username + " подключился.";
    BroadcastMessage(welcomeMsg + "\n", ClientSocket);
    std::cout << welcomeMsg << std::endl;

    int iResult;
    while (true) {
        iResult = recv(ClientSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            std::string message = username + ": " + recvbuf;

            if (message.find("/exit") != std::string::npos) {
                break;
            }

            BroadcastMessage(message + "\n", ClientSocket);
            std::cout << message << std::endl;
        }
        else {
            break;
        }
    }

    // Удаляем клиента из списка
    {
        setlocale(LC_ALL, "RU");
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(),
            [ClientSocket](const Client& client) { return client.socket == ClientSocket; }), clients.end());
    }

    std::string leaveMsg = "[SERVER] " + username + " вышел из чата.";
    BroadcastMessage(leaveMsg + "\n", ClientSocket);
    std::cout << leaveMsg << std::endl;

    closesocket(ClientSocket);
}

// Проверка уникальности имени
bool isNameTaken(const std::string& username) {
    setlocale(LC_ALL, "RU");
    std::lock_guard<std::mutex> lock(clientsMutex);
    if (username == "server" || username == "SERVER") return true;
    for (const Client& client : clients) {
        if (client.username == username) return true;
    }
    return false;
}

// Основная функция сервера
int main() {
    setlocale(LC_ALL, "RU");
    WSADATA wsaData;
    SOCKET ListenSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL, hints;

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo("192.168.0.150", DEFAULT_PORT, &hints, &result);

    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);

    listen(ListenSocket, SOMAXCONN);
    std::cout << "[SERVER] Сервер запущен и ожидает клиентов...\n";

    while (true) {
        SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) continue;

        char nameBuffer[DEFAULT_BUFLEN];
        int nameLength;

        // Цикл, чтобы клиент мог попытаться снова ввести имя, если оно занято
        std::string username;
        bool validName = false;
        while (!validName) {
            nameLength = recv(ClientSocket, nameBuffer, DEFAULT_BUFLEN, 0);

            if (nameLength > 0) {
                username = std::string(nameBuffer, nameLength);
                username.erase(username.find_last_not_of("\r\n") + 1); // Убираем лишние символы

                if (isNameTaken(username)) {
                    std::string errorMsg = "[SERVER] Имя занято или зарезервировано.\n";
                    send(ClientSocket, errorMsg.c_str(), errorMsg.length(), 0);
                }
                else {
                    validName = true;
                    // Добавляем клиента в список
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        clients.push_back({ ClientSocket, username });
                    }

                    std::string successMsg = "[SERVER] Добро пожаловать, " + username + "!\n";
                    send(ClientSocket, successMsg.c_str(), successMsg.length(), 0);

                    // Запускаем поток обработки клиента
                    std::thread(HandleClient, ClientSocket).detach();
                }
            }
            else {
                closesocket(ClientSocket);
                break;
            }
        }
    }

    closesocket(ListenSocket);
    WSACleanup();
    return 0;
}

