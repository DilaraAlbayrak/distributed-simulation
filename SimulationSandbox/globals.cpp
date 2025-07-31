#include "globals.h"

namespace globals
{
	extern std::atomic<float> gravityY{ -9.81f };
	std::atomic<int> integrationMethod{ 0 };
	std::atomic<bool> isPaused{ false };            // Start non-paused
	std::atomic<int> gravityEnabled{ 1 };   // 1 = enabled, 0 = disabled, -1 = reversed

	std::atomic<float> elasticity{ -1.0f }; // Default non valid value
	std::atomic<float> dynamicFriction{ -1.0f }; // Default non valid value
	std::atomic<float> staticFriction{ -1.0f }; // Default non valid value

	std::atomic<float> targetSimFrequencyHz{ 125.0f }; // Default simulation frequency
	std::atomic<float> targetNetFrequencyHz{ 20.0f };  // Default network frequency
	std::atomic<float> targetGfxFrequencyHz{ 30.0f };  // Default graphics frequency

	std::atomic<float> actualSimFrequencyHz{ 0.0f };
	std::atomic<float> actualGfxFrequencyHz{ 0.0f };
	std::atomic<float> actualNetFrequencyHz{ 0.0f };
}