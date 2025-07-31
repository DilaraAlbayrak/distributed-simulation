#include "TestScenario4.h"
#include <memory>
#include "Sphere.h"

void TestScenario4::setupFixedObjects()
{
	float radius = 0.4f; // Fixed radius for the capsule and spheres
	float scale = radius * 2.0f;
	//float x = randomFloat(-axisLength, axisLength);
	float x = 0.0f;
	float y = -0.7f;
	float z = 0.0f;
	//float z = randomFloat(-axisLength, axisLength);

	ConstantBuffer cb;

	auto createFixedSphere = [&](float x, float y, float z, bool elevate = false)
		{
			float radius = 0.9f;
			float scale = radius * 2.0f;

			auto sphere = std::make_unique<PhysicsObject>(
				std::make_unique<Sphere>(
					DirectX::XMFLOAT3(x, y, z),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(scale, scale, scale)),
				true, // fixed
				50.0f, // mass
				Material::MAT3 // material type
			);
			sphere->LoadModel("shapes/sphere.sjg");

			cb = sphere->getConstantBuffer();
			cb.LightColour = { 1 - radius, radius, radius, 1.0f };
			cb.DarkColour = cb.LightColour;
			sphere->setConstantBuffer(cb);

			addPhysicsObject(std::move(sphere));
		};

	//createFixedSphere(0.5f, -globals::AXIS_LENGTH, 0.5f, true);
	createFixedSphere(-globals::AXIS_LENGTH, 0.0f, 0.5f);
	createFixedSphere(globals::AXIS_LENGTH, 0.0f, 0.5f);
	//createFixedSphere(-0.8f, 0.5f, -globals::AXIS_LENGTH);

	auto sphere = std::make_unique<PhysicsObject>(
		std::make_unique<Sphere>(
			DirectX::XMFLOAT3(x + 1.3f, 2.5f, z),
			DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3(0.4f, 0.4f, 0.4f)), false, 1.0f, Material::MAT3);
	sphere->setObjectId(0); // Set a unique ID for the sphere
	sphere->setPeerID(0); // Set a peer ID if needed, here we use 0 for simplicity
	sphere->setIsOwned(true); // Mark as owned by the local peer
	sphere->LoadModel("shapes/sphere.sjg");
	addPhysicsObject(std::move(sphere));

	sphere = std::make_unique<PhysicsObject>(
		std::make_unique<Sphere>(
			DirectX::XMFLOAT3(x - 1.3f, 2.5f, z),
			DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3(0.4f, 0.4f, 0.4f)), false, 2.0f, Material::MAT3);
	cb = sphere->getConstantBuffer();
	cb.LightColour = { cb.LightColour.x * 1.5f, cb.LightColour.y * 1.5f, cb.LightColour.z * 1.5f, 1.0f };
	cb.DarkColour = { cb.LightColour.x * 0.5f, cb.LightColour.y * 0.5f, cb.LightColour.z * 0.5f, 1.0f };
	sphere->setConstantBuffer(cb);
	sphere->setObjectId(1); // Set a unique ID for the sphere
	sphere->setPeerID(0); // Set a peer ID if needed, here we use 0 for simplicity
	sphere->setIsOwned(true); // Mark as owned by the local peer
	sphere->LoadModel("shapes/sphere.sjg");
	addPhysicsObject(std::move(sphere));
}

void TestScenario4::onLoad()
{
	OutputDebugString(L">>>>>>>>>> TestScenario4::onLoad\n");

	initObjects();

	initInstancedRendering();

	spawnRoom();

	setupFixedObjects();
}

void TestScenario4::onUnload()
{
	OutputDebugString(L">>>>>>>>>> TestScenario4::onUnload\n");

	unloadScenario();
}

void TestScenario4::onUpdate(float dt)
{
	//updateMovement(dt);
}

void TestScenario4::ImGuiMainMenu()
{
	applySharedGUI();
}