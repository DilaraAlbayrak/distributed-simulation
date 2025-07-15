#include "D3DFramework.h"
#include "PhysicsManager.h"
#include "NetworkManager.h"
#include <chrono>
#include <thread>

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nShowCmd) {

	auto& render = D3DFramework::getInstance();
	auto& physicsManager = PhysicsManager::getInstance();
	auto& networkManager = NetworkManager::getInstance();

	bool threadingEnabled = false; 

	if (FAILED(render.initWindow(hInstance, nShowCmd)))
		return 0;

	if (FAILED(render.initDevice()))
		return 0;

	// Main message loop
	MSG msg;
	msg.message = 0;
	while (WM_QUIT != msg.message) // yoda condition
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			render.render();

			if (!threadingEnabled && render.isScenarioReady())
			{
				unsigned int totalCores = std::thread::hardware_concurrency();
				unsigned int simThreadCount = (totalCores > 3) ? totalCores - 3 : 1;
				physicsManager.startThreads(simThreadCount, 0.008f);
				networkManager.startNetworking();

				threadingEnabled = true;
			}
			else if (threadingEnabled && !render.isScenarioReady())
			{
				physicsManager.stopThreads();
				networkManager.stopNetworking();
				threadingEnabled = false;
			}
		}
	}
	if (threadingEnabled)
	{
		physicsManager.stopThreads();
		networkManager.stopNetworking();
	}

	return static_cast<int>(msg.wParam);
}