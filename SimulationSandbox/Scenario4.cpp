#include "Scenario4.h"
#include <memory>
#include "Capsule.h"
#include "Sphere.h"

void Scenario4::setupFixedObjects()
{
	auto createFixedCapsule = [&](const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& rotation,
		float radius)
		{
			// The scale vector should be proportional to the desired radius.
			// Our base model (capsule.sjg) has a radius of 1.0, so we scale all axes by the radius
			// to maintain the model's proportions.
			DirectX::XMFLOAT3 scaleVec = { radius, radius, radius };

			auto capsule = std::make_unique<PhysicsObject>(
				std::make_unique<Capsule>(
					position,
					rotation,
					scaleVec),
				true, // isFixed
				50.0f, // mass
				Material::MAT2 // material
			);

			capsule->LoadModel("shapes/capsule.sjg");

			ConstantBuffer cb = capsule->getConstantBuffer();
			cb.LightColour = { abs(1.0f - radius / 3.0f), abs(1.0f - radius / 2.0f), abs(1.0f - radius / 3.0f), 1.0f };
			cb.DarkColour = cb.LightColour;
			capsule->setConstantBuffer(cb);

			addPhysicsObject(std::move(capsule));
		};

	float radius;

	// This capsule is on the floor (a y-axis wall).
	// Its origin is placed exactly on the wall plane to appear halfway through.
	//radius = Scenario::randomFloat(0.7f, 1.3f);
	radius = 1.0f;
	createFixedCapsule(
		{ 0.0f, -globals::AXIS_LENGTH + radius, 0.0f }, // Positioned directly on the floor plane.
		{ 90.0f, 0.0f, 0.0f }, // Rotated to lie along the world z-axis.
		radius
	);

	// This capsule is on the left wall (an x-axis wall).
	//radius = Scenario::randomFloat(0.7f, 1.3f);
	radius = 0.8f;
	createFixedCapsule(
		{ -globals::AXIS_LENGTH, 0.8f, 1.0f }, // Positioned directly on the left wall plane.
		{ 0.0f, 0.0f, 90.0f }, // Rotated to lie along the world x-axis.
		radius
	);

	auto createFixedSphere = [&](float x, float y, float z, float radius = 0.6f, bool elevate = false)
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
			cb.LightColour = { 1 - radius, radius, radius, 1.0f };
			cb.DarkColour = cb.LightColour;
			sphere->setConstantBuffer(cb);

			addPhysicsObject(std::move(sphere));
		};

	createFixedSphere(globals::AXIS_LENGTH, 0.8f, -1.2f, 0.7f);
	createFixedSphere(-0.8f, 0.5f, -globals::AXIS_LENGTH);
}

void Scenario4::onLoad()
{
	OutputDebugString(L">>>>>>>>>> Scenario4::onLoad\n");

	initObjects();

	initInstancedRendering();

	spawnRoom();

	setupFixedObjects();

	generateSpawnData(globals::AXIS_LENGTH);
}

void Scenario4::onUnload()
{
	OutputDebugString(L">>>>>>>>>> Scenario4::onUnload\n");

	unloadScenario();
}

void Scenario4::onUpdate(float dt)
{
	//updateMovement(dt);
}

void Scenario4::ImGuiMainMenu()
{
	applySharedGUI();
}