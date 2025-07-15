#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <memory>
#include <DirectXMath.h>

#pragma comment(lib, "ws2_32.lib")

class NetworkManager {
private:
    static std::unique_ptr<NetworkManager> _instance;

    SOCKET _socket = INVALID_SOCKET;
    std::thread _networkThread;
    std::atomic<bool> _running{ false };
    std::mutex _recvMutex;

    sockaddr_in _selfAddr{};
    sockaddr_in _peerAddr{};
    int _port = 8888;

    NetworkManager() = default;
    void setupSocket();
    void networkLoop();

public:
    ~NetworkManager();

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    NetworkManager(NetworkManager&&) = delete;
    NetworkManager& operator=(NetworkManager&&) = delete;

    static NetworkManager& getInstance();

    void startNetworking();
    void stopNetworking();

    void sendObjectUpdate(int objectId, const DirectX::XMFLOAT3& position);
};
