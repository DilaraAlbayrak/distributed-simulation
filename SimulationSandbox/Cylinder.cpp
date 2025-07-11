#include "Cylinder.h"
#include "Sphere.h"
#include <algorithm>

using namespace DirectX;

DirectX::XMFLOAT3 Cylinder::getAxis() const
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

void Cylinder::updateDimensionTransformation()
{
    _radius = getScale().x;
    _height = _radius;
	_height *= getScale().y; // Scale height by Y-axis
    /*if (getAxis().y > 0.1f)
		_radius *= 0.5f;*/		
}

bool Cylinder::isColliding(const Collider& other, XMFLOAT3& outNormal, float& penetrationDepth) const
{
    if (const Sphere* sphere = dynamic_cast<const Sphere*>(&other)) {
        bool result = sphere->isCollidingWithCylinder(*this, outNormal, penetrationDepth);
        // Invert normal to match this cylinder as 'self'
        outNormal.x = -outNormal.x;
        outNormal.y = -outNormal.y;
        outNormal.z = -outNormal.z;
        return result;
    }
    return false;
}

