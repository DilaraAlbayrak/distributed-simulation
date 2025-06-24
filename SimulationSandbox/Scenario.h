#pragma once
#include "PhysicsObject.h"
#include "ShaderManager.h"
#include <optional>
#include <shared_mutex>

using globals::AXIS_LENGTH;

class Scenario
{
private:
	std::vector<CComPtr<ID3D11Buffer>> vertexBuffers;
	std::vector<CComPtr<ID3D11Buffer>> indexBuffers;
	std::vector<CComPtr<ID3D11InputLayout>> inputLayouts;
	std::vector<CComPtr<ID3D11Buffer>> constantBuffers;
	CComPtr <ID3D11Device> device;
	CComPtr <ID3D11DeviceContext> context;
	CComPtr<ID3D11VertexShader> vertexShader;
	CComPtr<ID3D11PixelShader> pixelShader;
	CComPtr<ID3DBlob> vertexShaderBlob;
	CComPtr<ID3DBlob> pixelShaderBlob;
	std::vector<UINT> indexCounts;

	int integrationMethod = 0;

	int numMovingSpheres = 25;
	float minRadius = 0.01f;
	float maxRadius = 0.01f;
	size_t spawnIndex = 0;
	size_t numSpawned = 0;

	std::vector<std::shared_ptr<PhysicsObject>> physicsObjects;
	std::vector<std::tuple<float, float, float>> spawnData = {};

	// threading
	std::mutex _spawnMutex;

	HRESULT initRenderingResources(PhysicsObject* obj);

protected:
	Scenario(const CComPtr <ID3D11Device>& pDevice, const CComPtr <ID3D11DeviceContext>& pContext) : device(pDevice), context(pContext) {}
	void initObjects(const std::wstring& shaderFile = L"Simulation.fx");
	void unloadScenario();
	void applySharedGUI();

	void spawnRoom();
	void generateSpawnData(float areaHalfSize = globals::AXIS_LENGTH) {
		spawnData = generateUniform2DPositions(numMovingSpheres, areaHalfSize, minRadius, maxRadius);
		spawnIndex = 0;
	}
	std::vector<std::tuple<float, float, float>> generateUniform2DPositions(int n = 25, float areaHalfSize = globals::AXIS_LENGTH, float minRadius = 0.3f, float maxRadius = 0.3f);

	void addPhysicsObject(std::unique_ptr<PhysicsObject> obj);
	virtual void setupFixedObjects() = 0; // Pure virtual function to be implemented in derived classes

	// helper method(s)
	static float randomFloat(float min, float max);

public:
	virtual ~Scenario() = default;

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

	std::optional<std::pair<DirectX::XMFLOAT3, float>> getNextSpawn(); // returns position and radius of the next spawn, or std::nullopt if no more spawns are available

	void processPendingSpawns();

	void spawnMovingSphere();
	void setNumMovingSpheres(int n) { numMovingSpheres = n; }
	void setMinRadius(float r) { minRadius = r; }
	void setMaxRadius(float r) { maxRadius = r; }
};

