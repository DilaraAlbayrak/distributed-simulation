#pragma once
#include "Collider.h"

class Capsule : public Collider
{
private:
	float _radius = 1.0f;
	float _height = 2.0f;

	void updateDimensions(); 

public:
	Capsule(
		DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f })
		: Collider(position, rotation, scale)
	{
		updateDimensions();
	}
	~Capsule() = default;

	bool isColliding(const Collider& other, DirectX::XMFLOAT3& outNormal, float& penetrationDepth) const override;

	float getRadius() const { return _radius; }
	float getHeight() const { return _height; }

	DirectX::XMFLOAT3 getAxis() const; // Yön vektörü
};
