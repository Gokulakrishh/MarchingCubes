#include "Heterogeneous/HeterogeneousRunner.h"

#include "DataCenter/PlyWriter.h"
#include "DataCenter/ScalarData.h"
#include "MarchingCube/MarchingCubeTable.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cuda_runtime.h>
#include <expected>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <thrust/count.h>
#include <thrust/device_ptr.h>
#include <thrust/device_vector.h>
#include <thrust/universal_vector.h>
#include <thrust/for_each.h>
#include <thrust/host_vector.h>
#include <thrust/iterator/counting_iterator.h>
#include <thrust/reduce.h>
#include <thrust/scan.h>
#include <thrust/system/cuda/execution_policy.h>
#include <thrust/transform.h>

namespace
{
constexpr float kIsoEpsilon = 0.00001f;
constexpr unsigned int kCornerCount = 8;
constexpr unsigned int kEdgeCount = 12;
constexpr unsigned int kTriangleSlotCount = 5;
constexpr unsigned int kTriangleVertexCount = 3;
constexpr unsigned int kTriangleEdgeRowWidth = 16;


__host__ __device__ DeviceVec3 makeDeviceVec3(float x, float y, float z)
{
    return DeviceVec3{x, y, z};
}

__device__ unsigned int cornerOffset(unsigned int corner, unsigned int axis)
{
    constexpr unsigned int offsets[kCornerCount][kTriangleVertexCount] = {
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},
        {0, 0, 1},
        {1, 0, 1},
        {1, 1, 1},
        {0, 1, 1}
    };
    return offsets[corner][axis];
}

__device__ unsigned int edgeVertex(unsigned int edge, unsigned int endpoint)
{
    constexpr unsigned int edgeToVertices[kEdgeCount][2] = {
        {0, 1}, {1, 2}, {2, 3}, {0, 3},
        {4, 5}, {5, 6}, {6, 7}, {4, 7},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    return edgeToVertices[edge][endpoint];
}

Vec3 makeHostVec3(const DeviceVec3& deviceVector)
{
    return Vec3{deviceVector.x, deviceVector.y, deviceVector.z};
}

__device__ std::size_t scalarIndex(unsigned int x, unsigned int y, unsigned int z, unsigned int width, unsigned int height)
{
    return static_cast<std::size_t>(x) +
           static_cast<std::size_t>(width) *
               (static_cast<std::size_t>(y) + static_cast<std::size_t>(height) * static_cast<std::size_t>(z));
}

__device__ void decodeCubeId(const GpuContext& context, std::size_t cubeId, unsigned int& x, unsigned int& y, unsigned int& z)
{
    const unsigned int cubeWidth = context.width - 1;
    const unsigned int cubeHeight = context.height - 1;
    const std::size_t xyPlaneSize = static_cast<std::size_t>(cubeWidth) * cubeHeight;

    x = static_cast<unsigned int>(cubeId % cubeWidth);
    y = static_cast<unsigned int>((cubeId / cubeWidth) % cubeHeight);
    z = static_cast<unsigned int>(cubeId / xyPlaneSize);
}

__device__ DeviceVec3 positionAt(const GpuContext& context, unsigned int x, unsigned int y, unsigned int z)
{
    return makeDeviceVec3(
        context.origin.x + static_cast<float>(x) * context.spacing.x,
        context.origin.y + static_cast<float>(y) * context.spacing.y,
        context.origin.z + static_cast<float>(z) * context.spacing.z);
}

__device__ DeviceVec3 interpolateVertex(
    float isoValue,
    const DeviceVec3& firstPosition,
    float firstValue,
    const DeviceVec3& secondPosition,
    float secondValue)
{
    if (fabsf(isoValue - firstValue) < kIsoEpsilon) {
        return firstPosition;
    }

    if (fabsf(isoValue - secondValue) < kIsoEpsilon) {
        return secondPosition;
    }

    if (fabsf(firstValue - secondValue) < kIsoEpsilon) {
        return firstPosition;
    }

    const float ratio = fminf(fmaxf((isoValue - firstValue) / (secondValue - firstValue), 0.0f), 1.0f);
    return makeDeviceVec3(
        firstPosition.x + ratio * (secondPosition.x - firstPosition.x),
        firstPosition.y + ratio * (secondPosition.y - firstPosition.y),
        firstPosition.z + ratio * (secondPosition.z - firstPosition.z));
}

__device__ int buildCubeIndex(const GpuContext& context, unsigned int x, unsigned int y, unsigned int z)
{
    int cubeIndex = 0;

    for (unsigned int corner = 0; corner < kCornerCount; ++corner) {
        const float value = context.values[scalarIndex(
            x + cornerOffset(corner, 0),
            y + cornerOffset(corner, 1),
            z + cornerOffset(corner, 2),
            context.width,
            context.height)];
        if (value < context.isoValue) {
            cubeIndex |= 1 << corner;
        }
    }

    return cubeIndex;
}

__device__ std::size_t triangleCountForCubeIndex(const GpuContext& context, int cubeIndex)
{
    if (cubeIndex == 0 || cubeIndex == 255) {
        return 0;
    }

    std::size_t triangleCount = 0;
    const int* triangleEdges = context.triangleEdgeTable + static_cast<std::size_t>(cubeIndex) * kTriangleEdgeRowWidth;
    for (unsigned int triangle = 0; triangle < kTriangleSlotCount; ++triangle) {
        if (triangleEdges[triangle * 3] == -1) {
            break;
        }

        ++triangleCount;
    }

    return triangleCount;
}

struct CountTrianglesFunctor
{
    GpuContext context;

    __device__ std::size_t operator()(std::size_t cubeId) const
    {
        unsigned int x{};
        unsigned int y{};
        unsigned int z{};
        decodeCubeId(context, cubeId, x, y, z);
        return triangleCountForCubeIndex(context, buildCubeIndex(context, x, y, z));
    }
};

struct GenerateTrianglesFunctor
{
    GpuContext context;
    const std::size_t* triangleOffsets{};
    DeviceTriangle* outputTriangles{};

    __device__ void operator()(std::size_t cubeId) const
    {
        unsigned int x{};
        unsigned int y{};
        unsigned int z{};
        decodeCubeId(context, cubeId, x, y, z);

        const int cubeIndex = buildCubeIndex(context, x, y, z);
        if (cubeIndex == 0 || cubeIndex == 255) {
            return;
        }

        DeviceVec3 cornerPositions[kCornerCount]{};
        float cornerValues[kCornerCount]{};
        for (unsigned int corner = 0; corner < kCornerCount; ++corner) {
            const unsigned int sampleX = x + cornerOffset(corner, 0);
            const unsigned int sampleY = y + cornerOffset(corner, 1);
            const unsigned int sampleZ = z + cornerOffset(corner, 2);

            cornerPositions[corner] = positionAt(context, sampleX, sampleY, sampleZ);
            cornerValues[corner] = context.values[scalarIndex(sampleX, sampleY, sampleZ, context.width, context.height)];
        }

        DeviceVec3 edgeIntersections[kEdgeCount]{};
        const int edgeMask = context.edgeTable[cubeIndex];
        for (unsigned int edge = 0; edge < kEdgeCount; ++edge) {
            if ((edgeMask & (1 << edge)) == 0) {
                continue;
            }

            const unsigned int firstCorner = edgeVertex(edge, 0);
            const unsigned int secondCorner = edgeVertex(edge, 1);

            edgeIntersections[edge] = interpolateVertex(
                context.isoValue,
                cornerPositions[firstCorner],
                cornerValues[firstCorner],
                cornerPositions[secondCorner],
                cornerValues[secondCorner]);
        }

        const int* triangleEdges = context.triangleEdgeTable + static_cast<std::size_t>(cubeIndex) * kTriangleEdgeRowWidth;
        const std::size_t outputOffset = triangleOffsets[cubeId];

        for (unsigned int triangle = 0; triangle < kTriangleSlotCount; ++triangle) {
            const unsigned int triangleEdge = triangle * 3;
            if (triangleEdges[triangleEdge] == -1) {
                break;
            }

            DeviceTriangle outputTriangle{};
            for (unsigned int vertex = 0; vertex < kTriangleVertexCount; ++vertex) {
                const unsigned int edge = static_cast<unsigned int>(triangleEdges[triangleEdge + vertex]);
                outputTriangle.vertices[vertex] = edgeIntersections[edge];
            }

            outputTriangles[outputOffset + triangle] = outputTriangle;
        }
    }
};

/*
std::vector<float> flattenScalarValues(const ScalarData& scalarData)
{
    std::vector<float> values;
    values.reserve(static_cast<std::size_t>(scalarData.width()) * scalarData.height() * scalarData.depth());

    for (unsigned int z = 0; z < scalarData.depth(); ++z) {
        for (unsigned int y = 0; y < scalarData.height(); ++y) {
            for (unsigned int x = 0; x < scalarData.width(); ++x) {
                values.push_back(scalarData.at(x, y, z));
            }
        }
    }

    return values;
}*/

std::vector<int> flattenTriangleEdgeTable()
{
    std::vector<int> flattened;
    flattened.reserve(256 * 16);

    for (unsigned int cubeIndex = 0; cubeIndex < 256; ++cubeIndex) {
        for (unsigned int edgeIndex = 0; edgeIndex < 16; ++edgeIndex) {
            flattened.push_back(TriangleEdgeTable[cubeIndex][edgeIndex]);
        }
    }

    return flattened;
}

std::vector<Triangle> convertTrianglesToHost(const DeviceTriangle* deviceTriangles, std::size_t size)
{
    std::vector<Triangle> triangles;
    triangles.reserve(size);

    for (std::size_t i = 0; i < size; ++i)
        triangles.push_back({{makeHostVec3(deviceTriangles[i].vertices[0]),
                               makeHostVec3(deviceTriangles[i].vertices[1]),
                               makeHostVec3(deviceTriangles[i].vertices[2])}});
    return triangles;
}

DeviceVec3 toDeviceVec3(const Vec3& value)
{
    return makeDeviceVec3(value.x, value.y, value.z);
}

} // namespace

HeterogeneousRunner::HeterogeneousRunner(const std::string& inputFile, const std::string& outputFile, float isoValue)
    : m_inputFile(inputFile),
      m_outputFile(outputFile),
      m_isoValue(isoValue)
{
    std::cout << "Input: " << inputFile << '\n';
    std::cout << "Output: " << outputFile << '\n';
}

std::expected<void, std::string> HeterogeneousRunner::run()
{
    std::cout << "Running heterogeneous marching cubes\n";

    const auto totalStart = std::chrono::steady_clock::now();

    ScalarData scalarData(m_inputFile);
    if (!scalarData.valid()) {
        return std::unexpected(*scalarData.error());
    }

    if (scalarData.width() < 2 || scalarData.height() < 2 || scalarData.depth() < 2) {
        return std::unexpected("Scalar field is too small to build cubes");
    }

    const auto flattenStart = std::chrono::steady_clock::now();
    const thrust::universal_vector<float> deviceValues(scalarData.values().begin(), scalarData.values().end());//flattenScalarValues(scalarData);
    const thrust::universal_vector<int> deviceEdgeTable(std::begin(EdgeTable), std::end(EdgeTable));
    const thrust::universal_vector<int> deviceTriangleEdgeTable = flattenTriangleEdgeTable();
    const Vec3 origin = scalarData.position(0, 0, 0);
    const Vec3 spacing = {
        scalarData.position(1, 0, 0).x - origin.x,
        scalarData.position(0, 1, 0).y - origin.y,
        scalarData.position(0, 0, 1).z - origin.z
    };
    const auto flattenEnd = std::chrono::steady_clock::now();

    try {
        const auto gpuStart = std::chrono::steady_clock::now();

        const unsigned int cubeWidth = scalarData.width() - 1;
        const unsigned int cubeHeight = scalarData.height() - 1;
        const unsigned int cubeDepth = scalarData.depth() - 1;
        const std::size_t cubeCount = static_cast<std::size_t>(cubeWidth) * cubeHeight * cubeDepth;
        thrust::device_vector<std::size_t> triangleCounts(cubeCount);
        thrust::device_vector<std::size_t> triangleOffsets(cubeCount);

        GpuContext context{
            thrust::raw_pointer_cast(deviceValues.data()),
            scalarData.width(),
            scalarData.height(),
            scalarData.depth(),
            m_isoValue,
            toDeviceVec3(origin),
            toDeviceVec3(spacing),
            thrust::raw_pointer_cast(deviceEdgeTable.data()),
            thrust::raw_pointer_cast(deviceTriangleEdgeTable.data())
        };

        const auto cubeBegin = thrust::counting_iterator<std::size_t>(0);
        const auto cubeEnd = cubeBegin + cubeCount;

        thrust::transform(thrust::cuda::par, cubeBegin, cubeEnd, triangleCounts.begin(), CountTrianglesFunctor{context});
        thrust::exclusive_scan(thrust::cuda::par, triangleCounts.begin(), triangleCounts.end(), triangleOffsets.begin());

        const std::size_t triangleCount = triangleOffsets.back() + triangleCounts.back();
        thrust::device_vector<DeviceTriangle> deviceTriangles(triangleCount);

        thrust::for_each(
            thrust::cuda::par,
            cubeBegin,
            cubeEnd,
            GenerateTrianglesFunctor{
                context,
                thrust::raw_pointer_cast(triangleOffsets.data()),
                thrust::raw_pointer_cast(deviceTriangles.data())
            });

        const cudaError_t generationResult = cudaDeviceSynchronize();
        if (generationResult != cudaSuccess) {
            throw std::runtime_error(
                std::string("synchronize Thrust triangle generation: ") +
                cudaGetErrorString(generationResult));
        }
        const auto gpuEnd = std::chrono::steady_clock::now();

        const auto downloadStart = std::chrono::steady_clock::now();
        thrust::host_vector<DeviceTriangle> hostDeviceTriangles = deviceTriangles;
        const auto downloadEnd = std::chrono::steady_clock::now();

        const auto convertStart = std::chrono::steady_clock::now();
        const std::vector<Triangle> triangles = convertTrianglesToHost(thrust::raw_pointer_cast(hostDeviceTriangles.data()), hostDeviceTriangles.size());
        const auto convertEnd = std::chrono::steady_clock::now();

        std::cout << "Generated triangles: " << triangles.size() << '\n';

        const auto writeStart = std::chrono::steady_clock::now();
        const PlyWriter writer;
        if (auto result = writer.write(m_outputFile, triangles); !result) {
            return std::unexpected(result.error());
        }
        const auto writeEnd = std::chrono::steady_clock::now();

        std::cout << "Wrote PLY: " << m_outputFile << '\n';
        std::cout << "Heterogeneous flatten time: "
                  << std::chrono::duration<double, std::milli>(flattenEnd - flattenStart).count() << " ms\n";
        std::cout << "Heterogeneous GPU time: "
                  << std::chrono::duration<double, std::milli>(gpuEnd - gpuStart).count() << " ms\n";
        std::cout << "Heterogeneous download time: "
                  << std::chrono::duration<double, std::milli>(downloadEnd - downloadStart).count() << " ms\n";
        std::cout << "Heterogeneous convert time: "
                  << std::chrono::duration<double, std::milli>(convertEnd - convertStart).count() << " ms\n";
        std::cout << "Heterogeneous write time: "
                  << std::chrono::duration<double, std::milli>(writeEnd - writeStart).count() << " ms\n";
        std::cout << "Heterogeneous total time: "
                  << std::chrono::duration<double, std::milli>(writeEnd - totalStart).count() << " ms\n";
    } catch (const std::exception& exception) {
        return std::unexpected(std::string("CUDA/Thrust execution failed: ") + exception.what());
    }

    return {};
}
