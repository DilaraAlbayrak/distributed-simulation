#include "Scenario.h"
#include "PhysicsManager.h" // Crucial for the new architecture
#include "Sphere.h"
#include "Plane.h"
#include "ShaderManager.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <random>
#include <algorithm>
#include <cmath>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

HRESULT Scenario::initRenderingResources(PhysicsObject* obj)
{
	if (!device || !context) {
		OutputDebugString(L"[FATAL] Scenario::initRenderingResources - device or context is nullptr.\n");
		return E_FAIL;
	}
	const auto& vertices = obj->getVertices();
	const auto& indices = obj->getIndices();
	if (vertices.empty() || indices.empty()) {
		OutputDebugString(L"[WARNING] Scenario::initRenderingResources - Object model data is empty.\n");
		return E_FAIL;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(DirectX::XMFLOAT3), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(DirectX::XMFLOAT3) * 2, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	CComPtr<ID3D11InputLayout> inputLayout;
	HRESULT hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &inputLayout);
	if (FAILED(hr)) {
		OutputDebugString(L"Failed to create input layout.\n");
		return hr;
	}
	inputLayouts.push_back(inputLayout);

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size());
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = vertices.data();
	CComPtr<ID3D11Buffer> vb;
	hr = device->CreateBuffer(&bd, &initData, &vb);
	if (FAILED(hr)) return hr;
	vertexBuffers.push_back(vb);

	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.ByteWidth = static_cast<UINT>(sizeof(int) * indices.size());
	initData.pSysMem = indices.data();
	CComPtr<ID3D11Buffer> ib;
	hr = device->CreateBuffer(&bd, &initData, &ib);
	if (FAILED(hr)) return hr;
	indexBuffers.push_back(ib);
	indexCounts.push_back(static_cast<UINT>(indices.size()));

	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(ConstantBuffer);
	CComPtr<ID3D11Buffer> cb;
	hr = device->CreateBuffer(&bd, nullptr, &cb);
	if (FAILED(hr)) return hr;
	constantBuffers.push_back(cb);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	return S_OK;
}

void Scenario::initObjects(const std::wstring& shaderFile)
{
	HRESULT hr = ShaderManager::getInstance(device)->compileShaderFromFile(shaderFile.c_str(), "VS", "vs_5_0", &vertexShaderBlob);
	if (FAILED(hr)) return;
	hr = device->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr, &vertexShader);
	if (FAILED(hr)) return;

	hr = ShaderManager::getInstance(device)->compileShaderFromFile(shaderFile.c_str(), "PS", "ps_5_0", &pixelShaderBlob);
	if (FAILED(hr)) return;
	hr = device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &pixelShader);
}

void Scenario::unloadScenario()
{
	// Clear local rendering resources owned by this Scenario instance.
	vertexBuffers.clear();
	indexBuffers.clear();
	inputLayouts.clear();
	constantBuffers.clear();
	indexCounts.clear();

	// Clear all physics objects from the central PhysicsManager.
	PhysicsManager::getInstance().clearObjects();

	// Clear spawn data and any pending requests to ensure a clean state.
	spawnData.clear();
	spawnIndex = 0;
	{
		std::scoped_lock lock(_spawnMutex);
		_pendingSpawns.clear();
	}
}

void Scenario::addPhysicsObject(std::unique_ptr<PhysicsObject> obj)
{
	if (!obj) return;

	// Create the rendering resources for this object.
	if (FAILED(initRenderingResources(obj.get())))
	{
		OutputDebugString(L"[ERROR] Failed to initialize rendering resources. Object not added.\n");
		return;
	}

	// Transfer ownership to the PhysicsManager.
	PhysicsManager::getInstance().addObject(std::move(obj));
}

// Queues a request to spawn a new sphere
// It gets data from the pre-calculated spawn list
void Scenario::spawnMovingSphere()
{
	// Get the pre-calculated spawn data
	if (spawnIndex >= spawnData.size()) return;

	// Note: This part is not thread-safe by itself. If you call spawnMovingSphere from multiple threads,
	// you'll need to protect spawnIndex as well. For now, assuming it's called from the main thread.
	auto& data = spawnData[spawnIndex++];

	// Create a spawn request
	SpawnRequest request;
	request.position = { std::get<0>(data), globals::AXIS_LENGTH - 0.5f, std::get<1>(data) };
	request.radius = std::get<2>(data);

	// Add the request to the pending queue in a thread-safe way
	std::scoped_lock lock(_spawnMutex);
	_pendingSpawns.push_back(request);
}

// Processes all queued spawn requests. This must be called on the main/render thread
void Scenario::processPendingSpawns()
{
	// Quickly copy requests and clear the queue to minimize lock time.
	std::vector<SpawnRequest> requestsToProcess;
	{
		std::scoped_lock lock(_spawnMutex);
		if (_pendingSpawns.empty())
		{
			return;
		}
		requestsToProcess.swap(_pendingSpawns);
	}

	// Now process the copied requests without holding the lock.
	for (const auto& request : requestsToProcess)
	{
		float scale = request.radius;

		auto sphere = std::make_unique<PhysicsObject>(
			std::make_unique<Sphere>(
				request.position,
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(scale, scale, scale)
			),
			false, 1.0f, Material::MAT1
		);
		sphere->LoadModel("shapes/sphere.sjg");
		//sphere->setIntegrationMethod(integrationMethod);

		// This private helper creates resources and passes ownership to PhysicsManager
		addPhysicsObject(std::move(sphere));
	}
}

void Scenario::spawnRoom()
{
	auto createWallPlane = [&](const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& rotation,
		const DirectX::XMFLOAT3& normal,
		const DirectX::XMFLOAT4& colour)
		{
			auto wall = std::make_unique<PhysicsObject>(
				std::make_unique<Plane>(position, rotation, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f), normal),
				true, 1.0f, Material::MAT4
			);

			wall->LoadModel("shapes/plane.sjg");

			ConstantBuffer cb = wall->getConstantBuffer();
			cb.LightColour = colour;
			cb.DarkColour = colour;
			wall->setConstantBuffer(cb);

			addPhysicsObject(std::move(wall));
		};

	float axis = globals::AXIS_LENGTH;

	createWallPlane(
		{ 0.0f, 0.0f, -axis },   // back
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 0.55f, 0.65f, 0.75f, 1.0f }
	);

	createWallPlane(
		{ 0.0f, 0.0f, axis },    // front
		{ 0.0f, 180.0f, 0.0f },
		{ 0.0f, 0.0f, -1.0f },
		{ 0.55f, 0.65f, 0.75f, 1.0f }
	);

	createWallPlane(
		{ -axis, 0.0f, 0.0f },   // right
		{ 0.0f, 90.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 0.6f, 0.7f, 0.8f, 1.0f }
	);

	createWallPlane(
		{ axis, 0.0f, 0.0f },    // left
		{ 0.0f, -90.0f, 0.0f },
		{ -1.0f, 0.0f, 0.0f },
		{ 0.6f, 0.7f, 0.8f, 1.0f }
	);

	createWallPlane(
		{ 0.0f, -axis, 0.0f },   // floor
		{ -90.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.63f, 0.73f, 0.83f, 1.0f }
	);

	createWallPlane(
		{ 0.0f, axis, 0.0f },    // ceiling
		{ 90.0f, 0.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f },
		{ 0.63f, 0.73f, 0.83f, 1.0f }
	);

}

void Scenario::renderObjects()
{
	if (!vertexShader || !pixelShader || !context) return;

	auto& physicsManager = PhysicsManager::getInstance();

	// Call the new accessor function and provide a lambda that contains the rendering logic.
	physicsManager.accessPhysicsObjects([&](std::span<const std::shared_ptr<PhysicsObject>> objectsToRender) {

		// The code inside this lambda is executed while the physics manager's mutex is locked.

		if (objectsToRender.size() != vertexBuffers.size())
		{
			return; // Mismatch, exit safely.
		}

		context->VSSetShader(vertexShader, nullptr, 0);
		context->PSSetShader(pixelShader, nullptr, 0);

		for (size_t i = 0; i < objectsToRender.size(); ++i)
		{
			const auto& obj = objectsToRender[i];
			if (!obj) continue;

			// Set resources for this object
			context->IASetInputLayout(inputLayouts[i]);
			UINT stride = sizeof(Vertex);
			UINT offset = 0;
			context->IASetVertexBuffers(0, 1, &vertexBuffers[i].p, &stride, &offset);
			context->IASetIndexBuffer(indexBuffers[i], DXGI_FORMAT_R32_UINT, 0);

			// Update and set constant buffer
			ConstantBuffer cb = obj->getConstantBuffer();
			context->UpdateSubresource(constantBuffers[i], 0, nullptr, &cb, 0, 0);
			context->VSSetConstantBuffers(1, 1, &constantBuffers[i].p);
			context->PSSetConstantBuffers(1, 1, &constantBuffers[i].p);

			// Draw the object
			context->DrawIndexed(indexCounts[i], 0, 0);
		}
		}); // End of lambda
}


void Scenario::onFrameUpdate(float dt) {
	
}

std::vector<std::tuple<float, float, float>> Scenario::generateUniform2DPositions(int n, float areaHalfSize, float minRadius, float maxRadius)
{
	if (n <= 0) return {};
	int gridSize = static_cast<int>(ceil(sqrt(n)));
	float cellSize = (areaHalfSize * 2.0f) / gridSize;
	std::vector<std::tuple<float, float, float>> positions;
	positions.reserve(n);
	for (int i = 0; i < gridSize && positions.size() < n; ++i) {
		for (int j = 0; j < gridSize && positions.size() < n; ++j) {
			float radius = randomFloat(minRadius, maxRadius);
			float minX = -areaHalfSize + i * cellSize;
			float minZ = -areaHalfSize + j * cellSize;
			float centerX = minX + cellSize / 2.0f;
			float centerZ = minZ + cellSize / 2.0f;
			float margin = std::max(0.0f, cellSize / 2.0f - radius);
			float x = randomFloat(centerX - margin, centerX + margin);
			float z = randomFloat(centerZ - margin, centerZ + margin);
			positions.emplace_back(x, z, radius);
		}
	}
	auto rd = std::random_device{};
	auto rng = std::mt19937{ rd() };
	std::shuffle(positions.begin(), positions.end(), rng);
	return positions;
}

float Scenario::randomFloat(float min, float max)
{
	static thread_local std::mt19937 generator(std::random_device{}());
	std::uniform_real_distribution<float> distribution(min, max);
	return distribution(generator);
}

void Scenario::applySharedGUI()
{
	ImGui::Begin("Scenario Controls");
	ImGui::PushItemWidth(150);

	int currentMethod = globals::integrationMethod.load();

	if (ImGui::RadioButton("Semi-Implicit Euler", &currentMethod, 0))
	{
		globals::integrationMethod = currentMethod;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("RK4", &currentMethod, 1))
	{
		globals::integrationMethod = currentMethod;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Midpoint", &currentMethod, 2)) 
	{
		globals::integrationMethod = currentMethod;
	}

	ImGui::PopItemWidth();
	ImGui::End();
}