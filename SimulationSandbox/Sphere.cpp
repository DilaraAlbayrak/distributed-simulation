#include <algorithm>
#include <cmath>

#include "Sphere.h"
#include "Plane.h"
#include "Cylinder.h"
#include "Cube.h"
#include "Capsule.h"

using namespace DirectX;

bool Sphere::isColliding(const Collider& other, DirectX::XMFLOAT3& outNormal, float& penetrationDepth) const
{
    // Check if it's a Sphere
    if (const Sphere* sphere = dynamic_cast<const Sphere*>(&other))
        return isCollidingWithSphere(*sphere, outNormal, penetrationDepth);

    // Check if it's a Plane
    if (const Plane* plane = dynamic_cast<const Plane*>(&other))
        return isCollidingWithPlane(*plane, outNormal, penetrationDepth);

    // Check if it's a Cylinder
    if (const Cylinder* cylinder = dynamic_cast<const Cylinder*>(&other))
        return isCollidingWithCylinder(*cylinder, outNormal, penetrationDepth);

    // Check if it's a Cube
    if (const Cube* cube = dynamic_cast<const Cube*>(&other))
        return isCollidingWithCube(*cube, outNormal, penetrationDepth);

	// Check if it's a Capsule
	if (const Capsule* capsule = dynamic_cast<const Capsule*>(&other))
		return isCollidingWithCapsule(*capsule, outNormal, penetrationDepth);

    return false;
}

bool Sphere::isCollidingWithSphere(const Sphere& other, DirectX::XMFLOAT3& outNormal, float& penetrationDepth) const
{
    float dx = getPosition().x - other.getPosition().x;
    float dy = getPosition().y - other.getPosition().y;
    float dz = getPosition().z - other.getPosition().z;

    float distanceSquared = dx * dx + dy * dy + dz * dz;
    float sumRadii = getRadius() + other.getRadius();
    float sumRadiiSquared = sumRadii * sumRadii;

    if (distanceSquared <= sumRadiiSquared + EPSILON)
    {
        float distance = sqrtf(distanceSquared);

        if (distance > 1e-5f)
        {
            outNormal.x = dx / distance;
            outNormal.y = dy / distance;
            outNormal.z = dz / distance;
        }
        else
        {
            outNormal = { 1.0f, 0.0f, 0.0f }; // Arbitrary direction
        }

        penetrationDepth = sumRadii - distance;
        return true;
    }

    outNormal = { 0.0f, 0.0f, 0.0f };
    penetrationDepth = 0.0f;
    return false;
}

bool Sphere::isCollidingWithPlane(const Plane& plane, DirectX::XMFLOAT3& outNormal, float& penetrationDepth) const
{
    XMFLOAT3 planeNormal = plane.getNormal();
    XMFLOAT3 planePoint = plane.getPosition();
    XMFLOAT3 spherePos = getPosition();

    XMVECTOR vPlaneNormal = XMLoadFloat3(&planeNormal);
    XMVECTOR vPlanePoint = XMLoadFloat3(&planePoint);
    XMVECTOR vSpherePos = XMLoadFloat3(&spherePos);

    vPlaneNormal = XMVector3Normalize(vPlaneNormal);

    XMVECTOR planeToSphere = XMVectorSubtract(vSpherePos, vPlanePoint);

    float distance = XMVectorGetX(XMVector3Dot(planeToSphere, vPlaneNormal));

    //float absDistance = fabsf(distance);

    //if (absDistance <= _radius + EPSILON)
    if (distance <= _radius + EPSILON)
    {
        //penetrationDepth = _radius - absDistance;
        penetrationDepth = _radius - distance;

        //if (distance < 0)
        //    vPlaneNormal = XMVectorNegate(vPlaneNormal);

        XMStoreFloat3(&outNormal, vPlaneNormal);
        return true;
    }

	// No collision
	outNormal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    penetrationDepth = 0.0f;
    return false;
}

bool Sphere::isCollidingWithCylinder(const Cylinder& cylinder, DirectX::XMFLOAT3& outNormal, float& penetrationDepth) const
{
    XMVECTOR vSphere = XMLoadFloat3(&getPosition());
    XMVECTOR vCylinder = XMLoadFloat3(&cylinder.getPosition());
    XMFLOAT3 axisDir = cylinder.getAxis();
    XMVECTOR vAxis = XMVector3Normalize(XMLoadFloat3(&axisDir));
    float halfHeight = cylinder.getHeight() * 0.5f;

    XMVECTOR d = XMVectorSubtract(vSphere, vCylinder);
    float projection = XMVectorGetX(XMVector3Dot(d, vAxis));
    float clampedProjection = std::clamp(projection, -halfHeight, halfHeight);

    XMVECTOR closestPoint = XMVectorAdd(vCylinder, XMVectorScale(vAxis, clampedProjection));
    XMVECTOR diff = XMVectorSubtract(vSphere, closestPoint);

    float distSq = XMVectorGetX(XMVector3LengthSq(diff));
    float combinedRadius = getRadius() + cylinder.getRadius();
    float combinedRadiusSq = combinedRadius * combinedRadius;

    if (distSq <= combinedRadiusSq + EPSILON) {
        float dist = sqrtf(distSq);
        if (dist > 1e-5f)
            XMStoreFloat3(&outNormal, XMVectorScale(diff, 1.0f / dist));
        else
            outNormal = { 0.0f, 1.0f, 0.0f };

        penetrationDepth = combinedRadius - dist;
        return true;
    }

    outNormal = { 0, 0, 0 };
    penetrationDepth = 0.0f;
    return false;
}

bool Sphere::isCollidingWithCapsule(const Capsule& capsule, DirectX::XMFLOAT3& outNormal, float& penetrationDepth) const
{
    // --- 1. Get properties of both objects ---
    XMVECTOR sphereCenter = XMLoadFloat3(&this->getPosition());
    float sphereRadius = this->getRadius();

    XMVECTOR capsuleCenter = XMLoadFloat3(&capsule.getPosition());
    XMFLOAT3 axisDirection = capsule.getAxis();
    XMVECTOR capsuleAxis = XMLoadFloat3(&axisDirection);
    float capsuleHalfHeight = capsule.getHeight() * 0.5f;
    float capsuleRadius = capsule.getRadius();

    // --- 2. Calculate the world-space endpoints of the capsule's line segment ---
    XMVECTOR pointA = capsuleCenter - capsuleAxis * capsuleHalfHeight;
    XMVECTOR pointB = capsuleCenter + capsuleAxis * capsuleHalfHeight;

    // --- 3. Find the closest point on the line segment AB to the sphere's center ---
    XMVECTOR segmentVector = pointB - pointA;        // The vector representing the segment.
    XMVECTOR sphereToPointA = sphereCenter - pointA; // Vector from segment start to sphere.

    // Project sphereToPointA onto the segment vector to find the parameter 't'.
    // t = dot(sphereToPointA, segmentVector) / dot(segmentVector, segmentVector)
    float segmentLengthSq = XMVectorGetX(XMVector3LengthSq(segmentVector));
    float t = 0.0f;
    if (segmentLengthSq > EPSILON) {
        t = XMVectorGetX(XMVector3Dot(sphereToPointA, segmentVector)) / segmentLengthSq;
    }

    // Clamp t to the range [0, 1] to ensure the point is on the segment.
    t = std::clamp(t, 0.0f, 1.0f);

    // Calculate the closest point on the segment using the clamped t.
    XMVECTOR closestPointOnSegment = pointA + segmentVector * t;

    // --- 4. Perform a final sphere-to-point distance check ---
    XMVECTOR diff = sphereCenter - closestPointOnSegment;
    float distSq = XMVectorGetX(XMVector3LengthSq(diff));
    float combinedRadius = sphereRadius + capsuleRadius;
    float combinedRadiusSq = combinedRadius * combinedRadius;

    if (distSq <= combinedRadiusSq)
    {
        float dist = sqrtf(distSq);
        penetrationDepth = combinedRadius - dist;

        if (dist > EPSILON) {
            // The normal points from the capsule's closest point towards the sphere's centre.
            XMStoreFloat3(&outNormal, diff / dist);
        }
        else {
            // The sphere's centre is on the capsule's line segment.
            // A good fallback normal is from the capsule's centre to the sphere's centre.
            XMVECTOR centerToCenter = sphereCenter - capsuleCenter;
            if (XMVectorGetX(XMVector3LengthSq(centerToCenter)) > EPSILON) {
                XMStoreFloat3(&outNormal, XMVector3Normalize(centerToCenter));
            }
            else {
                // The objects are at the exact same location. Provide an arbitrary normal.
                outNormal = { 0.0f, 1.0f, 0.0f };
            }
        }
        return true;
    }

    return false;
}

bool Sphere::isCollidingWithCube(const Cube& cube, DirectX::XMFLOAT3& outNormal, float& penetrationDepth) const
{
    DirectX::XMFLOAT3 sphereCenter = getPosition();
    float radius = getRadius();

    DirectX::XMFLOAT3 cubeCenter = cube.getPosition();
    DirectX::XMFLOAT3 cubeScale = cube.getScale();

    // Calculate cube's min and max corners
    float cubeMinX = cubeCenter.x - cubeScale.x * 0.5f;
    float cubeMaxX = cubeCenter.x + cubeScale.x * 0.5f;
    float cubeMinY = cubeCenter.y - cubeScale.y * 0.5f;
    float cubeMaxY = cubeCenter.y + cubeScale.y * 0.5f;
    float cubeMinZ = cubeCenter.z - cubeScale.z * 0.5f;
    float cubeMaxZ = cubeCenter.z + cubeScale.z * 0.5f;

    // Clamp sphere center to the closest point on the cube
    float closestX = std::max(cubeMinX, std::min(sphereCenter.x, cubeMaxX));
    float closestY = std::max(cubeMinY, std::min(sphereCenter.y, cubeMaxY));
    float closestZ = std::max(cubeMinZ, std::min(sphereCenter.z, cubeMaxZ));

    // Compute the vector from closest point to sphere center
    float dx = sphereCenter.x - closestX;
    float dy = sphereCenter.y - closestY;
    float dz = sphereCenter.z - closestZ;
    float distanceSq = dx * dx + dy * dy + dz * dz;

    float radiusSq = radius * radius;

    if (distanceSq <= radiusSq + EPSILON)
    {
        float distance = sqrtf(distanceSq);

        if (distance > 1e-5f)
        {
            outNormal.x = dx / distance;
            outNormal.y = dy / distance;
            outNormal.z = dz / distance;
        }
        else
        {
			outNormal = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f); // arbitrary value
        }

        penetrationDepth = radius - distance;
        return true;
    }

    // No collision
    penetrationDepth = 0.0f;
    outNormal = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f); 
    return false;
}
