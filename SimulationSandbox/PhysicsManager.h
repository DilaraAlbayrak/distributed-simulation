#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <span>
#include <memory>
#include <shared_mutex>
#include <functional> // Required for std::function
#include "PhysicsObject.h"

class PhysicsManager
{
private:
	static std::unique_ptr<PhysicsManager> _instance;

	std::vector<std::shared_ptr<PhysicsObject>> _physicsObjects;
	mutable std::shared_mutex _objectsMutex;

	std::vector<std::thread> _threads;
	std::atomic<bool> _running{ false };

	void simulationLoop(int threadIndex, int numThreads, float dt);

public:
	PhysicsManager() = default;
	~PhysicsManager();

	static PhysicsManager& getInstance();

	void addObject(std::shared_ptr<PhysicsObject> obj);
	void clearObjects();

	// It acquires a lock and then executes the provided function, passing the object list to it.
	// This guarantees that the list is accessed only while the lock is held.
	void accessPhysicsObjects(const std::function<void(std::span<const std::shared_ptr<PhysicsObject>>)>& accessor) const;

	void startThreads(int numThreads, float dt);
	void stopThreads();
};
