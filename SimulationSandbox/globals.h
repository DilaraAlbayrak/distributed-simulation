#pragma once
#include <DirectXMath.h>
#include <atomic>

namespace globals
{
    constexpr float AXIS_LENGTH = 3.0f;
    extern DirectX::XMFLOAT3 gravity;  
    extern std::atomic<int> integrationMethod;
}