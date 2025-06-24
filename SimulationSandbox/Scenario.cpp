#include "Scenario.h"
#include "Sphere.h"
#include "Plane.h"
#include "PhysicsManager.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <random>
#include <algorithm>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

HRESULT Scenario::initRenderingResources(PhysicsObject* obj)
{
	if (!device || !context)
	{
		OutputDebugString(L"[FATAL] device or context is nullptr in initRenderingResources()\n");
		return E_FAIL;
	}

	const std::vector<Vertex>& vertices = obj->getVertices();
	const std::vector<int>& indices = obj->getIndices();

	if (vertices.empty() || indices.empty())
		return E_FAIL;

	// **Define input layout for this object**
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(DirectX::XMFLOAT3), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT4), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	CComPtr<ID3D11InputLayout> inputLayout;
	HRESULT hr = device->CreateInputLayout(
		layout, ARRAYSIZE(layout),
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(),
		&inputLayout
	);

	if (FAILED(hr))
	{
		OutputDebugString(L"Failed to create input layout\n");
		return hr;
	}

	inputLayouts.push_back(inputLayout);

	// **Create vertex buffer**
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size());
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexInitData = {};
	vertexInitData.pSysMem = vertices.data();

	CComPtr<ID3D11Buffer> vertexBuffer;
	hr = device->CreateBuffer(&vertexBufferDesc, &vertexInitData, &vertexBuffer);
	if (FAILED(hr)) return hr;

	vertexBuffers.push_back(vertexBuffer);

	// **Create index buffer**
	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(int) * indices.size());
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexInitData = {};
	indexInitData.pSysMem = indices.data();

	CComPtr<ID3D11Buffer> indexBuffer;
	hr = device->CreateBuffer(&indexBufferDesc, &indexInitData, &indexBuffer);
	if (FAILED(hr)) return hr;

	indexBuffers.push_back(indexBuffer);
	indexCounts.push_back(static_cast<UINT>(indices.size()));

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.Usage = D3D11_USAGE_DEFAULT;
	cbDesc.ByteWidth = sizeof(ConstantBuffer);
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CComPtr <ID3D11Buffer> constantBuffer;
	device->CreateBuffer(&cbDesc, nullptr, &constantBuffer);
	constantBuffers.push_back(constantBuffer);

	// Set primitive topology
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return S_OK;
}

void Scenario::initObjects(const std::wstring& shaderFile)
{
	vertexBuffers.clear();
	indexBuffers.clear();
	inputLayouts.clear();
	indexCounts.clear();
	constantBuffers.clear();
	physicsObjects.clear();

	HRESULT hr = ShaderManager::getInstance(device)->compileShaderFromFile(shaderFile.c_str(), "VS", "vs_5_0", &vertexShaderBlob);
	if (FAILED(hr)) return;
	hr = device->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr, &vertexShader);
	if (FAILED(hr)) return;

	hr = ShaderManager::getInstance(device)->compileShaderFromFile(shaderFile.c_str(), "PS", "ps_5_0", &pixelShaderBlob);
	if (FAILED(hr)) return;
	hr = device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &pixelShader);
}

void Scenario::addPhysicsObject(std::unique_ptr<PhysicsObject> obj)
{
	std::shared_ptr<PhysicsObject> shared = std::move(obj);

	if (FAILED(initRenderingResources(shared.get()))) {
		OutputDebugString(L"[ERROR] Rendering resources creation failed\n");
		return;
	}

	PhysicsManager::getInstance().addObject(shared);
	physicsObjects.push_back(shared);
}

void Scenario::renderObjects()
{
	if (!vertexShader || !pixelShader) return;

	if (physicsObjects.size() != vertexBuffers.size()) return;

	for (size_t i = 0; i < physicsObjects.size(); ++i)
	{
		context->VSSetShader(vertexShader, nullptr, 0);
		context->PSSetShader(pixelShader, nullptr, 0);
		context->IASetInputLayout(inputLayouts[i]);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		ID3D11Buffer* vb = vertexBuffers[i];
		context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
		context->IASetIndexBuffer(indexBuffers[i], DXGI_FORMAT_R32_UINT, 0);

		ConstantBuffer cb = physicsObjects[i]->getConstantBuffer();
		context->UpdateSubresource(constantBuffers[i], 0, nullptr, &cb, 0, 0);
		context->VSSetConstantBuffers(1, 1, &constantBuffers[i]);
		context->PSSetConstantBuffers(1, 1, &constantBuffers[i]);

		context->DrawIndexed(indexCounts[i], 0, 0);
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
				true, 150.0f, Material::MAT4
			);

			wall->LoadModel("plane.sjg");

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

std::optional<std::pair<DirectX::XMFLOAT3, float>> Scenario::getNextSpawn()
{
	std::scoped_lock lock(_spawnMutex);

	if (spawnIndex >= spawnData.size())
		return std::nullopt;

	const auto& [x, z, radius] = spawnData[spawnIndex++];
	float y = globals::AXIS_LENGTH - (radius * 2.0f) * 1.2f;
	return std::make_pair(DirectX::XMFLOAT3(x, y, z), radius);
}

void Scenario::processPendingSpawns()
{
	auto result = getNextSpawn();
	if (!result.has_value()) return;

	const auto& [pos, radius] = result.value();
	float scale = radius * 2.0f;

	auto sphere = std::make_unique<PhysicsObject>(
		std::make_unique<Sphere>(
			pos,
			DirectX::XMFLOAT3(0, 0, 0),
			DirectX::XMFLOAT3(scale, scale, scale)
		),
		false
	);

	sphere->LoadModel("sphere.sjg");
	addPhysicsObject(std::move(sphere));
}

void Scenario::spawnMovingSphere()
{
	if (spawnIndex >= spawnData.size()) return;
	const auto& [x, z, radius] = spawnData[spawnIndex++];
	float scale = radius * 2.0f;
	float y = globals::AXIS_LENGTH - scale * 1.2f;

	auto sphere = std::make_unique<PhysicsObject>(
		std::make_unique<Sphere>(
			DirectX::XMFLOAT3(x, y, z),
			DirectX::XMFLOAT3(0, 0, 0),
			DirectX::XMFLOAT3(scale, scale, scale)
		),
		false
	);

	sphere->LoadModel("sphere.sjg");
	addPhysicsObject(std::move(sphere));
}

void Scenario::unloadScenario()
{
	vertexBuffers.clear();
	indexBuffers.clear();
	inputLayouts.clear();
	constantBuffers.clear();
	indexCounts.clear();
	physicsObjects.clear();
}

void Scenario::applySharedGUI()
{
	ImGui::Begin("Scenario Controls");
	ImGui::PushItemWidth(100);
	if (ImGui::RadioButton("Semi-Implicit Euler", integrationMethod == 0)) integrationMethod = 0;
	ImGui::SameLine();
	if (ImGui::RadioButton("RK4", integrationMethod == 1)) integrationMethod = 1;
	ImGui::PopItemWidth();
	ImGui::End();
}

void Scenario::onFrameUpdate(float dt) {
	// spawnMovingSphere(); // you may call this manually
}

std::vector<std::tuple<float, float, float>> Scenario::generateUniform2DPositions(int n, float areaHalfSize, float minRadius, float maxRadius)
{
	int gridSize = static_cast<int>(ceil(sqrt(n)));
	float cellSize = (areaHalfSize * 2.0f) / gridSize;

	std::vector<std::tuple<float, float, float>> positions;
	positions.reserve(n);

	for (int i = 0; i < gridSize && positions.size() < n; ++i)
		for (int j = 0; j < gridSize && positions.size() < n; ++j)
		{
			float radius = randomFloat(minRadius, maxRadius);
			float minX = -areaHalfSize + i * cellSize;
			float minZ = -areaHalfSize + j * cellSize;
			float centerX = minX + cellSize / 2.0f;
			float centerZ = minZ + cellSize / 2.0f;
			float margin = cellSize / 2.0f - (radius * 2.0f);
			float x = randomFloat(centerX - margin, centerX + margin);
			float z = randomFloat(centerZ - margin, centerZ + margin);
			positions.emplace_back(x, z, radius);
		}

	std::shuffle(positions.begin(), positions.end(), std::mt19937(std::random_device{}()));
	return positions;
}

float Scenario::randomFloat(float min, float max)
{
	static thread_local std::mt19937 generator(std::random_device{}());
	std::uniform_real_distribution<float> distribution(min, max);
	return distribution(generator);
}