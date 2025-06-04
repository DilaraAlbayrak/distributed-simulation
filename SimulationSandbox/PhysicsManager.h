#pragma once

#include <vector>
#include <memory>
#include <shared_mutex>
#include <span>
#include "PhysicsObject.h"

class PhysicsManager
{
private:
	std::vector<std::shared_ptr<PhysicsObject>> _physicsObjects; // not unique_ptr to allow shared ownership
	mutable std::shared_timed_mutex _mutex; // shared mutex for thread-safe access, mutable to allow const access
											// shared_mutex gives error

public:
	void addObject(std::shared_ptr<PhysicsObject> obj);
	void clearObjects();
	void updatePartitioned(int threadIndex, int numThreads, float dt);

	std::span<const std::shared_ptr<PhysicsObject>> accessPhysicsObjects() const;
};

