#include "Scenario.h"
#include "PhysicsManager.h"
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

const DirectX::XMFLOAT4 Scenario::peerColors[] = {
		{ 0.7f, 0.3f, 0.5f, 1.0f }, // Red
		{ 0.3f, 0.7f, 0.5f, 1.0f }, // Green
		{ 0.3f, 0.5f, 0.7f, 1.0f }, // Blue
		{ 0.7f, 0.7f, 0.2f, 1.0f }  // Yellow
};

void Scenario::initObjects(const std::wstring& shaderFile)
{
	HRESULT hr;
	CComPtr<ID3DBlob> vsBlob;

	// Compile the NON-INSTANCED shader
	hr = ShaderManager::getInstance(device)->compileShaderFromFile(shaderFile.c_str(), "VS", "vs_5_0", &vsBlob);
	if (SUCCEEDED(hr))
	{
		device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader_NonInstanced);
		vertexShaderBlob = vsBlob;
	}

	// Compile the INSTANCED shader and store its blob for the input layout.
	hr = ShaderManager::getInstance(device)->compileShaderFromFile(shaderFile.c_str(), "VS_Instanced", "vs_5_0", &_vertexShaderInstancedBlob);
	if (SUCCEEDED(hr))
	{
		device->CreateVertexShader(_vertexShaderInstancedBlob->GetBufferPointer(), _vertexShaderInstancedBlob->GetBufferSize(), nullptr, &vertexShader);
	}

	// Compile the Pixel Shader (only one is needed).
	hr = ShaderManager::getInstance(device)->compileShaderFromFile(shaderFile.c_str(), "PS", "ps_5_0", &pixelShaderBlob);
	if (SUCCEEDED(hr))
	{
		device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &pixelShader);
	}
}

void Scenario::initInstancedRendering()
{
	HRESULT hr;

	// 1. Load the base sphere mesh data once.
	std::vector<Vertex> sphereVertices;
	std::vector<int> sphereIndices;
	if (!SJGLoader::Load("shapes/sphere.sjg", sphereVertices, sphereIndices))
	{
		OutputDebugString(L"FATAL: Failed to load sphere.sjg for instancing.\n");
		return;
	}
	_sphereIndexCountInstanced = static_cast<UINT>(sphereIndices.size());

	// 2. Create the vertex buffer for the base sphere mesh.
	D3D11_BUFFER_DESC vbd = {};
	vbd.Usage = D3D11_USAGE_DEFAULT;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * sphereVertices.size());
	D3D11_SUBRESOURCE_DATA vsd = {};
	vsd.pSysMem = sphereVertices.data();
	hr = device->CreateBuffer(&vbd, &vsd, &_sphereVertexBufferInstanced);
	if (FAILED(hr)) return;

	// 3. Create the index buffer for the base sphere mesh.
	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.ByteWidth = static_cast<UINT>(sizeof(int) * sphereIndices.size());
	D3D11_SUBRESOURCE_DATA isd = {};
	isd.pSysMem = sphereIndices.data();
	hr = device->CreateBuffer(&ibd, &isd, &_sphereIndexBufferInstanced);
	if (FAILED(hr)) return;

	// 4. Create the DYNAMIC instance buffer (created once, updated every frame).
	D3D11_BUFFER_DESC instBd = {};
	instBd.Usage = D3D11_USAGE_DYNAMIC;
	instBd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	instBd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	instBd.ByteWidth = sizeof(InstanceData) * std::max(numMovingSpheres, 1);
	hr = device->CreateBuffer(&instBd, nullptr, &instanceBuffer);
	if (FAILED(hr)) return;

	// 5. Create the Input Layout for INSTANCED rendering.
	D3D11_INPUT_ELEMENT_DESC instancedLayout[] =
	{
		// Per-Vertex Data from buffer slot 0
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		//{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		// Per-Instance Transform (slot 1)
		{ "INSTANCE_TRANSFORM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TRANSFORM", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TRANSFORM", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TRANSFORM", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },

		// Per-Instance Colour Info (after transform matrix)
		{ "INSTANCE_LIGHTCOLOUR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_DARKCOLOUR",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 80,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};

	hr = device->CreateInputLayout(
		instancedLayout,
		ARRAYSIZE(instancedLayout),
		_vertexShaderInstancedBlob->GetBufferPointer(),
		_vertexShaderInstancedBlob->GetBufferSize(),
		&_inputLayoutInstanced
	);

	if (FAILED(hr))
	{
		OutputDebugString(L"Failed to create instanced input layout.\n");
	}
}

void Scenario::updateInstanceBuffer()
{
	instanceData.clear();
	// We can reserve based on the total number of moving spheres we intend to have.
	// This avoids multiple reallocations as spheres are spawned.
	instanceData.reserve(numMovingSpheres);

	float renderTime = static_cast<float>(GetTickCount64()) / 1000.0f;

	// The new access function gives us both lists.
	// We only need the 'movingObjects' list here.
	PhysicsManager::getInstance().accessAllObjects([&](
		std::span<const std::shared_ptr<PhysicsObject>> movingObjects,
		[[maybe_unused]] std::span<const std::shared_ptr<PhysicsObject>> fixedObjects)
		{
			// Now we loop directly over the list of moving objects.
			// No need for 'if (!obj->getFixed())' check anymore!
			for (const auto& obj : movingObjects)
			{
				if (obj)
				{
					InstanceData data;

					if (!obj->isOwned())
					{
						DirectX::XMFLOAT3 smoothedPos = obj->getSmoothedPosition(renderTime);

						Collider* collider = obj->getColliderPtr(); 
						if (collider)
						{
							collider->setPosition(smoothedPos);
							data.World = DirectX::XMMatrixTranspose(collider->updateWorldMatrix());
						}
						else
						{
							// Fallback: use a simple translation matrix
							data.World = DirectX::XMMatrixTranslation(smoothedPos.x, smoothedPos.y, smoothedPos.z);
						}
					}
					else
					{
						data.World = DirectX::XMMatrixTranspose(obj->getTransformMatrix());
					}

					const ConstantBuffer& cb = obj->getConstantBuffer();
					data.LightColour = cb.LightColour;
					data.DarkColour = cb.DarkColour;

					instanceData.push_back(data);
				}
			}
		});

	numInstances = static_cast<UINT>(instanceData.size());
	if (numInstances == 0) return;

	// The rest of the function (Map, memcpy, Unmap) remains the same.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr = context->Map(instanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(hr))
	{
		memcpy(mappedResource.pData, instanceData.data(), sizeof(InstanceData) * numInstances);
		context->Unmap(instanceBuffer, 0);
	}
}

void Scenario::unloadScenario()
{
	// Clear local rendering resources owned by this Scenario instance.
	vertexBuffers.clear();
	indexBuffers.clear();
	inputLayouts.clear();
	constantBuffers.clear();
	indexCounts.clear();

	// Clear instancing-specific resources.
	_sphereVertexBufferInstanced.Release();
	_sphereIndexBufferInstanced.Release();
	_inputLayoutInstanced.Release();
	instanceBuffer.Release();
	_sphereIndexCountInstanced = 0;

	// Clear all physics objects from the central PhysicsManager.
	//PhysicsManager::getInstance().clearObjects();

	// Clear spawn data and any pending requests to ensure a clean state.
	spawnData.clear();
	spawnIndex = 0;
	{
		std::scoped_lock lock(_spawnMutex);
		_pendingSpawns.clear();
	}
}

// This function prepares rendering resources for a single, non-instanced, fixed object.
HRESULT Scenario::initRenderingResources(PhysicsObject* obj)
{
	if (!device || !context)
	{
		OutputDebugString(L"FATAL: Scenario::initRenderingResources - device or context is nullptr.\n");
		return E_FAIL;
	}

	const auto& vertices = obj->getVertices();
	const auto& indices = obj->getIndices();
	if (vertices.empty() || indices.empty())
	{
		OutputDebugString(L"WARNING: Scenario::initRenderingResources - Object model data is empty.\n");
		return E_FAIL;
	}

	// This input layout is for NON-INSTANCED objects.
	// It only describes per-vertex data and matches the 'VS_NonInstanced' shader.
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		//{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	CComPtr<ID3D11InputLayout> inputLayout;
	// Ensure 'vertexShaderBlob' is the compiled blob from 'VS_NonInstanced'.
	HRESULT hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &inputLayout);
	if (FAILED(hr))
	{
		OutputDebugString(L"Failed to create non-instanced input layout.\n");
		return hr;
	}
	inputLayouts.push_back(inputLayout);

	// Create Vertex Buffer
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

	// Create Index Buffer
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.ByteWidth = static_cast<UINT>(sizeof(int) * indices.size());
	initData.pSysMem = indices.data();
	CComPtr<ID3D11Buffer> ib;
	hr = device->CreateBuffer(&bd, &initData, &ib);
	if (FAILED(hr)) return hr;
	indexBuffers.push_back(ib);
	indexCounts.push_back(static_cast<UINT>(indices.size()));

	// Create Constant Buffer for this specific object
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(ConstantBuffer);
	CComPtr<ID3D11Buffer> cb;
	hr = device->CreateBuffer(&bd, nullptr, &cb);
	if (FAILED(hr)) return hr;
	constantBuffers.push_back(cb);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	return S_OK;
}

void Scenario::addPhysicsObject(std::unique_ptr<PhysicsObject> obj)
{
	if (!obj) return;

	if (obj->getFixed())
	{
		// Only create rendering resources for fixed objects.
		if (FAILED(initRenderingResources(obj.get())))
		{
			OutputDebugString(L"[ERROR] Failed to initialize rendering resources. Object not added.\n");
			return;
		}
	}
	// Note: We no longer need a special case for non-fixed objects here.
	// Their rendering is handled entirely by the instancing system.

	PhysicsManager::getInstance().addObject(std::move(obj));
}

void Scenario::spawnMovingSphere()
{
	if (spawnIndex >= spawnData.size()) return;

	auto& data = spawnData[spawnIndex++];

	SpawnRequest request;
	request.position = { std::get<0>(data), globals::AXIS_LENGTH - 0.5f, std::get<1>(data) };
	request.radius = std::get<2>(data);

	std::scoped_lock lock(_spawnMutex);
	_pendingSpawns.push_back(request);
}

void Scenario::processPendingSpawns()
{
	std::vector<SpawnRequest> requestsToProcess;
	{
		std::scoped_lock lock(_spawnMutex);
		if (_pendingSpawns.empty())
		{
			return;
		}
		requestsToProcess.swap(_pendingSpawns);
	}

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
		int localPeerId = NetworkManager::getInstance().getLocalPeerId();
		_nextPeerId = _peerDist(_randGen);
		int objectId = (_nextPeerId << 24) | _nextObjectId++; // Combine local peer ID with a unique object ID, shift to ensure uniqueness across peers
		sphere->setObjectId(objectId);

		sphere->setPeerID(_nextPeerId); // Assign a random peer ID for distributed scenarios
		sphere->setIsOwned(localPeerId == _nextPeerId); // Set ownership based on peer ID
		//sphere->setPeerID(localPeerId); // Assign a random peer ID for distributed scenarios
		ConstantBuffer cb = sphere->getConstantBuffer();
		cb.LightColour = peerColors[_nextPeerId];
		cb.DarkColour = { cb.LightColour.x * 0.75f, cb.LightColour.y * 0.75f, cb.LightColour.z * 0.75f, 1.0f };
		sphere->setConstantBuffer(cb);

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
				true, 100.0f, Material::MAT4 // mass is 100
			);

			wall->LoadModel("shapes/plane.sjg");

			ConstantBuffer cb = wall->getConstantBuffer();
			cb.LightColour = colour;
			cb.DarkColour = colour;
			wall->setConstantBuffer(cb);

			addPhysicsObject(std::move(wall));
		};

	float axis = globals::AXIS_LENGTH;

	createWallPlane({ 0.0f, 0.0f, -axis }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.55f, 0.65f, 0.75f, 1.0f });
	createWallPlane({ 0.0f, 0.0f, axis }, { 0.0f, 180.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.55f, 0.65f, 0.75f, 1.0f });
	createWallPlane({ -axis, 0.0f, 0.0f }, { 0.0f, 90.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.6f, 0.7f, 0.8f, 1.0f });
	createWallPlane({ axis, 0.0f, 0.0f }, { 0.0f, -90.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.6f, 0.7f, 0.8f, 1.0f });
	createWallPlane({ 0.0f, -axis, 0.0f }, { -90.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.63f, 0.73f, 0.83f, 1.0f });
	createWallPlane({ 0.0f, axis, 0.0f }, { 90.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.63f, 0.73f, 0.83f, 1.0f });
}

void Scenario::renderObjects()
{
	if (!context || !pixelShader) return;

	context->PSSetShader(pixelShader, nullptr, 0);

	auto& physicsManager = PhysicsManager::getInstance();

	// Use the new access function to get both lists separately.
	physicsManager.accessAllObjects([&](
		std::span<const std::shared_ptr<PhysicsObject>> movingObjects,
		std::span<const std::shared_ptr<PhysicsObject>> fixedObjects)
		{
			// --- Render FIXED objects (Non-Instanced) ---
			if (vertexShader_NonInstanced && !fixedObjects.empty())
			{
				context->VSSetShader(vertexShader_NonInstanced, nullptr, 0);

				// Loop directly over the fixed objects list.
				// The rendering resources (vertexBuffers, etc.) are stored in parallel.
				for (size_t i = 0; i < fixedObjects.size(); ++i)
				{
					const auto& obj = fixedObjects[i];
					if (!obj) continue;
					if (i >= vertexBuffers.size()) continue; // Safety check

					context->IASetInputLayout(inputLayouts[i]);
					UINT stride = sizeof(Vertex);
					UINT offset = 0;
					context->IASetVertexBuffers(0, 1, &vertexBuffers[i].p, &stride, &offset);
					context->IASetIndexBuffer(indexBuffers[i], DXGI_FORMAT_R32_UINT, 0);

					ConstantBuffer cb = obj->getConstantBuffer();
					context->UpdateSubresource(constantBuffers[i], 0, nullptr, &cb, 0, 0);
					context->VSSetConstantBuffers(1, 1, &constantBuffers[i].p);
					context->PSSetConstantBuffers(1, 1, &constantBuffers[i].p);

					context->DrawIndexed(indexCounts[i], 0, 0);
				}
			}

			// --- Render MOVING spheres (Instanced) ---
			// 'numInstances' is already calculated by updateInstanceBuffer.
			// 'movingObjects' list is not directly used here, but this ensures
			// we don't try to render if there are no moving objects.
			if (vertexShader && numInstances > 0 && instanceBuffer && !movingObjects.empty())
			{
				context->VSSetShader(vertexShader, nullptr, 0);

				ID3D11Buffer* instancedBuffers[] = { _sphereVertexBufferInstanced, instanceBuffer };
				UINT strides[] = { sizeof(Vertex), sizeof(InstanceData) };
				UINT offsets[] = { 0, 0 };

				context->IASetInputLayout(_inputLayoutInstanced);
				context->IASetVertexBuffers(0, 2, instancedBuffers, strides, offsets);
				context->IASetIndexBuffer(_sphereIndexBufferInstanced, DXGI_FORMAT_R32_UINT, 0);

				context->DrawIndexedInstanced(_sphereIndexCountInstanced, numInstances, 0, 0, 0);
			}
		});
}

void Scenario::onFrameUpdate(float dt)
{
	updateInstanceBuffer();
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
	// Check if the main menu bar is visible
	if (ImGui::BeginMainMenuBar())
	{
		// Create the "Integration" menu
		if (ImGui::BeginMenu("Integration"))
		{
			// Get the current integration method
			int currentMethod = globals::integrationMethod.load();

			// Menu item for Semi-Implicit Euler
			if (ImGui::MenuItem("Semi-Implicit Euler", nullptr, currentMethod == 0))
			{
				globals::integrationMethod = 0;
			}
			// Menu item for RK4
			if (ImGui::MenuItem("RK4", nullptr, currentMethod == 1))
			{
				globals::integrationMethod = 1;
			}
			// Menu item for Midpoint
			if (ImGui::MenuItem("Midpoint", nullptr, currentMethod == 2))
			{
				globals::integrationMethod = 2;
			}

			ImGui::EndMenu();
		}

		bool stateChanged = false; // Flag to track if any distributed state was changed


		if (ImGui::BeginMenu("Timing"))
		{
			// Start a two-column table to place settings side-by-side.
			if (ImGui::BeginTable("SettingsTable", 2, ImGuiTableFlags_BordersInnerV))
			{
				// --- Column 1: Global Settings ---
				ImGui::TableNextColumn();
				ImGui::Text("Global Settings (*)");
				ImGui::Separator();

				float simFreq = globals::targetSimFrequencyHz.load();
				if (ImGui::SliderFloat("Sim Freq (Hz)", &simFreq, 1.0f, 1000.0f, "%.0f"))
				{
					globals::targetSimFrequencyHz.store(simFreq);
					stateChanged = true;
				}

				float deltaTime = 1.0f / globals::targetSimFrequencyHz.load();
				if (ImGui::DragFloat("Delta Time (s)", &deltaTime, 0.005f, 0.005f, 1.0f, "%.3f"))
				{
					if (deltaTime > 0.0f)
					{
						globals::targetSimFrequencyHz.store(1.0f / deltaTime);
						stateChanged = true;
					}
				}

				float netFreq = globals::targetNetFrequencyHz.load();
				if (ImGui::SliderFloat("Net Freq (Hz)", &netFreq, 1.0f, 60.0f, "%.0f"))
				{
					globals::targetNetFrequencyHz.store(netFreq);
					stateChanged = true;
				}

				// --- Column 2: Local Settings ---
				ImGui::TableNextColumn();

				ImGui::SetNextItemWidth(120.0f);
				ImGui::Separator();

				float gfxFreq = globals::targetGfxFrequencyHz.load();
				if (ImGui::SliderFloat("Target GFX Freq (Hz)", &gfxFreq, 1.0f, 144.0f, "%.0f"))
				{
					globals::targetGfxFrequencyHz.store(gfxFreq);
				}

				ImGui::Text("Actual GFX: %.1f Hz", globals::actualGfxFrequencyHz.load());
				ImGui::Text("Actual Sim: %.1f Hz", globals::actualSimFrequencyHz.load());
				ImGui::Text("Actual Net: %.1f Hz", globals::actualNetFrequencyHz.load());
				ImGui::Text("Num. moving objects: %d", numMovingSpheres);

				ImGui::EndTable();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Material"))
		{
			// Start a two-column table to place settings side-by-side.
			//if (ImGui::BeginTable("MaterialsTable", 1, ImGuiTableFlags_BordersInnerV))
			{
				float elasticity = globals::elasticity.load();
				if (ImGui::SliderFloat("Elasticiy", &elasticity, 0.0f, 1.0f, "%.1f"))
				{
					globals::elasticity.store(elasticity);
					//PhysicsObject::setRestitution(elasticity);
					stateChanged = true;
				}
			}

			float staticFriction = globals::staticFriction.load();
			if (ImGui::SliderFloat("Static F.", &staticFriction, 0.0f, 1.0f, "%.1f"))
			{
				globals::staticFriction.store(staticFriction);
				//PhysicsObject::setStaticFriction(staticFriction);
				stateChanged = true;
			}

			float dynamicFriction = globals::dynamicFriction.load();
			if (ImGui::SliderFloat("Dynamic F.", &dynamicFriction, 0.0f, 1.0f, "%.1f"))
			{
				globals::dynamicFriction.store(dynamicFriction);
				//PhysicsObject::setDynamicFriction(dynamicFriction);
				stateChanged = true;
			}

			ImGui::EndMenu();
		}

		const char* buttonLabel = globals::isPaused ? "Continue" : "Pause";

		bool isPaused = globals::isPaused.load();

		// Create the button and check if it's clicked
		if (ImGui::Button(buttonLabel))
		{
			// If clicked, toggle the boolean state
			globals::isPaused.store(!isPaused);
			stateChanged = true;
		}

		float gravityY = globals::gravityY.load();
		buttonLabel = gravityY < 0.0f ? "Reverse" : "Regular";

		// Create the button and check if it's clicked
		if (ImGui::Button(buttonLabel))
		{
			// If clicked, toggle the boolean state
			gravityY *= -1.0f; // Reverse the gravity direction
			globals::gravityY.store(gravityY);
			stateChanged = true;
		}

		buttonLabel = globals::gravityEnabled == 0 ? "Graity ON" : "Gravity OFF";

		// Create the button and check if it's clicked
		if (ImGui::Button(buttonLabel))
		{
			// If clicked, toggle the boolean state
			globals::gravityEnabled = (globals::gravityEnabled == 0) ? 1 : 0;
			stateChanged = true;
		}

		if (stateChanged)
		{
			NetworkManager::getInstance().broadcastGlobalState();
		}

		ImGui::EndMainMenuBar();

	}
}