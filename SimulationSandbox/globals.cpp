#include "globals.h"

namespace globals
{
	DirectX::XMFLOAT3 gravity = { 0.0f, -9.81f, 0.0f };
	std::atomic<int> integrationMethod{ 0 };
}