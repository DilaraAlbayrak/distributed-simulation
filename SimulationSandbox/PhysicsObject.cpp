#include "PhysicsObject.h"
#include "Sphere.h"
#include <debugapi.h>
#include <algorithm>

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

    integrationMethod = IntegrationMethod::SEMI_IMPLICIT_EULER;
}


void PhysicsObject::Update(float deltaTime)
{
	if (!_collider)
    {
        OutputDebugString(L"[ERROR] Update() called but _collider is null\n");
        return;
    }
    if (isFixed) return;

   // previousPosition = _collider->getPosition();

    acceleration = {globals::gravity.x * inverseMass,
                    globals::gravity.y * inverseMass,
                    globals::gravity.z * inverseMass };

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
    if (!_collider || !other._collider)
    {
        OutputDebugString(L"[ERROR] resolveCollision() skipped: collider is null\n");
        return;
    }

    if (isFixed && other.isFixed) return;

    // --- 1. Calculate Relative Velocity ---
    DirectX::XMFLOAT3 relativeVelocity = {
        velocity.x - other.velocity.x,
        velocity.y - other.velocity.y,
        velocity.z - other.velocity.z
    };

    float velocityAlongNormal =
        relativeVelocity.x * collisionNormal.x +
        relativeVelocity.y * collisionNormal.y +
        relativeVelocity.z * collisionNormal.z;

    if (velocityAlongNormal > 0.0f) return;

    // --- 2. Calculate and Apply Collision Impulse ---
    float invMassA = isFixed ? 0.0f : inverseMass;
    float invMassB = other.isFixed ? 0.0f : other.inverseMass;
    float invMassSum = invMassA + invMassB;

    if (invMassSum <= 1e-6f) return;

    int matA = static_cast<int>(material);
    int matB = static_cast<int>(other.material);
    float e = elasticityLookup[matA][matB];

    float impulseMagnitude = -(1.0f + e) * velocityAlongNormal / invMassSum;

    DirectX::XMFLOAT3 impulse = {
        impulseMagnitude * collisionNormal.x,
        impulseMagnitude * collisionNormal.y,
        impulseMagnitude * collisionNormal.z
    };

    if (!isFixed) {
        velocity.x += impulse.x * invMassA;
        velocity.y += impulse.y * invMassA;
        velocity.z += impulse.z * invMassA;
    }

    if (!other.isFixed) {
        other.velocity.x -= impulse.x * invMassB;
        other.velocity.y -= impulse.y * invMassB;
        other.velocity.z -= impulse.z * invMassB;
    }

    // --- 3. Calculate and Apply Friction Impulse ---
    relativeVelocity = {
        velocity.x - other.velocity.x,
        velocity.y - other.velocity.y,
        velocity.z - other.velocity.z
    };

    XMVECTOR vRel = XMLoadFloat3(&relativeVelocity);
    XMVECTOR vNorm = XMLoadFloat3(&collisionNormal);
    XMVECTOR velTangent = vRel - vNorm * XMVector3Dot(vRel, vNorm);
    float tangentSpeedSq = XMVectorGetX(XMVector3LengthSq(velTangent));

    if (tangentSpeedSq > 1e-6f)
    {
        XMVECTOR tangentDirection = XMVector3Normalize(velTangent);
        float frictionCoefficient = 0.4f;

        float jt = -XMVectorGetX(XMVector3Dot(vRel, tangentDirection));
        jt /= invMassSum;

        if (jt > impulseMagnitude * frictionCoefficient)
        {
            jt = impulseMagnitude * frictionCoefficient;
        }

        XMVECTOR frictionImpulseVec = tangentDirection * jt;
        XMFLOAT3 frictionImpulse;
        XMStoreFloat3(&frictionImpulse, frictionImpulseVec);

        if (!isFixed)
        {
            velocity.x += frictionImpulse.x * invMassA;
            velocity.y += frictionImpulse.y * invMassA;
            velocity.z += frictionImpulse.z * invMassA;
        }
        if (!other.isFixed)
        {
            other.velocity.x -= frictionImpulse.x * invMassB;
            other.velocity.y -= frictionImpulse.y * invMassB;
            other.velocity.z -= frictionImpulse.z * invMassB;
        }
    }

    // --- 4. Positional Correction (to resolve penetration) ---
    const float percent = 0.6f;
    const float slop = 0.01f;
    float correctionMag = (std::max(penetrationDepth - slop, 0.0f) / invMassSum) * percent;

    DirectX::XMFLOAT3 correction = {
        correctionMag * collisionNormal.x,
        correctionMag * collisionNormal.y,
        correctionMag * collisionNormal.z
    };

    if (!isFixed && _collider) {
        _collider->incrementPosition({
            correction.x * invMassA,
            correction.y * invMassA,
            correction.z * invMassA
            });
    }

    if (!other.isFixed && other._collider) {
        other._collider->incrementPosition({
            -correction.x * invMassB,
            -correction.y * invMassB,
            -correction.z * invMassB
            });
    }

    // --- 5. Smart Sleep and Nudge Logic ---
    const float sleepEpsilonSq = 0.002f;
    const float slideThresholdSq = 0.05f; // This threshold might need tuning
    const float nudgeFactor = 0.1f;      // This controls the strength of the slide nudge

    auto handle_sleeping_and_nudging = [&](PhysicsObject& obj, const DirectX::XMFLOAT3& normal)
        {
            if (obj.isFixed) return;

            XMVECTOR v = XMLoadFloat3(&obj.velocity);
            if (XMVectorGetX(XMVector3LengthSq(v)) < sleepEpsilonSq)
            {
                XMVECTOR g = XMLoadFloat3(&globals::gravity);
                XMVECTOR n = XMLoadFloat3(&normal);
                XMVECTOR gTangent = g - n * XMVector3Dot(g, n);

                if (XMVectorGetX(XMVector3LengthSq(gTangent)) < slideThresholdSq)
                {
                    // Tangential gravity is negligible, it's safe to sleep.
                    obj.setVelocity({ 0.0f, 0.0f, 0.0f });
                }
                else
                {
                    // Object is on a slope but stopped. Give it a nudge to start sliding.
                    XMVECTOR nudge = XMVectorScale(gTangent, nudgeFactor * obj.inverseMass);
                    obj.velocity.x += XMVectorGetX(nudge);
                    obj.velocity.y += XMVectorGetY(nudge);
                    obj.velocity.z += XMVectorGetZ(nudge);
                }
            }
        };

    // Apply the logic for both objects
    handle_sleeping_and_nudging(*this, collisionNormal);
    XMFLOAT3 otherNormal = { -collisionNormal.x, -collisionNormal.y, -collisionNormal.z };
    handle_sleeping_and_nudging(other, otherNormal);
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
