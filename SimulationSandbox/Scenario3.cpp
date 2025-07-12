#include "Scenario3.h"
#include "NotImplementedException.h"
#include <memory>

void Scenario3::setupFixedObjects()  
{  
   throw NotImplementedException("no implementation setupFixedObjects() for Scenario3");  
}

void Scenario3::onLoad()
{
	OutputDebugString(L">>>>>>>>>> Scenario3::onLoad\n");

	initObjects();

	spawnRoom();

	setNumMovingSpheres(5000);
	setMinRadius(0.01f);
	setMaxRadius(0.03f);
	generateSpawnData(globals::AXIS_LENGTH);
}

void Scenario3::onUnload()
{
	OutputDebugString(L">>>>>>>>>> Scenario3::onUnload\n");

	unloadScenario();
}

void Scenario3::onUpdate(float dt)
{
	//updateMovement(dt);
}

void Scenario3::ImGuiMainMenu()
{
	applySharedGUI();
}