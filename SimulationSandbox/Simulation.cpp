// In Simulation.cpp
#include "D3DFramework.h"
#include "NetworkManager.h"
#include <thread>

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nShowCmd) {

	auto& render = D3DFramework::getInstance();
	auto& networkManager = NetworkManager::getInstance();

	if (FAILED(render.initWindow(hInstance, nShowCmd))) return 0;
	if (FAILED(render.initDevice())) return 0;

	// Start networking just once when the application starts.
	//networkManager.startNetworking();

	MSG msg = {};
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// The main loop's only jobs are to process network commands and render.
			networkManager.processMainThreadCommands();
			render.render();
		}
	}

	// Clean up all systems when the application is closing.
	PhysicsManager::getInstance().stopThreads();
	networkManager.stopNetworking();

	return static_cast<int>(msg.wParam);
}