#pragma once
#include <DirectXMath.h>
#include <atomic>

namespace globals
{
    constexpr float AXIS_LENGTH = 3.0f;
	constexpr int NUM_PEERS = 4; // Number of peers in the simulation 
    extern DirectX::XMFLOAT3 gravity;  
    extern std::atomic<int> integrationMethod;
	extern std::atomic<bool> isPause;            // Start non-paused
    extern std::atomic<int> gravityEnabled;
    extern std::atomic<float> targetDt;
}