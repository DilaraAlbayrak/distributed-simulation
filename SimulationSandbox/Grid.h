#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <DirectXMath.h>

class PhysicsObject;

class Grid
{
private:
    // A map where the key is a hashed grid cell index and the value is a list of objects in that cell.
    std::unordered_map<int, std::vector<std::shared_ptr<PhysicsObject>>> _cells;

    // The size of one side of the cubic simulation world.
    float _worldSize;
    // The number of cells along one axis (e.g., 20 results in a 20x20x20 grid).
    int _cellsPerAxis;
    // The pre-calculated size of a single cell.
    float _cellSize;
    // The bottom-left-back corner of the grid, used as the origin.
    DirectX::XMFLOAT3 _origin;

    // A private helper function to convert a 3D world position into a 1D hash key for the map.
    int getCellIndex(const DirectX::XMFLOAT3& position) const;

public:
    // The constructor initialises the grid with the world's dimensions and the desired number of cells.
    Grid(float worldSize = 6.0f, int cellsPerAxis = 20);

    // Removes all objects from all cells, ready for the next frame.
    void clear();

    // Inserts a physics object into the correct cell based on its position.
    void insert(const std::shared_ptr<PhysicsObject>& obj);

    // For a given object, returns a list of all objects in its cell and all 26 neighbouring cells.
    std::vector<std::shared_ptr<PhysicsObject>> getNearbyObjects(const std::shared_ptr<PhysicsObject>& obj) const;
};