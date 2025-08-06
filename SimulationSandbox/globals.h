#pragma once
#include <DirectXMath.h>
#include <atomic>

namespace globals
{
    constexpr float AXIS_LENGTH = 3.0f;
    constexpr int NUM_PEERS = 2; // Number of peers in the simulation 
    extern std::atomic<float> gravityY;
    extern std::atomic<int> integrationMethod;
    extern std::atomic<bool> isPaused;            // Start non-paused
    extern std::atomic<int> gravityEnabled;

    extern std::atomic<float> elasticity;
	extern std::atomic<float> dynamicFriction;
	extern std::atomic<float> staticFriction;

    // Variables for Frequency Control
    extern std::atomic<float> targetSimFrequencyHz;
    extern std::atomic<float> targetNetFrequencyHz;

    // Variables for Actual Frequency Display (Local only)
    extern std::atomic<float> targetGfxFrequencyHz;
    extern std::atomic<float> actualSimFrequencyHz;
    extern std::atomic<float> actualGfxFrequencyHz;
    extern std::atomic<float> actualNetFrequencyHz;
}