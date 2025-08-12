#pragma once
#include "NetworkManager.h"
#include "PhysicsObject.h"
#include "ShaderManager.h"
#include <optional>
#include <vector>
#include <mutex>
#include <random>

using globals::AXIS_LENGTH;

// A lightweight struct to hold information about a requested spawn.
struct SpawnRequest
{
	DirectX::XMFLOAT3 position;
	float radius;
};

class Scenario
{
private:
	// --- Resources for Fixed (Non-Instanced) Objects ---
	// These are kept in parallel to the object list in PhysicsManager.
	std::vector<CComPtr<ID3D11Buffer>> vertexBuffers;
	std::vector<CComPtr<ID3D11Buffer>> indexBuffers;
	std::vector<CComPtr<ID3D11InputLayout>> inputLayouts;
	std::vector<CComPtr<ID3D11Buffer>> constantBuffers;
	std::vector<UINT> indexCounts;

	// --- DirectX Handles & Shaders ---
	CComPtr <ID3D11Device> device;
	CComPtr <ID3D11DeviceContext> context;
	CComPtr<ID3D11VertexShader> vertexShader; // This will now be the INSTANCED shader
	CComPtr<ID3D11VertexShader> vertexShader_NonInstanced;
	CComPtr<ID3D11PixelShader> pixelShader;
	CComPtr<ID3DBlob> vertexShaderBlob; // For non-instanced layout
	CComPtr<ID3DBlob> _vertexShaderInstancedBlob; // NEW: For instanced layout
	CComPtr<ID3DBlob> pixelShaderBlob;

	// --- Resources for Moving Spheres (Instanced) ---
	CComPtr<ID3D11Buffer> _sphereVertexBufferInstanced;
	CComPtr<ID3D11Buffer> _sphereIndexBufferInstanced;
	CComPtr<ID3D11InputLayout> _inputLayoutInstanced;
	UINT _sphereIndexCountInstanced = 0;

	// --- Instancing Data Handling ---
	//std::vector<DirectX::XMMATRIX> instanceWorldMatrices;
	std::vector<InstanceData> instanceData;
	CComPtr<ID3D11Buffer> instanceBuffer;
	UINT numInstances = 0;

	// --- Scene Setup & Spawning Logic ---
	int numMovingSpheres = 25;
	float minRadius = 0.01f;
	float maxRadius = 0.01f;

	size_t spawnIndex = 0;
	std::vector<std::tuple<float, float, float>> spawnData = {};

	std::vector<SpawnRequest> _pendingSpawns;
	std::mutex _spawnMutex;

	// distributed part
	int _nextPeerId = 0; // For distributed scenarios, to assign unique IDs to new objects
	std::mt19937 _randGen;
	std::uniform_int_distribution<int> _peerDist;
	static const DirectX::XMFLOAT4 peerColors[];
	unsigned int _nextObjectId = 0; // For distributed scenarios, to assign unique IDs to new objects

	// --- Private Helper Methods ---
	HRESULT initRenderingResources(PhysicsObject* obj);

protected:
	Scenario(const CComPtr <ID3D11Device>& pDevice, const CComPtr <ID3D11DeviceContext>& pContext) : device(pDevice), context(pContext)
	{
		_randGen = std::mt19937(4);
		_peerDist = std::uniform_int_distribution<int>(0, globals::NUM_PEERS - 1);
	}

	void initObjects(const std::wstring& shaderFile = L"Simulation.fx");
	void initInstancedRendering();
	void unloadScenario();
	void applySharedGUI();

	void spawnRoom();
	void generateSpawnData(float areaHalfSize = globals::AXIS_LENGTH) {
		spawnData = generateUniform2DPositions(numMovingSpheres, areaHalfSize, minRadius, maxRadius);
		spawnIndex = 0;
	}
	std::vector<std::tuple<float, float, float>> generateUniform2DPositions(int n, float areaHalfSize, float minRadius, float maxRadius);

	void addPhysicsObject(std::unique_ptr<PhysicsObject> obj);

	virtual void setupFixedObjects() = 0;

	static float randomFloat(float min, float max);

	void updateInstanceBuffer(float dt = 0.016f);

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

	void spawnMovingSphere();
	void processPendingSpawns();

	void setNumMovingSpheres(int n) { numMovingSpheres = n; }
	void setMinRadius(float r) { minRadius = r; }
	void setMaxRadius(float r) { maxRadius = r; }
};