#include "NetworkManager.h"
#include "PhysicsManager.h"
#include <algorithm>
#include <chrono>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "globals.h"

std::unique_ptr<PhysicsManager> PhysicsManager::_instance = nullptr;

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
	if (_running.load())
	{
		stopThreads();
	}
}

void PhysicsManager::addObject(std::shared_ptr<PhysicsObject> obj)
{
	if (!obj) return;

	// Add the object to the ID map for network lookups
	int objId = obj->getObjectId();
	if (objId != -1) {
		std::unique_lock lock(_mapMutex);
		_objectIDMap[objId] = obj;
	}

	std::unique_lock lock(_objectsMutex);
	if (obj->getFixed())
	{
		_fixedObjects.push_back(std::move(obj));
	}
	else
	{
		_movingObjects.push_back(std::move(obj));
	}
}

void PhysicsManager::clearObjects()
{
	std::unique_lock mapLock(_mapMutex);
	_objectIDMap.clear();
	mapLock.unlock();

	std::unique_lock lock(_objectsMutex);
	_movingObjects.clear();
	_fixedObjects.clear();

	if (!_grid.empty())
	{
		for (auto& cell : _grid) { cell.clear(); }
	}
	if (!_threadMovingPairs.empty())
	{
		for (auto& pair_list : _threadMovingPairs) { pair_list.clear(); }
	}
	if (!_threadFixedPairs.empty())
	{
		for (auto& pair_list : _threadFixedPairs) { pair_list.clear(); }
	}
	if (!_threadCollisionPairs.empty()) { for (auto& pair_list : _threadCollisionPairs) pair_list.clear(); }

	_threadCollisionPairs.clear();
	_allCollisionPairs.clear();
	_allMovingPairs.clear();
	_allFixedPairs.clear();
}

void PhysicsManager::accessAllObjects(const std::function<void(
	std::span<const std::shared_ptr<PhysicsObject>>,
	std::span<const std::shared_ptr<PhysicsObject>>
	)>& accessor) const
{
	std::shared_lock lock(_objectsMutex);
	accessor(_movingObjects, _fixedObjects);
}

void PhysicsManager::initGrid(float cellSize, const DirectX::XMFLOAT3& worldMin, const DirectX::XMFLOAT3& worldMax)
{
	_cellSize = cellSize;
	_worldMin = worldMin;
	_worldMax = worldMax;
	DirectX::XMFLOAT3 worldSize = { worldMax.x - worldMin.x, worldMax.y - worldMin.y, worldMax.z - worldMin.z };
	_gridCellsX = static_cast<int>(ceil(worldSize.x / _cellSize));
	_gridCellsY = static_cast<int>(ceil(worldSize.y / _cellSize));
	_gridCellsZ = static_cast<int>(ceil(worldSize.z / _cellSize));
	size_t totalCells = (size_t)_gridCellsX * _gridCellsY * _gridCellsZ;
	_grid.assign(totalCells, std::vector<int>());
	_gridMutexes = std::vector<std::mutex>(totalCells);
}

int PhysicsManager::getGridIndex(const DirectX::XMFLOAT3& position) const
{
	float x = std::clamp(position.x, _worldMin.x, _worldMax.x - 0.001f);
	float y = std::clamp(position.y, _worldMin.y, _worldMax.y - 0.001f);
	float z = std::clamp(position.z, _worldMin.z, _worldMax.z - 0.001f);
	int cellX = static_cast<int>((x - _worldMin.x) / _cellSize);
	int cellY = static_cast<int>((y - _worldMin.y) / _cellSize);
	int cellZ = static_cast<int>((z - _worldMin.z) / _cellSize);
	return cellX + cellY * _gridCellsX + cellZ * _gridCellsX * _gridCellsY;
}

void PhysicsManager::simulationLoop(int threadIndex, int numThreads, float dt)
{
	//if (globals::isPaused) return; // Skip simulation if paused

	static std::chrono::high_resolution_clock::time_point _lastSimTime;

	auto frameStart = std::chrono::high_resolution_clock::now();

	// --- Phase 0: Create a local, thread-safe copy of the data ---
	// By copying the shared_ptrs, we ensure that even if the main thread
	// modifies the original vectors, our pointers for this simulation step remain valid.
	std::vector<std::shared_ptr<PhysicsObject>> localMovingObjects;
	std::vector<std::shared_ptr<PhysicsObject>> localFixedObjects;
	{
		std::shared_lock lock(_objectsMutex);
		localMovingObjects = _movingObjects;
		localFixedObjects = _fixedObjects;
	} // The read lock is released immediately after copying.

	const size_t numMovingObjects = localMovingObjects.size();
	//if (numMovingObjects == 0) return;
	// Ensure all threads reach the barrier even if there are no objects.
	// If any thread exits early without hitting all arrive_and_wait points,
	// it will cause deadlocks for the others waiting at those points.
	if (numMovingObjects == 0)
	{
		_syncBarrier->arrive_and_wait();
		_syncBarrier->arrive_and_wait();
		_syncBarrier->arrive_and_wait();
		_syncBarrier->arrive_and_wait();
		return; // No objects to process, exit early.
	}

	// Calculate the workload for this thread.
	size_t objectsPerThread = (numMovingObjects + numThreads - 1) / numThreads;
	size_t startIndex = threadIndex * objectsPerThread;
	size_t endIndex = std::min(startIndex + objectsPerThread, numMovingObjects);

	// --- Phase 1: Clear and Populate the Grid (Parallel) ---
	size_t totalCells = _grid.size();
	size_t cellsPerThread = (totalCells + numThreads - 1) / numThreads;
	size_t startCell = threadIndex * cellsPerThread;
	size_t endCell = std::min(startCell + cellsPerThread, totalCells);
	for (size_t i = startCell; i < endCell; ++i)
	{
		_grid[i].clear();
	}
	_syncBarrier->arrive_and_wait();

	for (size_t i = startIndex; i < endIndex; ++i)
	{
		const auto& obj = localMovingObjects[i];
		if (obj)
		{
			int gridIndex = getGridIndex(obj->getPosition());
			if (gridIndex >= 0 && gridIndex < _grid.size())
			{
				std::lock_guard<std::mutex> cellLock(_gridMutexes[gridIndex]);
				_grid[gridIndex].push_back(i);
			}
		}
	}
	_syncBarrier->arrive_and_wait();

	// --- Phase 2: Detect ALL Collisions (Parallel) ---
	_threadCollisionPairs[threadIndex].clear();

	for (size_t i = startIndex; i < endIndex; ++i)
	{
		PhysicsObject* objA = localMovingObjects[i].get();
		if (!objA) continue;

 		//DirectX::XMFLOAT3 posA = objA->getPosition();
		DirectX::XMFLOAT3 posA = objA->isOwned() ? objA->getPosition() : objA->getSmoothedPosition(dt);
		int centerCellX = static_cast<int>((posA.x - _worldMin.x) / _cellSize);
		int centerCellY = static_cast<int>((posA.y - _worldMin.y) / _cellSize);
		int centerCellZ = static_cast<int>((posA.z - _worldMin.z) / _cellSize);

		// Detect Moving vs Moving
		for (int z = -1; z <= 1; ++z) for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x)
		{
			int currentCellX = centerCellX + x, currentCellY = centerCellY + y, currentCellZ = centerCellZ + z;
			if (currentCellX >= 0 && currentCellX < _gridCellsX && currentCellY >= 0 && currentCellY < _gridCellsY && currentCellZ >= 0 && currentCellZ < _gridCellsZ)
			{
				int gridIndex = currentCellX + currentCellY * _gridCellsX + currentCellZ * _gridCellsX * _gridCellsY;
				for (int j_idx : _grid[gridIndex])
				{
					if (j_idx >= localMovingObjects.size()) continue;

					if (i < j_idx)
					{
						PhysicsObject* objB = localMovingObjects[j_idx].get();
						if (!objB) continue;

						DirectX::XMFLOAT3 normal;
						float penetration = 0.0f;
						if (objA->checkCollision(*objB, normal, penetration))
						{
							_threadCollisionPairs[threadIndex].push_back({ objA, objB });
						}
					}
				}
			}
		}

		// Detect Moving vs Fixed
		for (const auto& fixed_ptr : localFixedObjects)
		{
			PhysicsObject* objB = fixed_ptr.get();
			if (!objB) continue;
			DirectX::XMFLOAT3 normal;
			float penetration = 0.0f;
			if (objA->checkCollision(*objB, normal, penetration))
			{
				_threadCollisionPairs[threadIndex].push_back({ objA, objB });
			}
		}
	}
	_syncBarrier->arrive_and_wait();

	// --- Phase 3: Resolve ALL Collisions (Single Thread) ---
	if (threadIndex == 0)
	{
		_allCollisionPairs.clear();
		for (const auto& thread_list : _threadCollisionPairs)
		{
			_allCollisionPairs.insert(_allCollisionPairs.end(), thread_list.begin(), thread_list.end());
		}

		for (const auto& pair : _allCollisionPairs)
		{
			DirectX::XMFLOAT3 normal;
			float penetration = 0.0f;

			if (pair.objA->checkCollision(*pair.objB, normal, penetration))
			{
				//OutputDebugString(L"[DEBUG] COLLISION \n");

				if (pair.objA->isOwned())
					pair.objA->resolveCollision(*pair.objB, normal, penetration);

				if (pair.objB->isOwned())
				{
					// Invert the normal for symmetric resolution
					DirectX::XMFLOAT3 inverseNormal = { -normal.x, -normal.y, -normal.z };
					pair.objB->resolveCollision(*pair.objA, inverseNormal, penetration);
				}
			}
		}
	}
	_syncBarrier->arrive_and_wait();

	auto& networkManager = NetworkManager::getInstance();
	int localPeerId = networkManager.getLocalPeerId();

	// --- Phase 4: Update Physics State (Parallel) ---
	for (size_t i = startIndex; i < endIndex; ++i)
	{
		if (const auto& obj = localMovingObjects[i])
		{
			/*obj->Update(dt);
			obj->constrainToBounds();*/
			if (obj->getPeerID() == localPeerId)
			//if (!globals::isPaused.load() && obj->getPeerID() == localPeerId)
			{
				// We own this object, so we calculate its physics.
				obj->Update(dt);
				obj->constrainToBounds();

				// After updating, broadcast the new state to other peers.
				networkManager.sendObjectUpdate(obj->getObjectId(), obj->getPosition(), obj->getRotation(), obj->getVelocity(), obj->getScale());
			}
		}
	}

	if (threadIndex == 0) {
		auto now = std::chrono::high_resolution_clock::now();
		float elapsedMs = std::chrono::duration<float, std::milli>(now - _lastSimTime).count();

		if (_lastSimTime.time_since_epoch().count() != 0 && elapsedMs > 0.0f) {
			float actualHz = 1000.0f / elapsedMs;
			globals::actualSimFrequencyHz.store(actualHz);
		}
		_lastSimTime = now;
	}
}

void PhysicsManager::startThreads(int numThreads, float dt)
{
	if (_running.load()) return;

	// stopThreads();

	// Clean up inconsistent state from previous attempts
	_threadCollisionPairs.resize(numThreads);
	_threadMovingPairs.resize(numThreads);
	_threadFixedPairs.resize(numThreads);

	_allCollisionPairs.clear();

	_threadCollisionPairs.resize(numThreads);
	int maxNumObjects = 10000; // 10k is fixed
	_allCollisionPairs.reserve(maxNumObjects * 5); // 10k is fixed

	_running = true;
	_threads.reserve(numThreads);

	float worldExtent = globals::AXIS_LENGTH;
	initGrid(0.5f, { -worldExtent, -worldExtent, -worldExtent }, { worldExtent, worldExtent, worldExtent });

	if (numThreads > 0)
	{
		_syncBarrier = std::make_unique<std::barrier<>>(numThreads);
	}

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
				if (globals::isPaused) continue;

				auto startTime = std::chrono::high_resolution_clock::now();

				simulationLoop(i, numThreads, dt);

				auto endTime = std::chrono::high_resolution_clock::now();
				auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
				auto targetTime = std::chrono::microseconds(static_cast<long long>(dt * 1'000'000));

				if (elapsedTime < targetTime)
				{
					// This is the single, correct wait.
					// It waits for the remaining time OR until stopThreads notifies it.
					std::unique_lock<std::mutex> lock(_runMutex);
					_runCondition.wait_for(lock, targetTime - elapsedTime, [this] { return !_running; });
				}
			}
			});
	}
}

void PhysicsManager::stopThreads()
{
	if (!_running.load()) return;

	{
		std::lock_guard<std::mutex> lock(_runMutex);
		_running = false; // Set running to false under a lock
	}
	_runCondition.notify_all(); // Notify all waiting threads that it's time to check the flag and exit

	for (auto& thread : _threads)
	{
		if (thread.joinable())
		{
			thread.join(); // This will now return promptly.
		}
	}
	_threads.clear();
}

std::shared_ptr<PhysicsObject> PhysicsManager::getObjectById(int objectId)
{
	std::shared_lock lock(_mapMutex);
	auto it = _objectIDMap.find(objectId);
	if (it != _objectIDMap.end()) {
		return it->second;
	}
	return nullptr;
}

void PhysicsManager::updateObjectState(int objectId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT3& scale)
{
	auto obj = getObjectById(objectId);
	if (obj) {
		obj->setNetworkState(position, rotation, velocity, scale);
	}
}
