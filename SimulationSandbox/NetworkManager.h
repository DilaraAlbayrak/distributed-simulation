#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <DirectXMath.h>
#include <functional>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

struct PeerInfo {
    int peerId;
    sockaddr_in address;
};

class NetworkManager {
private:
    static std::unique_ptr<NetworkManager> _instance;

    SOCKET _socket = INVALID_SOCKET;
    std::thread _networkThread;
    std::atomic<bool> _running{ false };
    std::mutex _recvMutex;

    sockaddr_in _selfAddr{};
    int _localPeerId = -1;
    int _localPort = 0;
    int _localColour = 1;

    std::vector<std::string> _peerIPs;

    // Used to track the last state we broadcasted to prevent spam
    int _lastBroadcastedScenarioId = -1;

    std::unordered_map<int, PeerInfo> _knownPeers;

    // Command queue for thread-safe operations on the main thread
    std::vector<std::function<void()>> _mainThreadCommandQueue;
    std::mutex _commandMutex;

    // Private Methods
    NetworkManager();
    bool setupSocket();
    void networkLoop();
    void handlePeerAnnounce(const char* data, int size, const sockaddr_in& senderAddr);
    void handleGlobalState(const char* data, int size);

public:
    ~NetworkManager();

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    static NetworkManager& getInstance();

    void startNetworking();
    void stopNetworking();

    void processMainThreadCommands();
    void sendObjectUpdate(int objectId, const DirectX::XMFLOAT3& position);
    void sendPeerAnnounce();
    void checkForLocalStateChangesAndBroadcast();
    void broadcastScenarioChange(int scenarioId);

    int getLocalPeerId() const { return _localPeerId; }
    int getLocalColour() const { return _localColour; }
    bool isRunning() const { return _running.load(); }
};