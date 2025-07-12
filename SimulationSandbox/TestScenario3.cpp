#include "TestScenario3.h"
#include <memory>
#include "Capsule.h"
#include "Sphere.h"
#include "Cube.h"

void TestScenario3::setupFixedObjects()
{
	float radius = 0.5f; // Fixed radius for the capsule and spheres
	float scale = radius * 2.0f;
	//float x = randomFloat(-axisLength, axisLength);
	float x = 0.0f;
	float y = -0.5f;
	float z = 0.0f;
	//float z = randomFloat(-axisLength, axisLength);

	auto fixedCapsule = std::make_unique<PhysicsObject>(
		std::make_unique<Capsule>(
			DirectX::XMFLOAT3(x, y, z),
			DirectX::XMFLOAT3(90.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3(scale, scale, scale)), true, 100.0f);
	fixedCapsule->LoadModel("capsule.sjg");
	ConstantBuffer cb = fixedCapsule->getConstantBuffer();
	cb.LightColour = { x,1 - radius,y, 1.0f };
	cb.DarkColour = cb.LightColour;
	fixedCapsule->setConstantBuffer(cb);
	addPhysicsObject(std::move(fixedCapsule));

	scale = 1.5f;
	auto cube = std::make_unique<PhysicsObject>(
		std::make_unique<Cube>(
			DirectX::XMFLOAT3(x, -globals::AXIS_LENGTH+scale/2.0f, z),
			DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3(scale, scale, scale)),
		true, // isFixed
		100.0f
	);

	cube->LoadModel("cube.sjg");

	cb = cube->getConstantBuffer();
	cb.LightColour = { 1 - radius,1 - radius,radius, 1.0f };
	cb.DarkColour = cb.LightColour;
	cube->setConstantBuffer(cb);

	addPhysicsObject(std::move(cube));

	auto createFixedSphere = [&](float x, float y, float z, bool elevate = false)
		{
			float radius = 0.7f; // Fixed radius for the spheres
			float scale = radius * 2.0f;

			auto sphere = std::make_unique<PhysicsObject>(
				std::make_unique<Sphere>(
					DirectX::XMFLOAT3(x, y, z),
					DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
					DirectX::XMFLOAT3(scale, scale, scale)),
				true, // fixed
				100.0f // mass
			);
			sphere->LoadModel("sphere.sjg");

			cb = sphere->getConstantBuffer();
			cb.LightColour = { 1 - radius, radius, radius, 1.0f };
			cb.DarkColour = cb.LightColour;
			sphere->setConstantBuffer(cb);

			addPhysicsObject(std::move(sphere));
		};

	//createFixedSphere(0.5f, -globals::AXIS_LENGTH, 0.5f, true);
	createFixedSphere(-globals::AXIS_LENGTH, 0.0f, 1.0f);
	createFixedSphere(globals::AXIS_LENGTH, 0.8f, 0.5f);
	createFixedSphere(-0.8f, 0.5f, -globals::AXIS_LENGTH);

	auto sphere = std::make_unique<PhysicsObject>(
		std::make_unique<Sphere>(
			DirectX::XMFLOAT3(x + 0.2f, 2.5f, z),
			DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3(0.2f, 0.2f, 0.2f)), false);
	sphere->LoadModel("sphere.sjg");
	addPhysicsObject(std::move(sphere));

	sphere = std::make_unique<PhysicsObject>(
		std::make_unique<Sphere>(
			DirectX::XMFLOAT3(x - 0.2f, 2.5f, z),
			DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3(0.2f, 0.2f, 0.2f)), false);
	sphere->LoadModel("sphere.sjg");
	addPhysicsObject(std::move(sphere));
}

void TestScenario3::onLoad()
{
	OutputDebugString(L">>>>>>>>>> TestScenario3::onLoad\n");

	initObjects();
	spawnRoom();
	setupFixedObjects();
}

void TestScenario3::onUnload()
{
	OutputDebugString(L">>>>>>>>>> TestScenario3::onUnload\n");

	unloadScenario();
}

void TestScenario3::onUpdate(float dt)
{
	//updateMovement(dt);
}

void TestScenario3::ImGuiMainMenu()
{
	applySharedGUI();
}