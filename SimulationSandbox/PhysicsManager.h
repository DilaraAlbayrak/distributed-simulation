#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <span>
#include <memory>
#include <shared_mutex>
#include "PhysicsObject.h"

class PhysicsManager
{
private:
	static std::unique_ptr<PhysicsManager> _instance;

	std::vector<std::shared_ptr<PhysicsObject>> _physicsObjects; // not unique_ptr to allow shared ownership
	mutable std::shared_timed_mutex _mutex; // shared mutex for thread-safe access, mutable to allow const access
											// shared_mutex gives error

	std::vector<std::thread> _threads; 
	std::atomic<bool> _running{ false }; // atomic flag to control thread execution

public:
	static PhysicsManager& getInstance()
	{
		if (!_instance)
			_instance = std::make_unique<PhysicsManager>();
		return *_instance;
	}

	void addObject(std::shared_ptr<PhysicsObject> obj);
	void clearObjects();
	void updatePartitioned(int threadIndex, int numThreads, float dt);

	std::span<const std::shared_ptr<PhysicsObject>> accessPhysicsObjects() const;

	void startThreads(int numThreads, float dt);
	void stopThreads();
};