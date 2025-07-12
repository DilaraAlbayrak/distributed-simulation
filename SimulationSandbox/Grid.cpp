#include "Grid.h"
#include "PhysicsObject.h"
#include <cmath>
#include <algorithm>

Grid::Grid(float worldSize, int cellsPerAxis)
    : _worldSize(worldSize), _cellsPerAxis(cellsPerAxis)
{
    // Ensure cellsPerAxis is at least 1 to avoid division by zero.
    if (_cellsPerAxis == 0) _cellsPerAxis = 1;
    _cellSize = _worldSize / _cellsPerAxis;
    float halfSize = _worldSize / 2.0f;
    _origin = { -halfSize, -halfSize, -halfSize };
}

void Grid::clear()
{
    _cells.clear();
}

int Grid::getCellIndex(const DirectX::XMFLOAT3& position) const
{
    int x = static_cast<int>(floorf((position.x - _origin.x) / _cellSize));
    int y = static_cast<int>(floorf((position.y - _origin.y) / _cellSize));
    int z = static_cast<int>(floorf((position.z - _origin.z) / _cellSize));

    x = std::clamp(x, 0, _cellsPerAxis - 1);
    y = std::clamp(y, 0, _cellsPerAxis - 1);
    z = std::clamp(z, 0, _cellsPerAxis - 1);

    return x + y * _cellsPerAxis + z * _cellsPerAxis * _cellsPerAxis;
}

void Grid::insert(const std::shared_ptr<PhysicsObject>& obj)
{
    if (!obj) return;
    int cellIndex = getCellIndex(obj->getPosition());
    _cells[cellIndex].push_back(obj);
}

std::vector<std::shared_ptr<PhysicsObject>> Grid::getNearbyObjects(const std::shared_ptr<PhysicsObject>& obj) const
{
    std::vector<std::shared_ptr<PhysicsObject>> nearby;
    if (!obj) return nearby;

    DirectX::XMFLOAT3 pos = obj->getPosition();
    int centerX = static_cast<int>(floorf((pos.x - _origin.x) / _cellSize));
    int centerY = static_cast<int>(floorf((pos.y - _origin.y) / _cellSize));
    int centerZ = static_cast<int>(floorf((pos.z - _origin.z) / _cellSize));

    // Iterate through the 3x3x3 block of cells around the object's central cell.
    for (int z_offset = -1; z_offset <= 1; ++z_offset)
    {
        for (int y_offset = -1; y_offset <= 1; ++y_offset)
        {
            for (int x_offset = -1; x_offset <= 1; ++x_offset)
            {
                int checkX = centerX + x_offset;
                int checkY = centerY + y_offset;
                int checkZ = centerZ + z_offset;

                // Check if the neighbouring cell coordinate is within the grid bounds.
                if (checkX < 0 || checkX >= _cellsPerAxis ||
                    checkY < 0 || checkY >= _cellsPerAxis ||
                    checkZ < 0 || checkZ >= _cellsPerAxis)
                {
                    continue;
                }

                // Calculate the hash index directly from the grid coordinates.
                int cellIndex = checkX + checkY * _cellsPerAxis + checkZ * _cellsPerAxis * _cellsPerAxis;

                auto it = _cells.find(cellIndex);
                if (it != _cells.end())
                {
                    // Add the objects from the found cell to the results list.
                    nearby.insert(nearby.end(), it->second.begin(), it->second.end());
                }
            }
        }
    }

    return nearby;
}