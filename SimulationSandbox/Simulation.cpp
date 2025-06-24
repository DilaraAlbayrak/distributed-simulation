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
	while (WM_QUIT != msg.message) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			render.render();

			if (!threadingEnabled && render.getScenario())
			{
				// Start physics threads if not already started
				unsigned int totalCores = std::thread::hardware_concurrency();
				unsigned int simThreadCount = (totalCores > 3) ? totalCores - 3 : 1;
				//std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow time for the window to initialize
				physicsManager.startThreads(simThreadCount, 0.008f);
				
				// Start network threads if not already started
				threadingEnabled = true;
			}
		}
	}

	return static_cast<int>(msg.wParam);
}