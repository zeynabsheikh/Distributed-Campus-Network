#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"      // Change if needed
#define HEARTBEAT_PORT 6000        // Send heartbeat to server
#define BROADCAST_PORT 6001        // Receive server broadcasts
#define TCP_PORT 5000              // TCP server port

#define BUFFER_SIZE 1024

std::string campusName;
std::string password;
std::string department;

// ===================== SEND HEARTBEAT =====================
void sendHeartbeat() {
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) {
        std::cerr << "[ERROR] Failed to create UDP socket.\n";
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HEARTBEAT_PORT);    // Correct port
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    while (true) {
        std::string heartbeat = campusName + "|ONLINE";
        sendto(udpSock, heartbeat.c_str(), heartbeat.size(), 0,
               (sockaddr*)&serverAddr, sizeof(serverAddr));

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

// ===================== RECEIVE BROADCASTS =====================
void listenBroadcasts() {
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) {
        std::cerr << "[ERROR] Failed to create UDP broadcast socket.\n";
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROADCAST_PORT);   // Bind to broadcast port
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[ERROR] Failed to bind broadcast port.\n";
        return;
    }

    char buffer[BUFFER_SIZE];
    std::cout << "[INFO] Listening for broadcasts...\n";

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);

        int n = recvfrom(udpSock, buffer, BUFFER_SIZE, 0, nullptr, nullptr);
        if (n > 0) {
            std::cout << "\n[BROADCAST] " << buffer << "\n> ";
        }
    }
}

// ===================== RECEIVE TCP MESSAGES =====================
void listenTCP(int tcpSock) {
    char buffer[BUFFER_SIZE];

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);

        int n = recv(tcpSock, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            std::cout << "\n[INFO] Disconnected from server.\n";
            close(tcpSock);
            exit(0);
        }

        std::cout << "\n[MSG] " << buffer << "\n> ";
    }
}

// ===================== MAIN =====================
int main() {
    std::cout << "Enter Campus Name: ";
    std::getline(std::cin, campusName);
    std::cout << "Enter Password: ";
    std::getline(std::cin, password);
    std::cout << "Enter Department: ";
    std::getline(std::cin, department);

    // -------- CONNECT TCP --------
    int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSock < 0) {
        std::cerr << "[ERROR] Failed to create TCP socket.\n";
        return -1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    if (connect(tcpSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "[ERROR] Could not connect to server.\n";
        return -1;
    }

    // -------- AUTHENTICATION --------
    std::string authMsg = "Campus:" + campusName + ",Pass:" + password;
    send(tcpSock, authMsg.c_str(), authMsg.size(), 0);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int n = recv(tcpSock, buffer, BUFFER_SIZE, 0);

    if (n <= 0 || strncmp(buffer, "AUTH_SUCCESS", 12) != 0) {
        std::cerr << "[ERROR] Authentication failed.\n";
        close(tcpSock);
        return -1;
    }

    std::cout << "[INFO] Authentication Successful.\n";

    // -------- START THREADS --------
    std::thread hb(sendHeartbeat);
    hb.detach();

    std::thread bc(listenBroadcasts);
    bc.detach();

    std::thread tcpListener(listenTCP, tcpSock);
    tcpListener.detach();

    // -------- USER MENU --------
    while (true) {
        std::cout << "\nMenu:\n1. Send Message\n2. Exit\n> ";
        int choice;
        std::cin >> choice;
        std::cin.ignore(); // clear newline

        if (choice == 1) {
            std::string targetCampus, targetDept, message;

            std::cout << "Enter Target Campus: ";
            std::getline(std::cin, targetCampus);
            std::cout << "Enter Target Department: ";
            std::getline(std::cin, targetDept);
            std::cout << "Enter Message: ";
            std::getline(std::cin, message);

            std::string msg = targetCampus + "|" + targetDept + "|" + message;

            send(tcpSock, msg.c_str(), msg.size(), 0);
        }
        else if (choice == 2) {
            std::cout << "Exiting...\n";
            close(tcpSock);
            return 0;
        }
        else {
            std::cout << "[WARN] Invalid choice.\n";
        }
    }

    return 0;
}
 
