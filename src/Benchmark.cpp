#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "Entity.hpp"
#include "SpatialGrid.hpp"

namespace {
constexpr float WorldWidth = 800.0f;
constexpr float WorldHeight = 600.0f;
constexpr float CellSize = 50.0f;
constexpr int MeasuredFrames = 1000;
constexpr int WarmupFrames = 10;

std::vector<std::shared_ptr<Entity>> createEntities(std::size_t count) {
    std::mt19937 generator(20261106);
    std::uniform_int_distribution<int> xDistribution(0, 799);
    std::uniform_int_distribution<int> yDistribution(0, 599);
    std::uniform_int_distribution<int> velocityDistribution(-5, 5);

    std::vector<std::shared_ptr<Entity>> entities;
    entities.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float x = static_cast<float>(xDistribution(generator));
        const float y = static_cast<float>(yDistribution(generator));
        const float vx = static_cast<float>(velocityDistribution(generator)) * 0.3f;
        const float vy = static_cast<float>(velocityDistribution(generator)) * 0.3f;
        entities.push_back(std::make_shared<Entity>(
            static_cast<int>(i), x, y, vx, vy));
    }
    return entities;
}

void updateBruteForce(
    std::vector<std::shared_ptr<Entity>>& entities,
    int frames) {
    for (int frame = 0; frame < frames; ++frame) {
        for (const auto& entity : entities) {
            entity->update(WorldWidth, WorldHeight);
        }
    }
}

void updateSpatialGrid(
    std::vector<std::shared_ptr<Entity>>& entities,
    int frames) {
    SpatialGrid grid(CellSize);
    for (int frame = 0; frame < frames; ++frame) {
        for (const auto& entity : entities) {
            grid.addEntity(entity);
        }
        grid.processEntitiesParallel(WorldWidth, WorldHeight);
        grid.clear();
    }
}

double checksum(const std::vector<std::shared_ptr<Entity>>& entities) {
    double result = 0.0;
    for (const auto& entity : entities) {
        result += entity->x + entity->y;
    }
    return result;
}

template <typename Update>
double measureMillisecondsPerFrame(
    std::vector<std::shared_ptr<Entity>>& entities,
    Update update) {
    const auto start = std::chrono::steady_clock::now();
    update(entities, MeasuredFrames);
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count() / MeasuredFrames;
}

void benchmark(std::size_t count) {
    auto warmupEntities = createEntities(count);
    updateBruteForce(warmupEntities, WarmupFrames);
    warmupEntities = createEntities(count);
    const double bruteForceMs = measureMillisecondsPerFrame(
        warmupEntities, updateBruteForce);
    const double bruteForceChecksum = checksum(warmupEntities);

    warmupEntities = createEntities(count);
    updateSpatialGrid(warmupEntities, WarmupFrames);
    warmupEntities = createEntities(count);
    const double spatialGridMs = measureMillisecondsPerFrame(
        warmupEntities, updateSpatialGrid);
    const double spatialGridChecksum = checksum(warmupEntities);

    std::cout << count << ",Brute-force," << bruteForceMs << ','
              << bruteForceChecksum << '\n';
    std::cout << count << ",Spatial Grid," << spatialGridMs << ','
              << spatialGridChecksum << '\n';
}
}  // namespace

int main() {
    std::cout << "Update-only benchmark; no collision detection is implemented.\n"
              << "hardware_threads,"
              << std::thread::hardware_concurrency() << '\n'
              << "entities,method,ms_per_frame,checksum\n"
              << std::fixed << std::setprecision(6);

    for (const std::size_t count : {1000, 5000, 10000, 50000}) {
        benchmark(count);
    }
    return 0;
}
