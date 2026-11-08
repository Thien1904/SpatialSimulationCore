cmake_minimum_required(VERSION 3.15)
project(SpatialSimulationCore VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/O2 /arch:AVX2)
else()
    add_compile_options(-O3 -pthread -march=native)
endif()

include_directories(include)

add_executable(SpatialSimulation
    src/main.cpp
    src/Entity.cpp
    src/SpatialGrid.cpp
    src/WebSocketServer.cpp
)

add_executable(SpatialSimulationBenchmark
    src/Benchmark.cpp
    src/Entity.cpp
    src/SpatialGrid.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(SpatialSimulation PRIVATE Threads::Threads)
target_link_libraries(SpatialSimulationBenchmark PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(SpatialSimulation PRIVATE ws2_32)
endif()