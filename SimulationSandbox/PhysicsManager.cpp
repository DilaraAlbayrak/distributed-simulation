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

// read-only access to the physics objects
void PhysicsManager::accessPhysicsObjects(const std::function<void(std::span<const std::shared_ptr<PhysicsObject>>)>& accessor) const
{
	std::shared_lock lock(_objectsMutex);
	accessor(std::span<const std::shared_ptr<PhysicsObject>>(_physicsObjects));
}

void PhysicsManager::simulationLoop(int threadIndex, int numThreads, float dt)
{
	// Use the safe accessor to work with the objects.
	// This ensures the list is not modified while we are using it.
	accessPhysicsObjects([&](std::span<const std::shared_ptr<PhysicsObject>> objects) {

		const size_t totalObjects = objects.size();
		if (totalObjects == 0) return;

		// Calculate the range of objects this thread is responsible for.
		const size_t objectsPerThread = (totalObjects + numThreads - 1) / numThreads;
		const size_t startIndex = threadIndex * objectsPerThread;
		const size_t endIndex = std::min(startIndex + objectsPerThread, totalObjects);

		// Update positions
		for (size_t i = startIndex; i < endIndex; ++i)
		{
			// The objects span is const, but the PhysicsObject itself is not.
			// We can call non-const methods on it.
			objects[i]->Update(dt);
		}

		// Check for collisions
		for (size_t i = startIndex; i < endIndex; ++i)
		{
			// We check collisions against all subsequent objects, even those
			// managed by other threads, to ensure all pairs are checked once.
			for (size_t j = i + 1; j < totalObjects; ++j)
			{
				auto& objA = objects[i];
				auto& objB = objects[j];

				// This check is now safe because we are guaranteed that
				// objA and objB are valid pointers inside the locked span.
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
		}); 
}

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