#include "PhysicsObject.h"
#include "Sphere.h"
#include <debugapi.h>
#include <algorithm>
#include <sysinfoapi.h>

using namespace DirectX;

void PhysicsObject::moveSemiImplicitEuler(float dt)
{
    velocity.x += acceleration.x * dt;
    velocity.y += acceleration.y * dt;
    velocity.z += acceleration.z * dt;

    DirectX::XMFLOAT3 deltaPos = { velocity.x * dt, velocity.y * dt, velocity.z * dt };
    _collider->incrementPosition(deltaPos);
}

void PhysicsObject::moveRK4(float dt)
{
    DirectX::XMFLOAT3 k1_v = velocity;
    DirectX::XMFLOAT3 k1_a = acceleration;

    DirectX::XMFLOAT3 mid_v = {
        velocity.x + 0.5f * dt * k1_a.x,
        velocity.y + 0.5f * dt * k1_a.y,
        velocity.z + 0.5f * dt * k1_a.z
    };
    DirectX::XMFLOAT3 k2_v = mid_v;
    DirectX::XMFLOAT3 k2_a = k1_a;

    mid_v = {
        velocity.x + 0.5f * dt * k2_a.x,
        velocity.y + 0.5f * dt * k2_a.y,
        velocity.z + 0.5f * dt * k2_a.z
    };
    DirectX::XMFLOAT3 k3_v = mid_v;
    DirectX::XMFLOAT3 k3_a = k2_a;

    DirectX::XMFLOAT3 end_v = {
        velocity.x + dt * k3_a.x,
        velocity.y + dt * k3_a.y,
        velocity.z + dt * k3_a.z
    };
    DirectX::XMFLOAT3 k4_v = end_v;
    DirectX::XMFLOAT3 k4_a = k3_a;

    velocity.x += (dt / 6.0f) * (k1_a.x + 2.0f * k2_a.x + 2.0f * k3_a.x + k4_a.x);
    velocity.y += (dt / 6.0f) * (k1_a.y + 2.0f * k2_a.y + 2.0f * k3_a.y + k4_a.y);
    velocity.z += (dt / 6.0f) * (k1_a.z + 2.0f * k2_a.z + 2.0f * k3_a.z + k4_a.z);

    DirectX::XMFLOAT3 deltaPos = { (dt / 6.0f) * (k1_v.x + 2.0f * k2_v.x + 2.0f * k3_v.x + k4_v.x),
                                   (dt / 6.0f) * (k1_v.y + 2.0f * k2_v.y + 2.0f * k3_v.y + k4_v.y),
                                   (dt / 6.0f) * (k1_v.z + 2.0f * k2_v.z + 2.0f * k3_v.z + k4_v.z) };

    _collider->incrementPosition(deltaPos);
}

void PhysicsObject::moveMidpoint(float dt)
{
    DirectX::XMFLOAT3 midVelocity = {
        velocity.x + 0.5f * acceleration.x * dt,
        velocity.y + 0.5f * acceleration.y * dt,
        velocity.z + 0.5f * acceleration.z * dt
    };

    DirectX::XMFLOAT3 deltaPos = {
        midVelocity.x * dt,
        midVelocity.y * dt,
        midVelocity.z * dt
    };
    _collider->incrementPosition(deltaPos);

    velocity.x += acceleration.x * dt;
    velocity.y += acceleration.y * dt;
    velocity.z += acceleration.z * dt;
}


PhysicsObject::PhysicsObject(std::unique_ptr<Collider> col, bool fixed, float objMass, Material mat)
    : _collider(std::move(col)), isFixed(fixed), mass(objMass), material(mat)
{
	setMass(objMass);
    inverseMass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;

    if (_collider)
    {
        previousPosition = _collider->getPosition();
        _constantBuffer.World = _collider->updateWorldMatrix();
    }
    else
    {
        OutputDebugString(L"[ERROR] PhysicsObject created with nullptr collider\n");
        previousPosition = { 0.0f, 0.0f, 0.0f };
        _constantBuffer.World = DirectX::XMMatrixIdentity();
    }

    // integrationMethod = IntegrationMethod::SEMI_IMPLICIT_EULER;
}

void PhysicsObject::setMass(float newMass)
{
    mass = newMass;
    inverseMass = (mass > 1e-6f) ? 1.0f / mass : 0.0f;

    if (!isFixed && _collider)
    {
        if (auto sphere = dynamic_cast<Sphere*>(_collider.get()))
        {
            float radius = sphere->getRadius();
            if (mass > 1e-6f && radius > 1e-6f)
            {
                // for sphere
                float momentOfInertia = (2.0f / 5.0f) * mass * radius * radius;
                inverseMomentOfInertia = 1.0f / momentOfInertia;
            }
            else
            {
                inverseMomentOfInertia = 0.0f;
            }
        }
        else { inverseMomentOfInertia = 0.0f; } 
    }
    else { inverseMomentOfInertia = 0.0f; } 
}

DirectX::XMFLOAT3 PhysicsObject::getSmoothedPosition(float renderTime) const
{
    if (globals::isPaused.load())
    {
        return currentRenderingPosition;
    }

    if (currentTimestamp == 0.0f)
    {
        // Not yet initialised — avoid glitch
        return currentRenderingPosition;
    }

    float delta = currentTimestamp - previousTimestamp;
    if (delta <= 0.0f)
        return currentRenderingPosition;

    float t = (renderTime - previousTimestamp) / delta;

    if (t < 0.0f)
    {
        return previousRenderingPosition;
    }
    else if (t <= 1.0f)
    {
        DirectX::XMFLOAT3 result;
        result.x = previousRenderingPosition.x + t * (currentRenderingPosition.x - previousRenderingPosition.x);
        result.y = previousRenderingPosition.y + t * (currentRenderingPosition.y - previousRenderingPosition.y);
        result.z = previousRenderingPosition.z + t * (currentRenderingPosition.z - previousRenderingPosition.z);
        return result;
    }
    else
    {
        DirectX::XMFLOAT3 velocity;
        velocity.x = (currentRenderingPosition.x - previousRenderingPosition.x) / delta;
        velocity.y = (currentRenderingPosition.y - previousRenderingPosition.y) / delta;
        velocity.z = (currentRenderingPosition.z - previousRenderingPosition.z) / delta;

        float extraTime = renderTime - currentTimestamp;
        DirectX::XMFLOAT3 result;
        result.x = currentRenderingPosition.x + velocity.x * extraTime;
        result.y = currentRenderingPosition.y + velocity.y * extraTime;
        result.z = currentRenderingPosition.z + velocity.z * extraTime;
        return result;
    }
}

void PhysicsObject::setNetworkState(const DirectX::XMFLOAT3& newPosition, const DirectX::XMFLOAT3& newRotation,
    const DirectX::XMFLOAT3& newVelocity,
    const DirectX::XMFLOAT3& newScale)
{
    float now = static_cast<float>(GetTickCount64()) / 1000.0f;

    if (currentTimestamp == 0.0f) {
        previousRenderingPosition = newPosition;
        currentRenderingPosition = newPosition;
        previousTimestamp = now;
        currentTimestamp = now;
    }
    else {
        previousRenderingPosition = currentRenderingPosition;
        previousTimestamp = currentTimestamp;

        currentRenderingPosition = newPosition;
        currentTimestamp = now;
    }

    if (_collider) {
        _collider->setPosition(newPosition);
		_collider->setRotation(newRotation);
        _collider->setScale(newScale);
        _constantBuffer.World = _collider->updateWorldMatrix();
    }

    velocity = newVelocity;
}


void PhysicsObject::constrainToBounds()
{
    if (isFixed || !_collider) return;
    Sphere* sphere = dynamic_cast<Sphere*>(_collider.get());
    if (!sphere) return;

    const DirectX::XMFLOAT3 boxMin = { -globals::AXIS_LENGTH, -globals::AXIS_LENGTH, -globals::AXIS_LENGTH };
    const DirectX::XMFLOAT3 boxMax = { globals::AXIS_LENGTH,  globals::AXIS_LENGTH,  globals::AXIS_LENGTH };

    DirectX::XMFLOAT3 pos = sphere->getPosition();
    float radius = sphere->getRadius();
    const float bounceDamping = -0.4f;

    // Correct the logic to only affect velocity if the object is moving towards the boundary.
    // This prevents waking up sleeping objects.

    // X-Axis
    if (pos.x - radius < boxMin.x && velocity.x < 0) {
        pos.x = boxMin.x + radius;
        velocity.x *= bounceDamping;
    }
    else if (pos.x + radius > boxMax.x && velocity.x > 0) {
        pos.x = boxMax.x - radius;
        velocity.x *= bounceDamping;
    }
    // Y-Axis
    if (pos.y - radius < boxMin.y && velocity.y < 0) {
        pos.y = boxMin.y + radius;
        velocity.y *= bounceDamping;
    }
    else if (pos.y + radius > boxMax.y && velocity.y > 0) {
        pos.y = boxMax.y - radius;
        velocity.y *= bounceDamping;
    }
    // Z-Axis
    if (pos.z - radius < boxMin.z && velocity.z < 0) {
        pos.z = boxMin.z + radius;
        velocity.z *= bounceDamping;
    }
    else if (pos.z + radius > boxMax.z && velocity.z > 0) {
        pos.z = boxMax.z - radius;
        velocity.z *= bounceDamping;
    }

    sphere->setPosition(pos);
}

void PhysicsObject::Update(float deltaTime)
{
    //if (globals::isPaused.load()) return;

	if (!_collider)
    {
        OutputDebugString(L"[ERROR] Update() called but _collider is null\n");
        return;
    }
    if (isFixed) return;

   // previousPosition = _collider->getPosition();

    acceleration = {0.0f,
                    globals::gravityY * globals::gravityEnabled, // * inverseMass,
                    0.0f };

	int currentMethod = globals::integrationMethod.load();

	integrationMethod = static_cast<IntegrationMethod>(currentMethod);

    if (integrationMethod == IntegrationMethod::SEMI_IMPLICIT_EULER)
    {
        moveSemiImplicitEuler(deltaTime);
    }
    else if (integrationMethod == IntegrationMethod::RK4)
    {
        moveRK4(deltaTime);
    }
	else if (integrationMethod == IntegrationMethod::MIDPOINT)
	{
		moveMidpoint(deltaTime);
	}

    const float linearDamping = 0.998f;
    const float angularDamping = 0.995f;

    velocity.x *= linearDamping;
    velocity.y *= linearDamping;
    velocity.z *= linearDamping;

    angularVelocity.x *= angularDamping;
    angularVelocity.y *= angularDamping;
    angularVelocity.z *= angularDamping;

	// for slowest velocities, we set them to zero to prevent jittering and unnecessary calculations
    const float sleepEpsilon = 0.01f;

    float linearSpeedSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    float angularSpeedSq = angularVelocity.x * angularVelocity.x + angularVelocity.y * angularVelocity.y + angularVelocity.z * angularVelocity.z;

    if (linearSpeedSq < sleepEpsilon * sleepEpsilon && angularSpeedSq < sleepEpsilon * sleepEpsilon)
    {
        velocity = { 0.0f, 0.0f, 0.0f };
        angularVelocity = { 0.0f, 0.0f, 0.0f };
    }

    if (inverseMomentOfInertia > 0.0f)
    {
        float deltaRoll = angularVelocity.x * deltaTime * 0.1f;
        float deltaPitch = angularVelocity.y * deltaTime * 0.1f;
        float deltaYaw = angularVelocity.z * deltaTime * 0.1f;

        _collider->incrementRotation({
            XMConvertToDegrees(deltaRoll),
            XMConvertToDegrees(deltaPitch),
            XMConvertToDegrees(deltaYaw)
            });
    }

    _constantBuffer.World = _collider->updateWorldMatrix();
}

void PhysicsObject::savePreviousPosition()
{
    if (_collider)
        previousPosition = _collider->getPosition();
    else
        OutputDebugString(L"[WARNING] savePreviousPosition() skipped: _collider is null\n");
}

void PhysicsObject::resolveCollision(PhysicsObject& other, const DirectX::XMFLOAT3& collisionNormal, float penetrationDepth)
{
    if (!_collider || !other._collider || (isFixed && other.isFixed)) return;

	if (!isOwned()) return; // Only owned objects can resolve collisions

    float invMassA = isFixed ? 0.0f : inverseMass;
    float invMassB = other.isFixed ? 0.0f : other.inverseMass;
    float invMassSum = invMassA + invMassB;
    if (invMassSum <= 1e-6f) return;

    // --- 1. Restitution Impulse (Bounce) ---
    DirectX::XMFLOAT3 relativeVelocity = { velocity.x - other.velocity.x, velocity.y - other.velocity.y, velocity.z - other.velocity.z };
    float velocityAlongNormal = relativeVelocity.x * collisionNormal.x + relativeVelocity.y * collisionNormal.y + relativeVelocity.z * collisionNormal.z;

    if (velocityAlongNormal > 0.0f) return; // Objects are already moving apart

    int matA = static_cast<int>(material);
    int matB = static_cast<int>(other.material);

	float restitution = globals::elasticity.load();
	if (restitution < 0.0f) {
		// Use lookup table for restitution if not set globally
		restitution = elasticityLookup[matA][matB];
		globals::elasticity.store(restitution); // Update global value for consistency
	}

    // dv = -(1.0f + restitution) * velocityAlongNormal
	// J = m * dv
    float normalImpulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / invMassSum;

    DirectX::XMFLOAT3 normalImpulse = {
        normalImpulseMagnitude * collisionNormal.x,
        normalImpulseMagnitude * collisionNormal.y,
        normalImpulseMagnitude * collisionNormal.z
    };

    if (!isFixed) {
        velocity.x += normalImpulse.x * invMassA;
        velocity.y += normalImpulse.y * invMassA;
        velocity.z += normalImpulse.z * invMassA;
    }
    if (!other.isFixed) {
        other.velocity.x -= normalImpulse.x * invMassB;
        other.velocity.y -= normalImpulse.y * invMassB;
        other.velocity.z -= normalImpulse.z * invMassB;
    }

    // --- 2. Friction Impulse (Static and Dynamic) ---
    // Recalculate relative velocity after applying normal impulse
    relativeVelocity = { velocity.x - other.velocity.x, velocity.y - other.velocity.y, velocity.z - other.velocity.z };
    DirectX::XMVECTOR vRel = XMLoadFloat3(&relativeVelocity);
    DirectX::XMVECTOR vNorm = XMLoadFloat3(&collisionNormal);

    // Get the tangential component of the relative velocity
    DirectX::XMVECTOR velTangent = vRel - vNorm * XMVector3Dot(vRel, vNorm);
    float tangentSpeedSq = XMVectorGetX(XMVector3LengthSq(velTangent));

    DirectX::XMVECTOR frictionImpulseVec = XMVectorSet(0.f, 0.f, 0.f, 0.f);

    if (tangentSpeedSq > 1e-6f) {
        DirectX::XMVECTOR tangentDirection = XMVector3Normalize(velTangent);

        // Calculate impulse required to stop tangential motion
        float jtMagnitude = -XMVectorGetX(XMVector3Dot(vRel, tangentDirection));
        jtMagnitude /= invMassSum;

        // Get friction coefficients from lookup tables
        float staticFriction = globals::staticFriction.load();
		if (staticFriction < 0.0f) {
			staticFriction = staticFrictionLookup[matA][matB];
			globals::staticFriction.store(staticFriction); // Update global value for consistency
		}
		float dynamicFriction = globals::dynamicFriction.load();
		if (dynamicFriction < 0.0f) {
			dynamicFriction = dynamicFrictionLookup[matA][matB];
			globals::dynamicFriction.store(dynamicFriction); // Update global value for consistency
		}

        // Check if static friction is strong enough to prevent sliding
        if (std::abs(jtMagnitude) < staticFriction * normalImpulseMagnitude) {
            // Static Friction holds: Apply the exact impulse to prevent sliding
            frictionImpulseVec = tangentDirection * jtMagnitude;
        }
        else {
            // Dynamic (Kinetic) Friction: Static friction breaks, apply sliding friction
            frictionImpulseVec = tangentDirection * -dynamicFriction * normalImpulseMagnitude;
        }

        // Apply friction impulse to linear velocities
        if (!isFixed) {
            velocity.x += XMVectorGetX(frictionImpulseVec) * invMassA;
            velocity.y += XMVectorGetY(frictionImpulseVec) * invMassA;
            velocity.z += XMVectorGetZ(frictionImpulseVec) * invMassA;
        }
        if (!other.isFixed) {
            other.velocity.x -= XMVectorGetX(frictionImpulseVec) * invMassB;
            other.velocity.y -= XMVectorGetY(frictionImpulseVec) * invMassB;
            other.velocity.z -= XMVectorGetZ(frictionImpulseVec) * invMassB;
        }
    }

    // --- 3. Torque from Friction ---
    if (XMVectorGetX(XMVector3LengthSq(frictionImpulseVec)) > 1e-6f)
    {
        if (!isFixed && inverseMomentOfInertia > 0.0f) {
            if (auto sphereA = dynamic_cast<Sphere*>(_collider.get())) {
                DirectX::XMVECTOR rA = -vNorm * sphereA->getRadius();
                DirectX::XMVECTOR torqueA = DirectX::XMVector3Cross(rA, frictionImpulseVec);
                angularVelocity.x += DirectX::XMVectorGetX(torqueA) * inverseMomentOfInertia;
                angularVelocity.y += DirectX::XMVectorGetY(torqueA) * inverseMomentOfInertia;
                angularVelocity.z += DirectX::XMVectorGetZ(torqueA) * inverseMomentOfInertia;
            }
        }
        if (!other.isFixed && other.inverseMomentOfInertia > 0.0f) {
            if (auto sphereB = dynamic_cast<Sphere*>(other._collider.get())) {
                DirectX::XMVECTOR rB = vNorm * sphereB->getRadius();
                DirectX::XMVECTOR torqueB = DirectX::XMVector3Cross(rB, frictionImpulseVec); // Friction on B is opposite
                other.angularVelocity.x -= DirectX::XMVectorGetX(torqueB) * other.inverseMomentOfInertia;
                other.angularVelocity.y -= DirectX::XMVectorGetY(torqueB) * other.inverseMomentOfInertia;
                other.angularVelocity.z -= DirectX::XMVectorGetZ(torqueB) * other.inverseMomentOfInertia;
            }
        }
    }

    // --- 4. Positional Correction ---
    const float percent = 0.4f; // Increased a bit for more stability
    const float slop = 0.01f;
    float correctionMag = (std::max(penetrationDepth - slop, 0.0f) / invMassSum) * percent;
    DirectX::XMFLOAT3 correction = {
        correctionMag * collisionNormal.x,
        correctionMag * collisionNormal.y,
        correctionMag * collisionNormal.z
    };

    if (!isFixed) {
        _collider->incrementPosition({ correction.x * invMassA, correction.y * invMassA, correction.z * invMassA });
    }
    if (!other.isFixed) {
        other._collider->incrementPosition({ -correction.x * invMassB, -correction.y * invMassB, -correction.z * invMassB });
    }
}

const ConstantBuffer PhysicsObject::getConstantBuffer() const
{
    if (!_collider)
        OutputDebugString(L"[WARNING] getConstantBuffer() - collider is null\n");

    return _constantBuffer;
}

const DirectX::XMFLOAT3 PhysicsObject::getPosition() const
{
    if (_collider)
        return _collider->getPosition();
    else
    {
        OutputDebugString(L"[WARNING] getPosition() called but _collider is null\n");
        return { 0.0f, 0.0f, 0.0f };
    }
}

const DirectX::XMFLOAT3 PhysicsObject::getRotation() const
{
    if (_collider)
		return _collider->getRotation();
    else
    {
        OutputDebugString(L"[WARNING] getRotation() called but _collider is null\n");
        return { 0.0f, 0.0f, 0.0f };
    }
}
