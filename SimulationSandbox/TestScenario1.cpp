#include "TestScenario1.h"
#include <memory>
#include "Sphere.h"

void TestScenario1::setupFixedObjects()
{
	float radius = 0.65f;
	float scale = radius * 2.0f;
	//float x = randomFloat(-axisLength, axisLength);
	float x = 0.0f;
	float y = -globals::AXIS_LENGTH + scale;
	float z = 0.0f;
	//float z = randomFloat(-axisLength, axisLength);

	auto fixedSphere = std::make_unique<PhysicsObject>(
		std::make_unique<Sphere>(
			DirectX::XMFLOAT3(x, y, z),
			DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3(scale, scale, scale)), true, 50.0f, Material::MAT2);
	fixedSphere->LoadModel("shapes/sphere.sjg");
	ConstantBuffer cb = fixedSphere->getConstantBuffer();
	cb.LightColour = { 1 - radius,radius,radius, 1.0f };
	cb.DarkColour = cb.LightColour;
	fixedSphere->setConstantBuffer(cb);
	addPhysicsObject(std::move(fixedSphere));

	auto sphere = std::make_unique<PhysicsObject>(
		std::make_unique<Sphere>(
			DirectX::XMFLOAT3(x+0.2f, 2.5f, z),
			DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3(0.2f, 0.2f, 0.2f)), false);
	sphere->setObjectId(0); // Set a unique ID for the sphere
	sphere->setPeerID(0); // Set a peer ID if needed, here we use 0 for simplicity
	sphere->setIsOwned(true); // Mark as owned by the local peer
	sphere->LoadModel("shapes/sphere.sjg");
	addPhysicsObject(std::move(sphere));
}

void TestScenario1::onLoad()
{
	OutputDebugString(L">>>>>>>>>> TestScenario1::onLoad\n");

	initObjects();

	initInstancedRendering();

	spawnRoom();

	setupFixedObjects();

	//generateSpawnData(globals::AXIS_LENGTH);
}

void TestScenario1::onUnload()
{
	OutputDebugString(L">>>>>>>>>> TestScenario1::onUnload\n");

	unloadScenario();
}

void TestScenario1::onUpdate(float dt)
{
	//updateMovement(dt);
}

void TestScenario1::ImGuiMainMenu()
{
	applySharedGUI();
}