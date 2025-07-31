#include "NetworkManager.h"
#include "network_messages_generated.h"
#include "globals.h"
#include <windows.h>
#include <string>
#include <sstream>
#include <iostream>
#include <chrono>

// This class now depends on D3DFramework, but not the other way around.
#include "D3DFramework.h"
// It also needs to know about the Scenario classes to create them.
#include "Scenario1.h"
#include "Scenario2.h"
#include "Scenario3.h"
#include "Scenario4.h"
#include "Scenario5.h"
#include "TestScenario1.h"
#include "TestScenario2.h"
#include "TestScenario3.h"
#include "TestScenario4.h"

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
  /*  _peerIPs = {
        "10.140.9.135",
        "10.140.9.62",
        "127.0.0.1"
    };*/
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
            wss << L">>>>>>>>>>>>>>>>[Network] Bound to port " << _localPort << L". Peer ID: " << _localPeerId << L"\n";
            DebugLog(wss.str());

            return true;
        }

        if (WSAGetLastError() != WSAEADDRINUSE) {
            std::wstringstream wss;
            wss << L">>>>>>>>>>>>>>>>[Network Error] bind() failed with error: " << WSAGetLastError() << L"\n";
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

    _netMonitorThread = std::thread([this]() {
        monitorNetworkFrequency();
        });

    sendPeerAnnounce();
}

void NetworkManager::monitorNetworkFrequency() {
    using namespace std::chrono;
    auto lastTime = high_resolution_clock::now();

    while (_running) {
        float targetIntervalMs = 1000.0f; // 1 second
        std::this_thread::sleep_for(duration<float, std::milli>(targetIntervalMs));

        auto now = high_resolution_clock::now();
        float elapsedMs = duration<float, std::milli>(now - lastTime).count();

        int packetCount = _netPacketCounter.exchange(0); // reset
        float actualHz = (elapsedMs > 0.0f) ? (packetCount * 1000.0f / elapsedMs) : 0.0f;

        globals::actualNetFrequencyHz.store(actualHz);
        lastTime = now;
    }
}

void NetworkManager::stopNetworking() {
    if (!_running) return;
    _running = false;
    if (_networkThread.joinable()) {
        _networkThread.join();
    }

	if (_netMonitorThread.joinable()) {
		_netMonitorThread.join();
	}
}

void NetworkManager::networkLoop() {
    char buffer[512];
    sockaddr_in senderAddr{};
    int senderLen = sizeof(senderAddr);

    while (_running) {
        int bytes = recvfrom(_socket, buffer, sizeof(buffer), 0, (sockaddr*)&senderAddr, &senderLen);
        bool messageProcessed = false;

        if (bytes > 0)
        {
            _netPacketCounter.fetch_add(1);

            const Message* msg = GetMessage(buffer);
            if (!msg) continue;

            switch (msg->data_type()) {
            case MessageData_PeerAnnounce:
                handlePeerAnnounce(buffer, bytes, senderAddr);
                break;
            case MessageData_GlobalState:
                handleGlobalState(buffer, bytes);
                break;
            case MessageData_ObjectUpdate:
                handleObjectUpdate(buffer, bytes);
                break;
            case MessageData_ScenarioChange:
                handleScenarioChange(buffer, bytes);
                break;
            }

            messageProcessed = true;
        }

        //// Only update frequency if something was processed
        //if (messageProcessed) {
        //    auto now = std::chrono::high_resolution_clock::now();
        //    float deltaMs = std::chrono::duration<float, std::milli>(now - lastTime).count();
        //    if (deltaMs > 0.0f) {
        //        globals::actualNetFrequencyHz.store(1000.0f / deltaMs);
        //    }
        //    lastTime = now;
        //}

        //// Sleep to match targetNetFrequencyHz — ALWAYS sleep a little to reduce CPU usage
        //float targetHz = globals::targetNetFrequencyHz.load();
        //if (targetHz > 0.0f) {
        //    int intervalMs = static_cast<int>(1000.0f / targetHz);
        //    if (intervalMs > 0)
        //        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        //}
    }
}


void NetworkManager::handleObjectUpdate(const char* data, int size) {
    const Message* msg = GetMessage(data);
    const ObjectUpdate* update = msg->data_as_ObjectUpdate();
    if (!update) return;

    // We received an update for an object owned by another peer.
    // We must not calculate its physics; just apply the state.
    int objectId = update->objectId();
    int ownerId = update->owner();

    // Ignore updates for our own objects to prevent feedback loops.
    if (ownerId == _localPeerId) return;

    DirectX::XMFLOAT3 position = { update->position()->x(), update->position()->y(), update->position()->z() };
	DirectX::XMFLOAT3 rotation = { update->rotation()->x(), update->rotation()->y(), update->rotation()->z() };
    DirectX::XMFLOAT3 velocity = { update->velocity()->x(), update->velocity()->y(), update->velocity()->z() };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f }; 
    if (update->scale()) {
        scale = { update->scale()->x(), update->scale()->y(), update->scale()->z() };
    }

    // Queue this action to be run on the main thread to avoid race conditions.
    std::lock_guard<std::mutex> lock(_commandMutex);
    _mainThreadCommandQueue.push_back([objectId, position, rotation, velocity, scale]() {
        PhysicsManager::getInstance().updateObjectState(objectId, position, rotation, velocity, scale);
        });
}

void NetworkManager::broadcastScenarioChange(int scenarioId)
{
    if (!D3DFramework::getInstance().isScenarioReady())
        return;
    // No need to check against last broadcasted ID here. We always send.
    flatbuffers::FlatBufferBuilder builder;
    auto scenarioPayload = CreateScenarioChange(builder, scenarioId);
    auto msg = CreateMessage(builder, GetTickCount64(), MessageData_ScenarioChange, scenarioPayload.Union());
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

void NetworkManager::handleScenarioChange(const char* data, int size) {
    const Message* msg = GetMessage(data);
    const ScenarioChange* change = msg->data_as_ScenarioChange();
    if (!change) return;

    int scenarioId = change->scenarioId();

    std::lock_guard<std::mutex> lock(_commandMutex);
    _mainThreadCommandQueue.push_back([scenarioId]() {
        auto& render = D3DFramework::getInstance();
        if (scenarioId == render.getCurrentScenarioId()) return;

        switch (scenarioId) {
        case 1: render.setScenario(std::make_unique<Scenario1>(render.getDevice(), render.getDeviceContext()), 1); break;
        case 2: render.setScenario(std::make_unique<Scenario2>(render.getDevice(), render.getDeviceContext()), 2); break;
        case 3: render.setScenario(std::make_unique<Scenario3>(render.getDevice(), render.getDeviceContext()), 3); break;
        case 4: render.setScenario(std::make_unique<Scenario4>(render.getDevice(), render.getDeviceContext()), 4); break;
        case 5: render.setScenario(std::make_unique<Scenario5>(render.getDevice(), render.getDeviceContext()), 5); break;
        case 6: render.setScenario(std::make_unique<TestScenario1>(render.getDevice(), render.getDeviceContext()), 6); break;
		case 7: render.setScenario(std::make_unique<TestScenario2>(render.getDevice(), render.getDeviceContext()), 7); break;
		case 8: render.setScenario(std::make_unique<TestScenario3>(render.getDevice(), render.getDeviceContext()), 8); break;
        case 9: render.setScenario(std::make_unique<TestScenario4>(render.getDevice(), render.getDeviceContext()), 9); break;
        case 0: render.setScenario(nullptr, 0); break;
        }
        });
}

void NetworkManager::broadcastGlobalState() {
    flatbuffers::FlatBufferBuilder builder;

    auto payload = NetworkSim::CreateGlobalState(
        builder,
        globals::isPaused.load(),
        globals::gravityEnabled.load(),
        globals::gravityY.load(),
        globals::elasticity.load(),
        globals::staticFriction.load(),
        globals::dynamicFriction.load(),
        globals::targetSimFrequencyHz.load(),
        globals::targetNetFrequencyHz.load()
    );

    auto msg = NetworkSim::CreateMessage(
        builder,
        GetTickCount64(),
        NetworkSim::MessageData_GlobalState,
        payload.Union()
    );

    builder.Finish(msg);

    const char* messageData = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int messageSize = builder.GetSize();

    std::lock_guard<std::mutex> lock(_recvMutex);
    for (const auto& [id, peer] : _knownPeers) {
        if (id != _localPeerId) {
            sendto(_socket, messageData, messageSize, 0, (sockaddr*)&peer.address, sizeof(peer.address));
        }
    }
}

void NetworkManager::handleGlobalState(const char* data, int size)
{
    const Message* msg = GetMessage(data);
    const GlobalState* state = msg->data_as_GlobalState();
    if (!state) return;

    globals::isPaused.store(state->is_paused());
    globals::gravityEnabled.store(state->gravity_enabled());
    globals::gravityY.store(state->gravity_y());
    globals::elasticity.store(state->elasticity());
    globals::staticFriction.store(state->staticFriction());
    globals::dynamicFriction.store(state->dynamicFriction());
    globals::targetSimFrequencyHz.store(state->target_sim_freq());
    globals::targetNetFrequencyHz.store(state->target_net_freq());
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
        wss << L">>>>>>>>>>>>>>>>[Network] Discovered Peer ID: " << id << L" at Port: " << peer->port() << L"\n";
        DebugLog(wss.str());
        sendPeerAnnounce();
    }
}

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

void NetworkManager::sendObjectUpdate(int objectId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT3& scale) {
    if (_knownPeers.empty()) return;

    flatbuffers::FlatBufferBuilder builder;

    auto posVec = CreateVec3(builder, position.x, position.y, position.z);
	auto rotVec = CreateVec3(builder, rotation.x, rotation.y, rotation.z);
    auto velVec = CreateVec3(builder, velocity.x, velocity.y, velocity.z);
    auto scaleVec = CreateVec3(builder, scale.x, scale.y, scale.z); 

    auto updatePayload = CreateObjectUpdate(builder, objectId, posVec, rotVec, velVec, scaleVec, _localPeerId);

    auto msg = CreateMessage(builder, GetTickCount64(), MessageData_ObjectUpdate, updatePayload.Union());
    builder.Finish(msg);

    const char* messageData = reinterpret_cast<const char*>(builder.GetBufferPointer());
    int messageSize = builder.GetSize();

    // Broadcast to all known peers
    std::lock_guard<std::mutex> lock(_recvMutex);
    for (const auto& [id, peer] : _knownPeers) {
        if (id != _localPeerId) { // Don't send to self
            sendto(_socket, messageData, messageSize, 0, (sockaddr*)&peer.address, sizeof(peer.address));
        }
    }
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