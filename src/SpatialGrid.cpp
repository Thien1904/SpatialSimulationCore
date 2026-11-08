#include "SpatialGrid.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>

SpatialGrid::SpatialGrid(float cellSize) : cellSize(cellSize) {}

long long SpatialGrid::getCellKey(float x, float y) const {
    int gx = static_cast<int>(std::floor(x / cellSize));
    int gy = static_cast<int>(std::floor(y / cellSize));
    return (static_cast<long long>(gx) << 32) | (static_cast<unsigned int>(gy));
}

void SpatialGrid::addEntity(std::shared_ptr<Entity> entity) {
    long long key = getCellKey(entity->x, entity->y);

    std::unique_lock<std::mutex> lock(mapMutex);
    if (gridMap.find(key) == gridMap.end()) {
        gridMap[key] = std::make_unique<Cell>();
    }

    std::unique_lock<std::mutex> cellLock(gridMap[key]->mtx);
    gridMap[key]->entities.push_back(entity);
}

void SpatialGrid::clear() {
    std::unique_lock<std::mutex> lock(mapMutex);
    gridMap.clear();
}

void SpatialGrid::processEntitiesParallel(float width, float height) {
    std::vector<Cell*> cells;
    {
        std::unique_lock<std::mutex> lock(mapMutex);
        cells.reserve(gridMap.size());
        for (auto& pair : gridMap) {
            cells.push_back(pair.second.get());
        }
    }

    if (cells.empty()) {
        return;
    }

    const unsigned int availableWorkers = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t workerCount = std::min<std::size_t>(cells.size(), availableWorkers);
    std::atomic<std::size_t> nextCell{0};
    std::vector<std::thread> workers;
    workers.reserve(workerCount);

    for (std::size_t i = 0; i < workerCount; ++i) {
        workers.emplace_back([&]() {
            while (true) {
                const std::size_t index = nextCell.fetch_add(1);
                if (index >= cells.size()) {
                    return;
                }

                Cell* cell = cells[index];
                std::unique_lock<std::mutex> cellLock(cell->mtx);
                for (auto& entity : cell->entities) {
                    entity->update(width, height);
                }
            }
        });
    }

    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }
}