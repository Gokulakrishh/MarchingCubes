cmake_minimum_required(VERSION 3.22)

project(MarchingCubes LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS 12.4)
    message(FATAL_ERROR
        "CUDA 12.4 or newer is required to compile the C++23 heterogeneous runner. "
        "Found CUDA ${CMAKE_CUDA_COMPILER_VERSION} at ${CMAKE_CUDA_COMPILER}."
    )
endif()

# CMake 3.28 does not yet model NVCC's CUDA C++23 support.
set(CMAKE_CUDA23_STANDARD_COMPILE_OPTION "--std=c++23")
set(CMAKE_CUDA23_EXTENSION_COMPILE_OPTION "--std=c++23")
list(APPEND CMAKE_CUDA_COMPILE_FEATURES cuda_std_23)
set(CMAKE_CUDA23_COMPILE_FEATURES cuda_std_23)

add_executable(MarchingCubes
    src/Cpu/CpuParallelRunner.cpp
    src/Cpu/CpuRunner.cpp
    src/Cuda/CudaRunner.cpp
    src/DataCenter/PlyWriter.cpp
    src/DataCenter/ScalarData.cpp
    src/main/main.cpp
)

add_library(HeterogeneousRunnerCuda OBJECT src/Heterogeneous/HeterogeneousRunner.cpp)
set_source_files_properties(src/Heterogeneous/HeterogeneousRunner.cpp PROPERTIES LANGUAGE CUDA)

target_include_directories(HeterogeneousRunnerCuda PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(MarchingCubes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(HeterogeneousRunnerCuda PRIVATE
    -Xcompiler=-Wall,-Wextra
    --display-error-number
    --Wreorder
)
target_compile_options(MarchingCubes PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(MarchingCubes PRIVATE HeterogeneousRunnerCuda)
