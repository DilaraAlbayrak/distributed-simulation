#include "NetworkManager.h"
#include "network_messages_generated.h"
#include <iostream>

using namespace NetworkSim;

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

    // Enable non-blocking mode
    u_long mode = 1;
    ioctlsocket(_socket, FIONBIO, &mode);

    // Enable broadcast capability
    int broadcastEnable = 1;
    if (setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastEnable, sizeof(broadcastEnable)) < 0) {
        std::cerr << "Failed to enable broadcast." << std::endl;
        closesocket(_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

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

    // Optional: Immediately announce self to network
    sendPeerAnnounce();
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

            const Message* message = GetMessage(buffer);
            switch (message->data_type()) {
            case MessageData_ObjectUpdate: {
                const ObjectUpdate* update = message->data_as_ObjectUpdate();
                if (update) {
                    std::cout << "Object " << update->objectId() << " at ("
                        << update->position()->x() << ", "
                        << update->position()->y() << ", "
                        << update->position()->z() << ")\n";
                }
                break;
            }
            case MessageData_PeerAnnounce: {
                const PeerAnnounce* peer = message->data_as_PeerAnnounce();
                if (peer) {
                    std::cout << "Peer Announce: " << peer->name()->c_str()
                        << " (ID=" << peer->peerId() << ", Colour=" << peer->colour() << ", Port=" << peer->port() << ")\n";
                }
                break;
            }
            default:
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void NetworkManager::sendObjectUpdate(int objectId, const DirectX::XMFLOAT3& position) {
    flatbuffers::FlatBufferBuilder builder;

    auto pos = CreateVec3(builder, position.x, position.y, position.z);
    auto vel = CreateVec3(builder, 0.0f, 0.0f, 0.0f);

    int ownerId = 0; // TODO: determine actual peer owner ID
    auto objUpdate = CreateObjectUpdate(builder, objectId, pos, vel, ownerId);
    auto msg = CreateMessage(builder, static_cast<uint64_t>(GetTickCount64()),
        MessageData_ObjectUpdate, objUpdate.Union());
    builder.Finish(msg);

    _peerAddr.sin_family = AF_INET;
    _peerAddr.sin_port = htons(_port);
    inet_pton(AF_INET, "127.0.0.1", &_peerAddr.sin_addr);

    sendto(_socket, reinterpret_cast<const char*>(builder.GetBufferPointer()),
        builder.GetSize(), 0, (sockaddr*)&_peerAddr, sizeof(_peerAddr));
}

void NetworkManager::sendPeerAnnounce() {
    flatbuffers::FlatBufferBuilder builder;

    int peerId = 1; // example ID
    int colour = 0; // example colour code (e.g., red)
    auto name = builder.CreateString("PeerOne");
    int port = _port;

    auto announce = CreatePeerAnnounce(builder, peerId, colour, name, port);
    auto msg = CreateMessage(builder, static_cast<uint64_t>(GetTickCount64()),
        MessageData_PeerAnnounce, announce.Union());
    builder.Finish(msg);

    sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(_port);
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

    sendto(_socket, reinterpret_cast<const char*>(builder.GetBufferPointer()),
        builder.GetSize(), 0, (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
}