#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
	//DirectX::XMFLOAT4 color = { 0.5f, 0.5f, 0.5f, 1.0f };
};

class SJGLoader
{
public:
    static bool Load(const std::string& filename, std::vector<Vertex>& vertices, std::vector<int>& indices);
};