#ifndef SPATIAL_GRID_HPP
#define SPATIAL_GRID_HPP

#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "Entity.hpp"

class SpatialGrid {
private:
    float cellSize;

    struct Cell {
        std::mutex mtx;
        std::vector<std::shared_ptr<Entity>> entities;
    };

    std::unordered_map<long long, std::unique_ptr<Cell>> gridMap;
    std::mutex mapMutex;

    long long getCellKey(float x, float y) const;

public:
    explicit SpatialGrid(float cellSize);

    void addEntity(std::shared_ptr<Entity> entity);
    void clear();
    void processEntitiesParallel(float width, float height);
};

#endif