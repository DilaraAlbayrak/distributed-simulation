#include "Scenario1.h"
#include "Sphere.h"
#include <memory>

void Scenario1::setupFixedObjects()
{
	auto createFixedSphere = [&](float x, float y, float z, float radius, bool elevate=false)
	{
		float scale = radius * 2.0f;

		if (elevate) {
			y += scale;
		}

		auto sphere = std::make_unique<PhysicsObject>(
			std::make_unique<Sphere>(
				DirectX::XMFLOAT3(x, y, z),
				DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
				DirectX::XMFLOAT3(scale, scale, scale)),
			true, // fixed
			50.0f, // mass
			Material::MAT2 // material
		);
		sphere->LoadModel("shapes/sphere.sjg");

		ConstantBuffer cb = sphere->getConstantBuffer();
		cb.LightColour = { 1-radius, radius, radius, 1.0f };
		cb.DarkColour = cb.LightColour;
		sphere->setConstantBuffer(cb);

		addPhysicsObject(std::move(sphere));
	};

	//createFixedSphere(0.0f, 0.0f, -3.0f);
	createFixedSphere(0.5f, -globals::AXIS_LENGTH, 0.5f, 0.55f, true);
	createFixedSphere(-globals::AXIS_LENGTH, 0.8f, 1.0f, 0.6f);
	createFixedSphere(globals::AXIS_LENGTH, 0.8f, -1.2f, 0.7f);
	createFixedSphere(-0.8f, 0.5f, -globals::AXIS_LENGTH, 0.65f);
}

void Scenario1::onLoad()
{
	OutputDebugString(L">>>>>>>>>> Scenario1::onLoad\n");

	initObjects();

	initInstancedRendering();

	spawnRoom();

	setupFixedObjects();
	
	generateSpawnData(globals::AXIS_LENGTH);
}

void Scenario1::onUnload()
{
	OutputDebugString(L">>>>>>>>>> Scenario1::onUnload\n");

	unloadScenario();
}

void Scenario1::onUpdate(float dt)
{
	//updateMovement(dt);
}

void Scenario1::ImGuiMainMenu()
{
	applySharedGUI();
}