#include "Capsule.h"
#include "Sphere.h"
#include <algorithm>

using namespace DirectX;

void Capsule::updateDimensions()
{
	// The base model in capsule.sjg has a radius of 1.0 and a
	// central line segment (the height of the cylindrical part) of 2.0.
	// We scale these base dimensions by the object's scale components.
	_radius = getScale().x * 1.0f;
	_height = getScale().y * 2.0f;
}

DirectX::XMFLOAT3 Capsule::getAxis() const
{
	XMVECTOR localAxis = XMVectorSet(0, 1, 0, 0);  // Model space Y-axis

	// Use the new helper function to get the non-transposed matrix
	XMMATRIX world = getWorldMatrix();

	XMVECTOR worldAxis = XMVector3TransformNormal(localAxis, world);
	worldAxis = XMVector3Normalize(worldAxis);

	XMFLOAT3 result;
	XMStoreFloat3(&result, worldAxis);
	return result;
}

bool Capsule::isColliding(const Collider& other, DirectX::XMFLOAT3& outNormal, float& penetrationDepth) const
{
	// Check if the other collider is a sphere.
	if (const Sphere* sphere = dynamic_cast<const Sphere*>(&other)) {
		// Delegate the collision check to the sphere class.
		bool result = sphere->isCollidingWithCapsule(*this, outNormal, penetrationDepth);

		// The normal returned by the sphere points from the capsule to the sphere.
		// For consistency, we invert it so it points from the sphere towards our capsule.
		if (result)
		{
			outNormal.x = -outNormal.x;
			outNormal.y = -outNormal.y;
			outNormal.z = -outNormal.z;
		}
		return result;
	}

	// For any other type of collider, do nothing.
	return false;
}