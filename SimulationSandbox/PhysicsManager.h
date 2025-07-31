#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <span>
#include <memory>
#include <shared_mutex>
#include <functional>
#include <barrier>
#include <utility>
#include "PhysicsObject.h"

struct CollisionPair
{
	PhysicsObject* objA;
	PhysicsObject* objB;
};

class PhysicsManager
{
private:
	static std::unique_ptr<PhysicsManager> _instance;

	// --- Object Lists ---
	std::vector<std::shared_ptr<PhysicsObject>> _movingObjects;
	std::vector<std::shared_ptr<PhysicsObject>> _fixedObjects;

	std::vector<std::vector<CollisionPair>> _threadCollisionPairs;
	std::vector<CollisionPair> _allCollisionPairs;

	// for object lookup by ID
	std::unordered_map<int, std::shared_ptr<PhysicsObject>> _objectIDMap;
	mutable std::shared_mutex _mapMutex;

	// --- Spatial Grid Members ---
	std::vector<std::vector<int>> _grid;
	std::vector<std::mutex> _gridMutexes;
	int _gridCellsX = 0;
	int _gridCellsY = 0;
	int _gridCellsZ = 0;
	float _cellSize = 0.0f;
	DirectX::XMFLOAT3 _worldMin;
	DirectX::XMFLOAT3 _worldMax;

	// --- Threading and Synchronization ---
	mutable std::shared_mutex _objectsMutex;
	std::vector<std::thread> _threads;
	std::atomic<bool> _running{ false };
	std::unique_ptr<std::barrier<>> _syncBarrier;

	//std::chrono::high_resolution_clock::time_point _lastSimTime;

	// For clean thread shutdown (transition between scenarios)
	std::mutex _runMutex;
	std::condition_variable _runCondition;

	// --- Collision Pair Storage ---
	std::vector<std::vector<std::pair<int, int>>> _threadMovingPairs;
	std::vector<std::vector<std::pair<int, int>>> _threadFixedPairs;
	std::vector<std::pair<int, int>> _allMovingPairs;
	std::vector<std::pair<int, int>> _allFixedPairs;

	// --- Private Methods ---
	void initGrid(float cellSize, const DirectX::XMFLOAT3& worldMin, const DirectX::XMFLOAT3& worldMax);
	int getGridIndex(const DirectX::XMFLOAT3& position) const;
	void simulationLoop(int threadIndex, int numThreads, float dt);

public:
	PhysicsManager()
	{
		_worldMin = { -globals::AXIS_LENGTH, -globals::AXIS_LENGTH, -globals::AXIS_LENGTH };
		_worldMax = { globals::AXIS_LENGTH, globals::AXIS_LENGTH, globals::AXIS_LENGTH };
	}
	~PhysicsManager();

	PhysicsManager(const PhysicsManager&) = delete;
	PhysicsManager& operator=(const PhysicsManager&) = delete;
	PhysicsManager(PhysicsManager&&) = delete;
	PhysicsManager& operator=(PhysicsManager&&) = delete;

	static PhysicsManager& getInstance();

	void addObject(std::shared_ptr<PhysicsObject> obj);
	void clearObjects();

	void accessAllObjects(const std::function<void(
		std::span<const std::shared_ptr<PhysicsObject>>,
		std::span<const std::shared_ptr<PhysicsObject>>
		)>& accessor) const;

	void startThreads(int numThreads, float dt);
	void stopThreads();

	bool isRunning() const { return _running.load(); }

	std::shared_ptr<PhysicsObject> getObjectById(int objectId);
	void updateObjectState(int objectId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT3& scale);
};