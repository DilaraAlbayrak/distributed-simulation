#pragma once
#include "PhysicsObject.h"
#include "ShaderManager.h"
#include <optional>
#include <vector>
#include <mutex>

using globals::AXIS_LENGTH;

// A lightweight struct to hold information about a requested spawn.
// This is thread-safe to create and queue from any thread (e.g., network thread).
struct SpawnRequest
{
	DirectX::XMFLOAT3 position;
	float radius;
};

class Scenario
{
private:
	// These are kept in parallel to the object list in PhysicsManager.
	// The order of creation must be maintained.
	std::vector<CComPtr<ID3D11Buffer>> vertexBuffers;
	std::vector<CComPtr<ID3D11Buffer>> indexBuffers;
	std::vector<CComPtr<ID3D11InputLayout>> inputLayouts;
	std::vector<CComPtr<ID3D11Buffer>> constantBuffers;
	std::vector<UINT> indexCounts;

	// --- DirectX Handles ---
	CComPtr <ID3D11Device> device;
	CComPtr <ID3D11DeviceContext> context;
	CComPtr<ID3D11VertexShader> vertexShader;
	CComPtr<ID3D11PixelShader> pixelShader;
	CComPtr<ID3DBlob> vertexShaderBlob;
	CComPtr<ID3DBlob> pixelShaderBlob;

	// --- Scene Setup & Spawning Logic (Responsibility of Scenario) ---
	//static int integrationMethod; // 
	int numMovingSpheres = 25;
	float minRadius = 0.01f;
	float maxRadius = 0.01f;

	// Pre-calculated spawn locations
	size_t spawnIndex = 0;
	std::vector<std::tuple<float, float, float>> spawnData = {};

	// Thread-safe queue for pending spawn requests
	std::vector<SpawnRequest> _pendingSpawns;
	std::mutex _spawnMutex; // Protects access to _pendingSpawns

	// Private helper to create GPU resources for an object.
	HRESULT initRenderingResources(PhysicsObject* obj);

protected:
	Scenario(const CComPtr <ID3D11Device>& pDevice, const CComPtr <ID3D11DeviceContext>& pContext) : device(pDevice), context(pContext)
	{
	}

	void initObjects(const std::wstring& shaderFile = L"Simulation.fx");
	void unloadScenario();
	void applySharedGUI();

	void spawnRoom();
	void generateSpawnData(float areaHalfSize = globals::AXIS_LENGTH) {
		spawnData = generateUniform2DPositions(numMovingSpheres, areaHalfSize, minRadius, maxRadius);
		spawnIndex = 0;
	}
	std::vector<std::tuple<float, float, float>> generateUniform2DPositions(int n, float areaHalfSize, float minRadius, float maxRadius);

	// This is now a private helper called by processPendingSpawns to finalize object creation.
	void addPhysicsObject(std::unique_ptr<PhysicsObject> obj);

	// To be implemented by derived classes like Scenario1, Scenario2, etc.
	virtual void setupFixedObjects() = 0;

	static float randomFloat(float min, float max);

public:
	virtual ~Scenario() = default;

	// --- Public Interface ---
	virtual void onLoad() = 0;
	virtual void onUnload() = 0;
	virtual void onUpdate(float dt = 0.016f) = 0;
	virtual void ImGuiMainMenu() = 0;

	void setDeviceAndContext(CComPtr<ID3D11Device> pDevice, CComPtr<ID3D11DeviceContext> pContext)
	{
		device = pDevice;
		context = pContext;
	}

	void renderObjects();
	void onFrameUpdate(float dt = 0.016f);

	// Queues a request to spawn a sphere. Safe to call from any thread.
	void spawnMovingSphere();
	// Processes all queued requests. Must be called on the main/render thread.
	void processPendingSpawns();

	void setNumMovingSpheres(int n) { numMovingSpheres = n; }
	void setMinRadius(float r) { minRadius = r; }
	void setMaxRadius(float r) { maxRadius = r; }
};