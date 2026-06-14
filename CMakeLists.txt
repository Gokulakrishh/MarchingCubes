cmake_minimum_required(VERSION 3.22)

project(MarchingCubes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(MarchingCubes
    src/Cpu/CpuRunner.cpp
    src/Cuda/CudaRunner.cpp
    src/Heterogeneous/HeterogeneousRunner.cpp
    src/main/main.cpp
)

target_include_directories(MarchingCubes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(MarchingCubes PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
)
