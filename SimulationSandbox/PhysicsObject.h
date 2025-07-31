#pragma once  
#include <memory>  
#include "SJGLoader.h"  
#include "Collider.h" 
#include "globals.h"
#include <shared_mutex>
#include <atomic>

enum class IntegrationMethod  
{  
SEMI_IMPLICIT_EULER,  
RK4,
MIDPOINT
};  

enum class Material
{
	MAT1,
	MAT2,
	MAT3,
	MAT4,
	MAT5
};

static float elasticityLookup[5][5] = {
	{ 0.70f, 0.50f, 0.45f, 0.60f, 0.30f },
	{ 0.60f, 0.50f, 0.35f, 0.55f, 0.40f },
	{ 0.45f, 0.35f, 0.75f, 0.30f, 0.45f },
	{ 0.70f, 0.55f, 0.30f, 0.65f, 0.50f },
	{ 0.30f, 0.40f, 0.45f, 0.50f, 0.35f }
};

static float staticFrictionLookup[5][5] = {
	{0.8f, 0.6f, 0.5f, 0.7f, 0.5f},
	{0.6f, 0.5f, 0.4f, 0.6f, 0.45f},
	{0.5f, 0.4f, 0.9f, 0.5f, 0.55f},
	{0.7f, 0.6f, 0.5f, 0.8f, 0.6f},
	{0.5f, 0.45f, 0.55f, 0.6f, 0.4f}
};

static float dynamicFrictionLookup[5][5] = {
	{0.6f, 0.45f, 0.35f, 0.55f, 0.3f},
	{0.45f, 0.4f, 0.3f, 0.5f, 0.35f},
	{0.35f, 0.3f, 0.7f, 0.4f, 0.4f},
	{0.55f, 0.5f, 0.4f, 0.6f, 0.5f},
	{0.3f, 0.35f, 0.4f, 0.5f, 0.25f}
};

struct ConstantBuffer  
{  
   DirectX::XMMATRIX World;  
   DirectX::XMFLOAT4 LightColour = { 0.3f,0.5f,0.7f, 1.0f };
   DirectX::XMFLOAT4 DarkColour = { 0.3f,0.4f,0.5f, 1.0f }; 
   DirectX::XMFLOAT2 CheckerboardSize = { 1.0f, 1.0f }; // Adjust tile size  
   DirectX::XMFLOAT2 Padding = { 0.0f, 0.0f };  
}; 

struct InstanceData
{
	DirectX::XMMATRIX World;
	DirectX::XMFLOAT4 LightColour;
	DirectX::XMFLOAT4 DarkColour;
};

class PhysicsObject  
{  
private:  
	std::unique_ptr<Collider> _collider = nullptr;  

	std::vector<Vertex> _vertices;  
	std::vector<int> _indices;  
	ConstantBuffer _constantBuffer;  

	// for distributed
	int _peerID = -1; // ID for peer-to-peer communication, if needed
	int _objectId = -1; // Unique ID for this object, used in networking
	bool _isOwned = false; // Flag to indicate if this object is owned by the local peer
	// for rendering latencies
	DirectX::XMFLOAT3 previousRenderingPosition = { 0.0f, globals::AXIS_LENGTH+1.0f, 0.0f };
	DirectX::XMFLOAT3 currentRenderingPosition = { 0.0f, globals::AXIS_LENGTH + 1.0f, 0.0f };
	float previousTimestamp;
	float currentTimestamp;

	bool isFixed = false;  
	DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };  
	DirectX::XMFLOAT3 angularVelocity = { 0.0f, 0.0f, 0.0f };  
	float inverseMomentOfInertia = 0.0f;
	DirectX::XMFLOAT3 acceleration = { 0.0f, 0.0f, 0.0f };  
	DirectX::XMFLOAT3 previousPosition = { 0.0f, 0.0f, 0.0f };
	//DirectX::XMFLOAT3 gravitationalForce; // Gravity vector  
	float mass = 1.0f;  
	float inverseMass = 1.0f;   
	Material material = Material::MAT1;
	IntegrationMethod integrationMethod = IntegrationMethod::RK4;

	// threading
	mutable std::mutex collisionMutex;
	mutable std::atomic_bool _positionRestored = false;

	void moveSemiImplicitEuler(float dt);  
	void moveRK4(float dt);  
	void moveMidpoint(float dt);

public: 
	PhysicsObject(std::unique_ptr<Collider> col, bool fixed = false, float objMass = 1.0f, Material mat = Material::MAT1);
	/*PhysicsObject(std::unique_ptr<Collider> col, bool fixed = false, float objMass = 1.0f, Material mat = Material::MAT1)
		: _collider(std::move(col)), isFixed(fixed)
	{
		mass = objMass;
		inverseMass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
		material = mat;
		previousPosition = _collider->getPosition();
		integrationMethod = IntegrationMethod::VERLET;
		_constantBuffer.World = _collider->updateWorldMatrix();
	}*/

	void Update(float dt);  

	bool LoadModel(const std::string& filename)  
	{  
		return SJGLoader::Load(filename, _vertices, _indices);  
	}  

	void savePreviousPosition();

	bool checkCollision(const PhysicsObject& other, DirectX::XMFLOAT3& outNormal, float& penetrationDepth) const
	{  
		if (!_collider || !other._collider) return false;

		return _collider->isColliding(*other._collider, outNormal, penetrationDepth);
	}  
	void resolveCollision(PhysicsObject& other, const DirectX::XMFLOAT3& collisionNormal, float penetrationDepth);

	void resetPositionRestorationFlag() const { _positionRestored = false; }

	const Collider& getCollider() const { return *_collider; }  
	Collider& getCollider() { return *_collider; }
	Collider* getColliderPtr() const { return _collider.get(); } // safer than using get() directly
	const std::vector<Vertex>& getVertices() const { return _vertices; }  
	const std::vector<int>& getIndices() const { return _indices; }  
	const DirectX::XMMATRIX& getTransformMatrix() const { return _constantBuffer.World; }  
	const ConstantBuffer getConstantBuffer() const;
	const float getMass() const { return mass; }  
	const DirectX::XMFLOAT3& getVelocity() const { return velocity; }
	const DirectX::XMFLOAT3 getPosition() const;
	const DirectX::XMFLOAT3 getScale() const { return _collider->getScale(); }
	const DirectX::XMFLOAT3 getRotation() const;

	void constrainToBounds();

	const bool getFixed() const { return isFixed; }  
	void setFixed(bool fixed) { isFixed = fixed; }  

	void setVelocity(const DirectX::XMFLOAT3& newVel) { velocity = newVel; }  
	void setAngularVelocity(const DirectX::XMFLOAT3& newAngVel) { angularVelocity = newAngVel; }  
	void setConstantBuffer(const ConstantBuffer& cb) { _constantBuffer = cb; }  
	void setIntegrationMethod(int method) { integrationMethod = static_cast<IntegrationMethod>(method); }  
	void setMass(float newMass); // { mass = newMass; inverseMass = (mass != 0.0f) ? 1.0f / mass : 0.0f; }

	// threading
	std::mutex& getCollisionMutex() const { return collisionMutex; }

	// networking
	void setPeerID(int id) { _peerID = id; }
	int getPeerID() const { return _peerID; }
	void setObjectId(int id) { _objectId = id; }
	int getObjectId() const { return _objectId; }
	void setIsOwned(bool owned) { _isOwned = owned; }
	bool isOwned() const { return _isOwned; }
	void setNetworkState(const DirectX::XMFLOAT3& newPosition, const DirectX::XMFLOAT3& newRotation, const DirectX::XMFLOAT3& newVelocity, const DirectX::XMFLOAT3& newScale);

	// smooth rendering for distributed
	DirectX::XMFLOAT3 getSmoothedPosition(float renderTime) const;
	void setPreviousRenderingPosition(const DirectX::XMFLOAT3& pos) { previousRenderingPosition = pos; }
	void setCurrentRenderingPosition(const DirectX::XMFLOAT3& pos) { currentRenderingPosition = pos; }
	void setPreviousTimestamp(float t) { previousTimestamp = t; }
	void setCurrentTimestamp(float t) { currentTimestamp = t; }

	DirectX::XMFLOAT3 getCurrentRenderingPosition() const { return currentRenderingPosition; }
	float getCurrentTimestamp() const { return currentTimestamp; }
};  
