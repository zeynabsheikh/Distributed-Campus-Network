#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define HEARTBEAT_PORT 6000
#define BROADCAST_PORT 6001
#define TCP_PORT 5000
#define BUFFER_SIZE 1024

std::mutex mtx;

// Hard-coded campus credentials
std::map<std::string, std::string> campusCreds = {
    {"Lahore", "NU-LHR-123"},
    {"Karachi", "NU-KHI-123"},
    {"Islamabad", "NU-ISB-123"},
    {"Peshawar", "NU-PSH-123"},
    {"Multan", "NU-MLT-123"}
};

// Active TCP client connections
std::map<std::string, int> campusSockets;

// Last UDP heartbeat timestamp
std::map<std::string, std::time_t> campusLastSeen;

// Timestamp helper
std::string currentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    return std::string(std::ctime(&now_time));
}

// ===================== TCP CLIENT HANDLER =====================
void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    std::string campusName;

    // 1. AUTHENTICATION
    memset(buffer, 0, BUFFER_SIZE);
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
    if (bytesReceived <= 0) {
        close(clientSocket);
        return;
    }

    std::string credentials(buffer);
    size_t cPos = credentials.find("Campus:");
    size_t pPos = credentials.find(",Pass:");

    if (cPos == std::string::npos || pPos == std::string::npos) {
        send(clientSocket, "AUTH_FAIL", 9, 0);
        close(clientSocket);
        return;
    }

    campusName = credentials.substr(cPos + 7, pPos - (cPos + 7));
    std::string password = credentials.substr(pPos + 6);

    // Validate
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (campusCreds.count(campusName) && campusCreds[campusName] == password) {
            campusSockets[campusName] = clientSocket;
            send(clientSocket, "AUTH_SUCCESS", 12, 0);
            std::cout << "[INFO] " << campusName << " connected at "
                      << currentTime();
        } else {
            send(clientSocket, "AUTH_FAIL", 9, 0);
            close(clientSocket);
            return;
        }
    }

    // 2. MESSAGE ROUTING LOOP
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(clientSocket, buffer, BUFFER_SIZE, 0);

        if (n <= 0) {
            std::lock_guard<std::mutex> lock(mtx);
            std::cout << "[INFO] " << campusName << " disconnected at " 
                      << currentTime();
            campusSockets.erase(campusName);
            close(clientSocket);
            break;
        }

        std::string msg(buffer);
        size_t sep1 = msg.find("|");
        size_t sep2 = msg.find("|", sep1 + 1);

        if (sep1 == std::string::npos || sep2 == std::string::npos) {
            std::cout << "[WARN] Invalid message format from " << campusName << "\n";
            continue;
        }

        std::string targetCampus = msg.substr(0, sep1);
        std::string targetDept   = msg.substr(sep1 + 1, sep2 - sep1 - 1);
        std::string message      = msg.substr(sep2 + 1);

        // Forward message
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (campusSockets.count(targetCampus)) {
                send(campusSockets[targetCampus], msg.c_str(), msg.size(), 0);
                std::cout << "[ROUTED] " << campusName << " -> " << targetCampus
                          << " | DEPT: " << targetDept << " | MSG: " << message << "\n";
            } else {
                std::cout << "[WARN] Target campus " << targetCampus
                          << " not connected.\n";
            }
        }
    }
}

// ===================== UDP HEARTBEAT LISTENER =====================
void udpListener() {
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) {
        std::cerr << "[ERROR] Could not create UDP heartbeat socket.\n";
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HEARTBEAT_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "[ERROR] Failed to bind heartbeat port.\n";
        return;
    }

    std::cout << "[INFO] Listening for UDP heartbeats...\n";

    char buffer[BUFFER_SIZE];
    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recvfrom(udpSock, buffer, BUFFER_SIZE, 0,
                         (sockaddr*)&clientAddr, &addrLen);

        if (n > 0) {
            std::string heartbeat(buffer);
            size_t sep = heartbeat.find("|");

            if (sep != std::string::npos) {
                std::string campus = heartbeat.substr(0, sep);
                std::lock_guard<std::mutex> lock(mtx);
                campusLastSeen[campus] = std::time(nullptr);
            }
        }
    }
}

// ===================== ADMIN CONSOLE =====================
void adminConsole() {
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);

    if (udpSock < 0) {
        std::cerr << "[ERROR] Failed to create admin broadcast socket.\n";
        return;
    }

    // Enable broadcast
    int broadcastEnable = 1;
    setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST,
               &broadcastEnable, sizeof(broadcastEnable));

    sockaddr_in bAddr{};
    bAddr.sin_family = AF_INET;
    bAddr.sin_port = htons(BROADCAST_PORT);
    inet_pton(AF_INET, "255.255.255.255", &bAddr.sin_addr);

    while (true) {
        std::cout << "\n[ADMIN] Enter broadcast or 'status': ";
        std::string input;
        std::getline(std::cin, input);

        if (input == "status") {
            std::lock_guard<std::mutex> lock(mtx);
            std::cout << "\n====== CAMPUS STATUS ======\n";
            for (auto& c : campusLastSeen) {
                std::time_t t = c.second;
                std::cout << c.first << " | Last heartbeat: " << std::ctime(&t);
            }
            continue;
        }

        sendto(udpSock, input.c_str(), input.size(), 0,
               (sockaddr*)&bAddr, sizeof(bAddr));

        std::cout << "[ADMIN] Broadcast sent.\n";
    }
}

// ===================== MAIN =====================
int main() {
    // Start UDP heartbeat listener
    std::thread heartbeatThread(udpListener);
    heartbeatThread.detach();

    // Start admin console thread
    std::thread adminThread(adminConsole);
    adminThread.detach();

    // Create TCP server
    int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSock < 0) {
        std::cerr << "[ERROR] TCP socket creation failed.\n";
        return -1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(TCP_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcpSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "[ERROR] TCP bind failed.\n";
        return -1;
    }

    listen(tcpSock, 5);
    std::cout << "[INFO] CENTRAL SERVER ONLINE. Waiting for connections...\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);

        int clientSocket = accept(tcpSock, (sockaddr*)&clientAddr, &addrLen);
        if (clientSocket >= 0) {
            std::thread clientThread(handleClient, clientSocket);
            clientThread.detach();
        }
    }

    close(tcpSock);
    return 0;
}
