#include "Scenario1.h"
#include "Sphere.h"
#include <memory>

void Scenario1::setupFixedObjects()
{
	auto createFixedSphere = [&](float x, float y, float z, bool elevate=false)
	{
		float radius = Scenario::randomFloat(0.5f, 0.8f);
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
			1.0f, // mass
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
	createFixedSphere(0.5f, -globals::AXIS_LENGTH, 0.5f, true);
	createFixedSphere(-globals::AXIS_LENGTH, 0.8f, 1.0f);
	createFixedSphere(globals::AXIS_LENGTH, 0.8f, -1.2f);
	createFixedSphere(-0.8f, 0.5f, -globals::AXIS_LENGTH);
}

void Scenario1::onLoad()
{
	OutputDebugString(L">>>>>>>>>> Scenario1::onLoad\n");

	initObjects();
	spawnRoom();

	setupFixedObjects();
	OutputDebugString(L">>>>>>>>>> Scenario1::setupFixedObjects\n");

	setNumMovingSpheres(100);
	OutputDebugString(L">>>>>>>>>> Scenario1::setNumMovingSpheres\n");
	setMinRadius(0.1f);
	OutputDebugString(L">>>>>>>>>> Scenario1::setMinRadius\n");
	setMaxRadius(0.1f);
	OutputDebugString(L">>>>>>>>>> Scenario1::setMaxRadius\n");
	generateSpawnData(globals::AXIS_LENGTH);
	OutputDebugString(L">>>>>>>>>> Scenario1::generateSpawnData\n");

	OutputDebugString(L">>>>>>>>>> Scenario1::initObjects\n");
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