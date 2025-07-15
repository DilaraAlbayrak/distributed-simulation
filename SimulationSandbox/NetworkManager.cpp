// NetworkManager.cpp
#include "NetworkManager.h"
#include <iostream>

std::unique_ptr<NetworkManager> NetworkManager::_instance = nullptr;

NetworkManager& NetworkManager::getInstance() {
    if (!_instance) {
        _instance = std::unique_ptr<NetworkManager>(new NetworkManager());
    }
    return *_instance;
}

NetworkManager::~NetworkManager() {
    stopNetworking();
    if (_socket != INVALID_SOCKET) {
        closesocket(_socket);
        WSACleanup();
    }
}

void NetworkManager::setupSocket() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (_socket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket." << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    u_long mode = 1; // Non-blocking mode
    ioctlsocket(_socket, FIONBIO, &mode);

    _selfAddr.sin_family = AF_INET;
    _selfAddr.sin_port = htons(_port);
    _selfAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(_socket, (sockaddr*)&_selfAddr, sizeof(_selfAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
}

void NetworkManager::startNetworking() {
    if (_running) return;

    setupSocket();

    _running = true;
    _networkThread = std::thread([this]() {
        // Affinity: Core 2
        DWORD_PTR mask = 1ULL << 2;
        SetThreadAffinityMask(GetCurrentThread(), mask);

        networkLoop();
        });
}

void NetworkManager::stopNetworking() {
    if (!_running) return;

    _running = false;
    if (_networkThread.joinable()) {
        _networkThread.join();
    }
}

void NetworkManager::networkLoop() {
    char buffer[512];
    sockaddr_in senderAddr;
    int senderLen = sizeof(senderAddr);

    while (_running) {
        int bytesReceived = recvfrom(_socket, buffer, sizeof(buffer), 0, (sockaddr*)&senderAddr, &senderLen);
        if (bytesReceived > 0) {
            std::lock_guard<std::mutex> lock(_recvMutex);
            // Process buffer (Flatbuffers decode will go here later)
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void NetworkManager::sendObjectUpdate(int objectId, const DirectX::XMFLOAT3& position) {
    // Placeholder: Send raw struct as byte stream (to be replaced with Flatbuffers)
    char buffer[16];
    memcpy(buffer, &objectId, sizeof(int));
    memcpy(buffer + 4, &position, sizeof(DirectX::XMFLOAT3));

    _peerAddr.sin_family = AF_INET;
    _peerAddr.sin_port = htons(_port);
    inet_pton(AF_INET, "127.0.0.1", &_peerAddr.sin_addr); // Replace with config-based IP

    sendto(_socket, buffer, sizeof(buffer), 0, (sockaddr*)&_peerAddr, sizeof(_peerAddr));
}
