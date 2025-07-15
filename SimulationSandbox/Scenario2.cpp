#include "Scenario2.h"
#include "Capsule.h"
#include "Sphere.h"
#include "Cube.h"
#include <memory>

void Scenario2::setupFixedObjects()
{
	// This lambda creates a capsule object that is fixed in place.
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
			cb.LightColour = { abs(1.0f - radius/3.0f), abs(1.0f - radius / 2.0f), abs(1.0f - radius / 3.0f), 1.0f};
			cb.DarkColour = cb.LightColour;
			capsule->setConstantBuffer(cb);

			addPhysicsObject(std::move(capsule));
		};

	float radius;

	// This capsule is on the floor (a y-axis wall).
	// Its origin is placed exactly on the wall plane to appear halfway through.
	radius = Scenario::randomFloat(0.7f, 1.3f);
	createFixedCapsule(
		{ 0.0f, -globals::AXIS_LENGTH, 0.0f }, // Positioned directly on the floor plane.
		{ 90.0f, 0.0f, 0.0f }, // Rotated to lie along the world z-axis.
		radius
	);

	// This capsule is on the left wall (an x-axis wall).
	radius = Scenario::randomFloat(0.7f, 1.3f);
	createFixedCapsule(
		{ -globals::AXIS_LENGTH, 0.8f, 1.0f }, // Positioned directly on the left wall plane.
		{ 0.0f, 0.0f, 90.0f }, // Rotated to lie along the world x-axis.
		radius
	);

	// This capsule is on the right wall (an x-axis wall).
	radius = Scenario::randomFloat(0.7f, 1.3f);
	createFixedCapsule(
		{ globals::AXIS_LENGTH, 0.8f, -1.2f }, // Positioned directly on the right wall plane.
		{ 0.0f, 0.0f, 90.0f }, // Rotated to lie along the world x-axis.
		radius
	);

	// This capsule is on the back wall (a z-axis wall).
	radius = Scenario::randomFloat(0.7f, 1.3f);
	createFixedCapsule(
		{ -0.8f, 0.5f, -globals::AXIS_LENGTH }, // Positioned directly on the back wall plane.
		{ 90.0f, 0.0f, 0.0f }, // Rotated to lie along the world z-axis.
		radius
	);
}

void Scenario2::onLoad()
{
	OutputDebugString(L">>>>>>>>>> Scenario2::onLoad\n");

	initObjects();

	spawnRoom();

	setupFixedObjects();

	setNumMovingSpheres(100);
	setMinRadius(0.1f);
	setMaxRadius(0.1f);
	generateSpawnData(globals::AXIS_LENGTH);
}

void Scenario2::onUnload()
{
	OutputDebugString(L">>>>>>>>>> Scenario2::onUnload\n");

	unloadScenario();
}

void Scenario2::onUpdate(float dt)
{
	//updateMovement(dt);
}

void Scenario2::ImGuiMainMenu()
{
	applySharedGUI();
}