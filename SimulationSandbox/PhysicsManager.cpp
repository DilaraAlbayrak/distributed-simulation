#include "PhysicsManager.h"
#include <algorithm>
#include <chrono>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// --- Singleton and Constructor/Destructor ---
std::unique_ptr<PhysicsManager> PhysicsManager::_instance;

PhysicsManager& PhysicsManager::getInstance()
{
	if (!_instance)
	{
		_instance = std::make_unique<PhysicsManager>();
	}
	return *_instance;
}

PhysicsManager::~PhysicsManager()
{
	if (_running)
	{
		stopThreads();
	}
}

// --- Object Management ---
void PhysicsManager::addObject(std::shared_ptr<PhysicsObject> obj)
{
	std::unique_lock lock(_objectsMutex);
	_physicsObjects.push_back(std::move(obj));
}

void PhysicsManager::clearObjects()
{
	std::unique_lock lock(_objectsMutex);
	_physicsObjects.clear();
}

/**
 * @brief Provides safe, read-only access to the physics objects.
 * It acquires a shared lock and then invokes the provided 'accessor' function,
 * passing a span of the objects. The lock is released automatically when this function returns.
 * @param accessor A function (usually a lambda) that takes a span of objects and performs an action (e.g., rendering).
 */
void PhysicsManager::accessPhysicsObjects(const std::function<void(std::span<const std::shared_ptr<PhysicsObject>>)>& accessor) const
{
	std::shared_lock lock(_objectsMutex);
	accessor(std::span<const std::shared_ptr<PhysicsObject>>(_physicsObjects));
}


// --- Simulation Loop (No changes needed here from last time) ---
void PhysicsManager::simulationLoop(int threadIndex, int numThreads, float dt)
{
	// Get object count once
	_objectsMutex.lock_shared();
	const size_t totalObjects = _physicsObjects.size();
	_objectsMutex.unlock_shared();

	if (totalObjects == 0) return;

	const size_t objectsPerThread = (totalObjects + numThreads - 1) / numThreads;
	const size_t startIndex = threadIndex * objectsPerThread;
	const size_t endIndex = std::min(startIndex + objectsPerThread, totalObjects);

	for (size_t i = startIndex; i < endIndex; ++i)
	{
		_physicsObjects[i]->Update(dt);
	}

	for (size_t i = startIndex; i < endIndex; ++i)
	{
		for (size_t j = i + 1; j < totalObjects; ++j)
		{
			auto& objA = _physicsObjects[i];
			auto& objB = _physicsObjects[j];

			if (objA->getFixed() && objB->getFixed()) continue;

			DirectX::XMFLOAT3 normal;
			float penetration = 0.0f;
			if (objA->checkCollision(*objB, normal, penetration))
			{
				std::mutex* p_m1 = &objA->getCollisionMutex();
				std::mutex* p_m2 = &objB->getCollisionMutex();
				if (p_m1 > p_m2) std::swap(p_m1, p_m2);

				std::scoped_lock lock(*p_m1, *p_m2);
				objA->resolveCollision(*objB, normal, penetration);
			}
		}
	}
}

// --- Thread Management (No changes needed here from last time) ---
void PhysicsManager::startThreads(int numThreads, float dt)
{
	if (_running) return;

	_running = true;
	_threads.reserve(numThreads);

	for (int i = 0; i < numThreads; ++i)
	{
		_threads.emplace_back([this, i, numThreads, dt]() {
			const int coreIndex = 3 + i;
			DWORD_PTR mask = 1ULL << coreIndex;
			if (SetThreadAffinityMask(GetCurrentThread(), mask) == 0)
			{
				OutputDebugString(L"[WARNING] Failed to set thread affinity.\n");
			}

			while (_running)
			{
				auto startTime = std::chrono::high_resolution_clock::now();
				simulationLoop(i, numThreads, dt);
				auto endTime = std::chrono::high_resolution_clock::now();
				auto elapsedTime = std::chrono::duration_cast<std::chrono::duration<float>>(endTime - startTime);

				if (elapsedTime.count() < dt)
				{
					std::this_thread::sleep_for(std::chrono::duration<float>(dt - elapsedTime.count()));
				}
			}
			});
	}
}

void PhysicsManager::stopThreads()
{
	if (!_running) return;

	_running = false;
	for (auto& thread : _threads)
	{
		if (thread.joinable())
		{
			thread.join();
		}
	}
	_threads.clear();
}