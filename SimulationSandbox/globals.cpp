#include "globals.h"

namespace globals
{
	DirectX::XMFLOAT3 gravity = { 0.0f, -9.81f, 0.0f };
	std::atomic<int> integrationMethod{ 0 };
	std::atomic<bool> isPause{ false };            // Start non-paused
	std::atomic<int> gravityEnabled{ 1 };   // 1 = enabled, 0 = disabled, -1 = reversed
	std::atomic<float> targetDt{ 0.008f }; // 125 hz default
}