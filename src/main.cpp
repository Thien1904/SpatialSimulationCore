#include <iostream>
#include <vector>
#include <memory>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include "Entity.hpp"
#include "SpatialGrid.hpp"
#include "WebSocketServer.hpp"

std::string serializeState(const std::vector<std::shared_ptr<Entity>>& entities, int frame, double frameMilliseconds) {
    std::ostringstream json;
    json << "{\"frame\":" << frame << ",\"frameMs\":" << frameMilliseconds << ",\"entities\":[";
    for (size_t i = 0; i < entities.size(); ++i) {
        if (i > 0) {
            json << ",";
        }
        json << "{\"x\":" << entities[i]->x << ",\"y\":" << entities[i]->y << "}";
    }
    json << "]}";
    return json.str();
}

void exportStateToJson(const std::string& state, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open output file: " + filename);
    }
    file << state;
    if (!file) {
        throw std::runtime_error("Cannot write output file: " + filename);
    }
}

int main() {
    constexpr float CellSize = 50.0f;
    constexpr float WorldWidth = 800.0f;
    constexpr float WorldHeight = 600.0f;
    SpatialGrid simulationGrid(CellSize);
    WebSocketServer server(8080);

    constexpr int NumEntities = 9000;
    std::vector<std::shared_ptr<Entity>> allEntities;
    allEntities.reserve(NumEntities);

    for (int i = 0; i < NumEntities; ++i) {
        float x = static_cast<float>(rand() % 800);
        float y = static_cast<float>(rand() % 600);
        float vx = static_cast<float>((rand() % 11) - 5) * 0.3f;
        float vy = static_cast<float>((rand() % 11) - 5) * 0.3f;

        allEntities.push_back(std::make_shared<Entity>(i, x, y, vx, vy));
    }

    try {
        server.start();
    } catch (const std::exception& error) {
        std::cerr << "[ERROR] " << error.what() << "\n";
        return 1;
    }

    std::cout << "[INFO] Initialized " << NumEntities << " entities.\n";
    std::cout << "[INFO] WebSocket server listening at ws://localhost:8080\n";

    for (int frame = 1; frame <= 1000; ++frame) {
        auto start = std::chrono::high_resolution_clock::now();

        for (const auto& entity : allEntities) {
            simulationGrid.addEntity(entity);
        }

        simulationGrid.processEntitiesParallel(WorldWidth, WorldHeight);
        simulationGrid.clear();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        const std::string state = serializeState(allEntities, frame, elapsed.count());
        exportStateToJson(state, "sim_state.json");
        server.publish(state);

        std::cout << "[FRAME " << frame << "] Processed successfully in " << elapsed.count() << " ms.\n";
    }

    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    server.stop();

    return 0;
}