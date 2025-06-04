#include "PhysicsManager.h"

void PhysicsManager::addObject(std::shared_ptr<PhysicsObject> obj)
{
	std::unique_lock lock(_mutex); // Use unique_lock for exclusive access (wiritng) to the shared resource
	_physicsObjects.push_back(std::move(obj));
}

void PhysicsManager::clearObjects()
{
	std::unique_lock lock(_mutex); // Use unique_lock for exclusive access (writing) to the shared resource
	_physicsObjects.clear();
}

void PhysicsManager::updatePartitioned(int threadIndex, int numThreads, float dt)
{
    std::shared_lock readLock(_mutex); // Çoklu thread okuma yapabilir
    const size_t count = _physicsObjects.size();
    if (count == 0 || threadIndex >= numThreads) return;

    const size_t perThread = count / numThreads;
    const size_t start = threadIndex * perThread;
    const size_t end = (threadIndex == numThreads - 1) ? count : start + perThread;

    // Save previous positions
    for (size_t i = start; i < end; ++i) {
        _physicsObjects[i]->resetPositionRestorationFlag();
        _physicsObjects[i]->savePreviousPosition();
    }

    // Integrate motion
    for (size_t i = start; i < end; ++i) {
        _physicsObjects[i]->Update(dt);
    }

	// Intra-thread collision - collision detection and resolution within the same thread
    for (size_t i = start; i < end; ++i) {
        for (size_t j = i + 1; j < end; ++j) {
            auto* a = _physicsObjects[i].get();
            auto* b = _physicsObjects[j].get();
            if (a > b) std::swap(a, b);

            float penetration = 0.0f;
            DirectX::XMFLOAT3 normal;
            if (a->checkCollision(*b, normal, penetration)) {
                std::scoped_lock lock(a->getCollisionMutex(), b->getCollisionMutex());
                a->resolveCollision(*b, normal, penetration);
            }
        }
    }

	// Inter-thread collision (evenly distributed) - collision detection and resolution between different threads
    for (size_t i = 0; i < count; ++i) {
        size_t part_i = i / perThread;
        for (size_t j = i + 1; j < count; ++j) {
            size_t part_j = j / perThread;
            if (part_i == part_j) continue;
            if ((i + j) % numThreads != threadIndex) continue;

            auto* a = _physicsObjects[i].get();
            auto* b = _physicsObjects[j].get();
            if (a > b) std::swap(a, b);

            float penetration = 0.0f;
            DirectX::XMFLOAT3 normal;
            if (a->checkCollision(*b, normal, penetration)) {
                std::scoped_lock lock(a->getCollisionMutex(), b->getCollisionMutex());
                a->resolveCollision(*b, normal, penetration);
            }
        }
    }
}


std::span<const std::shared_ptr<PhysicsObject>> PhysicsManager::accessPhysicsObjects() const
{
	std::shared_lock lock(_mutex); // Use shared_lock for read-only access to the shared resource
	return { _physicsObjects.begin(), _physicsObjects.end() };
}
