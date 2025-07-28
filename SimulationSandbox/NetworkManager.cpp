#include "NetworkManager.h"
#include "network_messages_generated.h"
#include "globals.h"
#include <windows.h>
#include <string>
#include <sstream>
#include <iostream>

// This class now depends on D3DFramework, but not the other way around.
#include "D3DFramework.h"
// It also needs to know about the Scenario classes to create them.
#include "Scenario1.h"
#include "Scenario2.h"
#include "Scenario3.h"
#include "Scenario4.h"
#include "Scenario5.h"


using namespace NetworkSim;

// Helper function for logging to the Visual Studio Output window.
void DebugLog(const std::wstring& msg) {
    OutputDebugStringW(msg.c_str());
}

std::unique_ptr<NetworkManager> NetworkManager::_instance = nullptr;

NetworkManager& NetworkManager::getInstance() {
    if (!_instance) {
        _instance = std::unique_ptr<NetworkManager>(new NetworkManager());
    }
    return *_instance;
}

NetworkManager::NetworkManager() : _localPeerId(-1), _localPort(0)
{
    _peerIPs = {
        "10.140.9.135",
        "10.140.9.62",
        "127.0.0.1"
    };
}

NetworkManager::~NetworkManager() {
    stopNetworking();
    if (_socket != INVALID_SOCKET) {
        closesocket(_socket);
        WSACleanup();
    }
}

bool NetworkManager::setupSocket() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        DebugLog(L"[Network Error] WSAStartup failed.\n");
        return false;
    }

    _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (_socket == INVALID_SOCKET) {
        DebugLog(L"[Network Error] Socket creation failed.\n");
        return false;
    }

    u_long mode = 1;
    ioctlsocket(_socket, FIONBIO, &mode);

    int broadcastEnable = 1;
    setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastEnable, sizeof(broadcastEnable));

    const int BASE_PORT = 8888;
    for (int i = 0; i < globals::NUM_PEERS; ++i) {
        int candidatePort = BASE_PORT + i;

        sockaddr_in addr_to_try{};
        addr_to_try.sin_family = AF_INET;
        addr_to_try.sin_port = htons(candidatePort);
        addr_to_try.sin_addr.s_addr = INADDR_ANY;

        if (bind(_socket, (sockaddr*)&addr_to_try, sizeof(addr_to_try)) == 0) {
            _localPort = candidatePort;
            _localPeerId = i;
            _selfAddr = addr_to_try;
            _localColour = _localPeerId;

            std::wstringstream wss;
            wss << L"[Network] Bound to port " << _localPort << L". Peer ID: " << _localPeerId << L"\n";
            DebugLog(wss.str());

            return true;
        }

        if (WSAGetLastError() != WSAEADDRINUSE) {
            std::wstringstream wss;
            wss << L"[Network Error] bind() failed with error: " << WSAGetLastError() << L"\n";
            DebugLog(wss.str());
            closesocket(_socket);
            return false;
        }
    }

    DebugLog(L"[Network Error] No available port found.\n");
    closesocket(_socket);
    return false;
}

void NetworkManager::startNetworking() {
    if (_running) return;

    if (!setupSocket()) {
        return;
    }

    _running = true;
    _networkThread = std::thread([this]() {
        SetThreadAffinityMask(GetCurrentThread(), 1ULL << 2);
        networkLoop();
        });

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
    sockaddr_in senderAddr{};
    int senderLen = sizeof(senderAddr);

    while (_running) {
        int bytes = recvfrom(_socket, buffer, sizeof(buffer), 0, (sockaddr*)&senderAddr, &senderLen);
        if (bytes > 0) {
            const Message* msg = GetMessage(buffer);
            if (!msg) continue;

            switch (msg->data_type())
            {
            case MessageData_PeerAnnounce:
                handlePeerAnnounce(buffer, bytes, senderAddr);
                break;
            case MessageData_GlobalState:
                handleGlobalState(buffer, bytes);
                break;
            }
        }

        checkForLocalStateChangesAndBroadcast();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void NetworkManager::broadcastScenarioChange(int scenarioId)
{
    // No need to check against last broadcasted ID here. We always send.
    flatbuffers::FlatBufferBuilder builder;
    auto statePayload = CreateGlobalState(builder, 0.0f, false, false, 0, scenarioId);
    auto msg = CreateMessage(builder, GetTickCount64(), MessageData_GlobalState, statePayload.Union());
    builder.Finish(msg);

    const char* messageData = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int messageSize = builder.GetSize();

    std::lock_guard<std::mutex> lock(_recvMutex);
    for (const auto& [id, peer] : _knownPeers) {
        sendto(_socket, messageData, messageSize, 0, (sockaddr*)&peer.address, sizeof(peer.address));
    }
    // Update our own state tracker after sending
    _lastBroadcastedScenarioId = scenarioId;
}

void NetworkManager::checkForLocalStateChangesAndBroadcast()
{
    // This function can now be used for other state changes in the future,
    // but the scenario change logic is removed from here.
}

void NetworkManager::handleGlobalState(const char* data, int size)
{
    const Message* msg = GetMessage(data);
    const GlobalState* state = msg->data_as_GlobalState();
    if (!state) return;

    int receivedScenarioId = state->scenarioId();

    // The check '!= localScenarioId' is removed.
    // If we receive a command to set a scenario, we do it.
    // This forces synchronization and handles reloads.
    std::lock_guard<std::mutex> lock(_commandMutex);
    _mainThreadCommandQueue.push_back([receivedScenarioId]() {
        auto& render = D3DFramework::getInstance();
        // This prevents reloading the same scenario if we are already on it.
        // But it allows reloading if the command comes from another peer.
        if (receivedScenarioId == render.getCurrentScenarioId()) return;

        switch (receivedScenarioId) {
        case 1: render.setScenario(std::make_unique<Scenario1>(render.getDevice(), render.getDeviceContext()), 1); break;
        case 2: render.setScenario(std::make_unique<Scenario2>(render.getDevice(), render.getDeviceContext()), 2); break;
        case 3: render.setScenario(std::make_unique<Scenario3>(render.getDevice(), render.getDeviceContext()), 3); break;
        case 4: render.setScenario(std::make_unique<Scenario4>(render.getDevice(), render.getDeviceContext()), 4); break;
        case 5: render.setScenario(std::make_unique<Scenario5>(render.getDevice(), render.getDeviceContext()), 5); break;
            // Handle scenario 0 (no scenario) if needed
        case 0: render.setScenario(nullptr, 0); break;
        }
        });
}


void NetworkManager::handlePeerAnnounce(const char* data, int size, const sockaddr_in& senderAddr) {
    const Message* msg = GetMessage(data);
    if (!msg) return;

    const PeerAnnounce* peer = msg->data_as_PeerAnnounce();
    if (!peer) return;

    int id = peer->peerId();
    if (id == _localPeerId) return;

    PeerInfo info;
    info.peerId = id;
    info.address = senderAddr;
    info.address.sin_port = htons(peer->port());

    std::lock_guard<std::mutex> lock(_recvMutex);

    bool isNewPeer = (_knownPeers.find(id) == _knownPeers.end());
    _knownPeers[id] = info;

    if (isNewPeer) {
        std::wstringstream wss;
        wss << L"[Network] Discovered Peer ID: " << id << L" at Port: " << peer->port() << L"\n";
        DebugLog(wss.str());
        sendPeerAnnounce();
    }
}

//void NetworkManager::handleGlobalState(const char* data, int size)
//{
//    const Message* msg = GetMessage(data);
//    const GlobalState* state = msg->data_as_GlobalState();
//    if (!state) return;
//
//    int receivedScenarioId = state->scenarioId();
//    auto& render = D3DFramework::getInstance();
//    int localScenarioId = render.getCurrentScenarioId();
//
//    if (receivedScenarioId > 0 && receivedScenarioId != localScenarioId)
//    {
//        std::lock_guard<std::mutex> lock(_commandMutex);
//        _mainThreadCommandQueue.push_back([receivedScenarioId]() {
//            auto& render = D3DFramework::getInstance();
//            switch (receivedScenarioId) {
//            case 1: render.setScenario(std::make_unique<Scenario1>(render.getDevice(), render.getDeviceContext()), 1); break;
//            case 2: render.setScenario(std::make_unique<Scenario2>(render.getDevice(), render.getDeviceContext()), 2); break;
//            case 3: render.setScenario(std::make_unique<Scenario3>(render.getDevice(), render.getDeviceContext()), 3); break;
//            case 4: render.setScenario(std::make_unique<Scenario4>(render.getDevice(), render.getDeviceContext()), 4); break;
//            case 5: render.setScenario(std::make_unique<Scenario5>(render.getDevice(), render.getDeviceContext()), 5); break;
//            }
//            });
//    }
//}

//void NetworkManager::sendPeerAnnounce() {
//    if (_localPeerId == -1) return;
//
//    // 1. Build the "I exist!" message (PeerAnnounce)
//    flatbuffers::FlatBufferBuilder builder;
//    auto announce = CreatePeerAnnounce(builder, _localPeerId, _localPort);
//    auto msg = CreateMessage(builder, GetTickCount64(), MessageData_PeerAnnounce, announce.Union());
//    builder.Finish(msg);
//
//    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
//    int size = builder.GetSize();
//
//    // 2. Proactively send this message to every known IP and potential port
//    const int BASE_PORT = 8888;
//    for (const auto& ip : _peerIPs)
//    {
//        for (int i = 0; i < globals::NUM_PEERS; ++i)
//        {
//            sockaddr_in targetAddr{};
//            targetAddr.sin_family = AF_INET;
//            targetAddr.sin_port = htons(BASE_PORT + i);
//            inet_pton(AF_INET, ip.c_str(), &targetAddr.sin_addr);
//
//            // Send the message to this specific IP:PORT combination
//            sendto(_socket, data, size, 0, (sockaddr*)&targetAddr, sizeof(targetAddr));
//        }
//    }
//
//    std::wstringstream wss;
//    wss << L"[Network] Sent PeerAnnounce for Peer ID: " << _localPeerId << L" to all configured addresses.\n";
//    DebugLog(wss.str());
//}

void NetworkManager::sendPeerAnnounce() {
    if (_localPeerId == -1) return;

    flatbuffers::FlatBufferBuilder builder;
    auto announce = CreatePeerAnnounce(builder, _localPeerId, _localPort);
    auto msg = CreateMessage(builder, GetTickCount64(), MessageData_PeerAnnounce, announce.Union());
    builder.Finish(msg);

    sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

    const int BASE_PORT = 8888;
    for (int i = 0; i < globals::NUM_PEERS; ++i) {
        broadcastAddr.sin_port = htons(BASE_PORT + i);
        sendto(_socket, reinterpret_cast<const char*>(builder.GetBufferPointer()),
            builder.GetSize(), 0, (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
    }
}

//void NetworkManager::sendPeerAnnounce() {
//    if (_localPeerId == -1) return;
//
//    flatbuffers::FlatBufferBuilder builder;
//    auto announce = CreatePeerAnnounce(builder, _localPeerId, _localPort);
//    auto msg = CreateMessage(builder, GetTickCount64(), MessageData_PeerAnnounce, announce.Union());
//    builder.Finish(msg);
//
//    const char* data = reinterpret_cast<const char*>(builder.GetBufferPointer());
//    int size = builder.GetSize();
//
//    std::vector<std::string> peerIps = {
//        //"127.0.0.1",    
//        //"127.0.0.1"    
//        "10.140.9.135",   
//        "10.140.9.62"    
//    };
//
//    for (int i = 0; i < globals::NUM_PEERS; ++i) {
//        sockaddr_in target{};
//        target.sin_family = AF_INET;
//        target.sin_port = htons(8888 + i); // PORT: BASE_PORT + i
//
//        if (inet_pton(AF_INET, peerIps[i].c_str(), &target.sin_addr) != 1) {
//            std::wstringstream wss;
//            wss << L"[Network Error] Invalid IP format: " << peerIps[i].c_str() << L"\n";
//            DebugLog(wss.str());
//            continue;
//        }
//
//        // Skip sending to self
//        if (target.sin_addr.s_addr == _selfAddr.sin_addr.s_addr && target.sin_port == _selfAddr.sin_port)
//            continue;
//
//        sendto(_socket, data, size, 0, (sockaddr*)&target, sizeof(target));
//    }
//}

void NetworkManager::sendObjectUpdate(int objectId, const DirectX::XMFLOAT3& position) {
    // This will be implemented in the next step.
}

void NetworkManager::processMainThreadCommands()
{
    std::vector<std::function<void()>> commandsToExecute;
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        if (_mainThreadCommandQueue.empty()) {
            return;
        }
        commandsToExecute.swap(_mainThreadCommandQueue);
    }

    for (const auto& command : commandsToExecute) {
        command();
    }
}