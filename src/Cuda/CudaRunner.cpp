#include "Cuda/CudaRunner.h"
#include "Cuda/CudaRunnerTypes.h"

#include "DataCenter/PlyWriter.h"
#include "DataCenter/ScalarData.h"
#include "MarchingCube/MarchingCubeTable.h"

#include <cub/device/device_scan.cuh>
#include <cuda_runtime.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
using namespace cuda_runner_detail;

__constant__ int kDeviceEdgeTable[256];
__constant__ int kDeviceTriangleEdgeTable[256 * kTriangleEdgeRowWidth];

__device__ CudaVec3 makeCudaVec3(float x, float y, float z)
{
    return CudaVec3{x, y, z};
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

__device__ std::size_t scalarIndex(
    unsigned int x,
    unsigned int y,
    unsigned int z,
    unsigned int width,
    unsigned int height)
{
    return static_cast<std::size_t>(x) +
           static_cast<std::size_t>(width) *
               (static_cast<std::size_t>(y) + static_cast<std::size_t>(height) * static_cast<std::size_t>(z));
}

__device__ void decodeCubeId(const KernelContext& context, std::size_t cubeId, unsigned int& x, unsigned int& y, unsigned int& z)
{
    const unsigned int cubeWidth = context.width - 1;
    const unsigned int cubeHeight = context.height - 1;
    const std::size_t xyPlaneSize = static_cast<std::size_t>(cubeWidth) * cubeHeight;

    x = static_cast<unsigned int>(cubeId % cubeWidth);
    y = static_cast<unsigned int>((cubeId / cubeWidth) % cubeHeight);
    z = static_cast<unsigned int>(cubeId / xyPlaneSize);
}

__device__ CudaVec3 positionAt(const KernelContext& context, unsigned int x, unsigned int y, unsigned int z)
{
    return makeCudaVec3(
        context.origin.x + static_cast<float>(x) * context.spacing.x,
        context.origin.y + static_cast<float>(y) * context.spacing.y,
        context.origin.z + static_cast<float>(z) * context.spacing.z);
}

__device__ CudaVec3 interpolateVertex(float isoValue, const CudaVec3& firstPosition, float firstValue, const CudaVec3& secondPosition, float secondValue)
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
    return makeCudaVec3(
        firstPosition.x + ratio * (secondPosition.x - firstPosition.x),
        firstPosition.y + ratio * (secondPosition.y - firstPosition.y),
        firstPosition.z + ratio * (secondPosition.z - firstPosition.z));
}

__device__ void loadCornerValues(
    const KernelContext& context,
    unsigned int x,
    unsigned int y,
    unsigned int z,
    float (&cornerValues)[kCornerCount])
{
#pragma unroll
    for (unsigned int corner = 0; corner < kCornerCount; ++corner) {
        const unsigned int sampleX = x + cornerOffset(corner, 0);
        const unsigned int sampleY = y + cornerOffset(corner, 1);
        const unsigned int sampleZ = z + cornerOffset(corner, 2);
        cornerValues[corner] =
            context.values[scalarIndex(sampleX, sampleY, sampleZ, context.width, context.height)];
    }
}

__device__ int buildCubeIndex(const float (&cornerValues)[kCornerCount], float isoValue)
{
    int cubeIndex = 0;
#pragma unroll
    for (unsigned int corner = 0; corner < kCornerCount; ++corner) {
        if (cornerValues[corner] < isoValue) {
            cubeIndex |= 1 << corner;
        }
    }
    return cubeIndex;
}

__device__ std::size_t triangleCountForCubeIndex(int cubeIndex)
{
    if (cubeIndex == 0 || cubeIndex == 255) {
        return 0;
    }

    std::size_t triangleCount = 0;
    const int* triangleEdges =
        kDeviceTriangleEdgeTable + static_cast<std::size_t>(cubeIndex) * kTriangleEdgeRowWidth;
#pragma unroll
    for (unsigned int triangle = 0; triangle < kTriangleSlotCount; ++triangle) {
        if (triangleEdges[triangle * kTriangleVertexCount] == -1) {
            break;
        }
        ++triangleCount;
    }
    return triangleCount;
}

__global__ void countTrianglesKernel(
    KernelContext context,
    std::size_t cubeCount,
    std::size_t* triangleCounts,
    int* cubeIndices)
{
    const std::size_t cubeId =
        static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (cubeId >= cubeCount) {
        return;
    }

    unsigned int x{};
    unsigned int y{};
    unsigned int z{};
    decodeCubeId(context, cubeId, x, y, z);

    float cornerValues[kCornerCount]{};
    loadCornerValues(context, x, y, z, cornerValues);
    const int cubeIndex = buildCubeIndex(cornerValues, context.isoValue);
    cubeIndices[cubeId] = cubeIndex;
    triangleCounts[cubeId] = triangleCountForCubeIndex(cubeIndex);
}

__global__ void generateTrianglesKernel(
    KernelContext context,
    std::size_t cubeCount,
    const int* cubeIndices,
    const std::size_t* triangleOffsets,
    CudaTriangle* outputTriangles)
{
    const std::size_t cubeId =
        static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (cubeId >= cubeCount) {
        return;
    }

    const int cubeIndex = cubeIndices[cubeId];
    if (cubeIndex == 0 || cubeIndex == 255) {
        return;
    }

    unsigned int x{};
    unsigned int y{};
    unsigned int z{};
    decodeCubeId(context, cubeId, x, y, z);

    float cornerValues[kCornerCount]{};
    loadCornerValues(context, x, y, z, cornerValues);

    CudaVec3 cornerPositions[kCornerCount]{};
#pragma unroll
    for (unsigned int corner = 0; corner < kCornerCount; ++corner) {
        cornerPositions[corner] = positionAt(
            context,
            x + cornerOffset(corner, 0),
            y + cornerOffset(corner, 1),
            z + cornerOffset(corner, 2));
    }

    CudaVec3 edgeIntersections[kEdgeCount]{};
    const int edgeMask = kDeviceEdgeTable[cubeIndex];
#pragma unroll
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

    const int* triangleEdges =
        kDeviceTriangleEdgeTable + static_cast<std::size_t>(cubeIndex) * kTriangleEdgeRowWidth;
    const std::size_t outputOffset = triangleOffsets[cubeId];

#pragma unroll
    for (unsigned int triangle = 0; triangle < kTriangleSlotCount; ++triangle) {
        const unsigned int triangleEdge = triangle * kTriangleVertexCount;
        if (triangleEdges[triangleEdge] == -1) {
            break;
        }

        CudaTriangle outputTriangle{};
#pragma unroll
        for (unsigned int vertex = 0; vertex < kTriangleVertexCount; ++vertex) {
            const unsigned int edge = static_cast<unsigned int>(triangleEdges[triangleEdge + vertex]);
            outputTriangle.vertices[vertex] = edgeIntersections[edge];
        }
        outputTriangles[outputOffset + triangle] = outputTriangle;
    }
}

CudaVec3 toCudaVec3(const Vec3& value)
{
    return CudaVec3{value.x, value.y, value.z};
}

std::vector<Triangle> convertTrianglesToHost(const PinnedHostBuffer<CudaTriangle>& cudaTriangles)
{
    static_assert(sizeof(CudaVec3) == sizeof(Vec3));
    static_assert(sizeof(CudaTriangle) == sizeof(Triangle));
    static_assert(alignof(CudaTriangle) == alignof(Triangle));
    static_assert(std::is_trivially_copyable_v<CudaTriangle>);
    static_assert(std::is_trivially_copyable_v<Triangle>);

    if (cudaTriangles.size() == 0) {
        return {};
    }

    const auto* triangleBegin = reinterpret_cast<const Triangle*>(cudaTriangles.data());
    return std::vector<Triangle>(triangleBegin, triangleBegin + cudaTriangles.size());
}
} // namespace

CudaRunner::CudaRunner(const std::string& inputFile, const std::string& outputFile, float isoValue)
    : m_inputFile(inputFile),
      m_outputFile(outputFile),
      m_isoValue(isoValue)
{
    std::cout << "Input: " << inputFile << '\n';
    std::cout << "Output: " << outputFile << '\n';
}

std::expected<void, std::string> CudaRunner::run()
{
    std::cout << "Running CUDA kernel marching cubes\n";
    const auto totalStart = std::chrono::steady_clock::now();

    ScalarData scalarData(m_inputFile);
    if (!scalarData.valid()) {
        return std::unexpected(*scalarData.error());
    }

    if (scalarData.width() < 2 || scalarData.height() < 2 || scalarData.depth() < 2) {
        return std::unexpected("Scalar field is too small to build cubes");
    }

    try {
        const auto setupStart = std::chrono::steady_clock::now();
        const CudaStream stream;

        const unsigned int cubeWidth = scalarData.width() - 1;
        const unsigned int cubeHeight = scalarData.height() - 1;
        const unsigned int cubeDepth = scalarData.depth() - 1;
        const std::size_t cubeCount = static_cast<std::size_t>(cubeWidth) * cubeHeight * cubeDepth;

        DeviceBuffer<float> deviceValues(scalarData.values().size());
        DeviceBuffer<std::size_t> triangleCounts(cubeCount + 1);
        DeviceBuffer<std::size_t> triangleOffsets(cubeCount + 1);
        DeviceBuffer<int> cubeIndices(cubeCount);

        checkCuda(
            cudaMemcpyAsync(
                deviceValues.data(),
                scalarData.values().data(),
                scalarData.values().size() * sizeof(float),
                cudaMemcpyHostToDevice,
                stream.get()),
            "copy scalar values to device");
        checkCuda(
            cudaMemcpyToSymbolAsync(
                kDeviceEdgeTable,
                EdgeTable,
                sizeof(EdgeTable),
                0,
                cudaMemcpyHostToDevice,
                stream.get()),
            "copy edge table to constant memory");
        checkCuda(
            cudaMemcpyToSymbolAsync(
                kDeviceTriangleEdgeTable,
                TriangleEdgeTable,
                sizeof(TriangleEdgeTable),
                0,
                cudaMemcpyHostToDevice,
                stream.get()),
            "copy triangle table to constant memory");
        checkCuda(
            cudaMemsetAsync(
                triangleCounts.data() + cubeCount,
                0,
                sizeof(std::size_t),
                stream.get()),
            "initialize scan sentinel");

        std::size_t scanStorageBytes{};
        checkCuda(
            cub::DeviceScan::ExclusiveSum(
                nullptr,
                scanStorageBytes,
                triangleCounts.data(),
                triangleOffsets.data(),
                cubeCount + 1,
                stream.get()),
            "query CUB scan storage");
        DeviceBuffer<std::byte> scanStorage(scanStorageBytes);

        const Vec3 origin = scalarData.position(0, 0, 0);
        const Vec3 spacing{
            scalarData.position(1, 0, 0).x - origin.x,
            scalarData.position(0, 1, 0).y - origin.y,
            scalarData.position(0, 0, 1).z - origin.z
        };
        const KernelContext context{
            deviceValues.data(),
            scalarData.width(),
            scalarData.height(),
            scalarData.depth(),
            m_isoValue,
            toCudaVec3(origin),
            toCudaVec3(spacing)
        };
        const unsigned int blockCount =
            static_cast<unsigned int>((cubeCount + kThreadsPerBlock - 1) / kThreadsPerBlock);
        checkCuda(cudaStreamSynchronize(stream.get()), "synchronize CUDA setup");
        const auto setupEnd = std::chrono::steady_clock::now();

        const auto gpuStart = std::chrono::steady_clock::now();

        countTrianglesKernel<<<blockCount, kThreadsPerBlock, 0, stream.get()>>>(
            context,
            cubeCount,
            triangleCounts.data(),
            cubeIndices.data());
        checkCuda(cudaGetLastError(), "launch countTrianglesKernel");
        checkCuda(cudaStreamSynchronize(stream.get()), "synchronize countTrianglesKernel");

        checkCuda(
            cub::DeviceScan::ExclusiveSum(
                scanStorage.data(),
                scanStorageBytes,
                triangleCounts.data(),
                triangleOffsets.data(),
                cubeCount + 1,
                stream.get()),
            "scan triangle counts");

        PinnedHostBuffer<std::size_t> hostTriangleCount(1);
        checkCuda(
            cudaMemcpyAsync(
                hostTriangleCount.data(),
                triangleOffsets.data() + cubeCount,
                sizeof(std::size_t),
                cudaMemcpyDeviceToHost,
                stream.get()),
            "copy triangle count");
        checkCuda(cudaStreamSynchronize(stream.get()), "synchronize triangle count copy");
        const std::size_t triangleCount = hostTriangleCount.data()[0];

        DeviceBuffer<CudaTriangle> deviceTriangles(triangleCount);
        if (triangleCount != 0) {
            generateTrianglesKernel<<<blockCount, kThreadsPerBlock, 0, stream.get()>>>(
                context,
                cubeCount,
                cubeIndices.data(),
                triangleOffsets.data(),
                deviceTriangles.data());
            checkCuda(cudaGetLastError(), "launch generateTrianglesKernel");
            checkCuda(cudaStreamSynchronize(stream.get()), "synchronize generateTrianglesKernel");
        }
        const auto gpuEnd = std::chrono::steady_clock::now();

        const auto downloadStart = std::chrono::steady_clock::now();
        PinnedHostBuffer<CudaTriangle> hostCudaTriangles(triangleCount);
        if (triangleCount != 0) {
            checkCuda(
                cudaMemcpyAsync(
                    hostCudaTriangles.data(),
                    deviceTriangles.data(),
                    triangleCount * sizeof(CudaTriangle),
                    cudaMemcpyDeviceToHost,
                    stream.get()),
                "copy triangles to host");
            checkCuda(cudaStreamSynchronize(stream.get()), "synchronize triangle download");
        }
        const auto downloadEnd = std::chrono::steady_clock::now();

        const auto convertStart = std::chrono::steady_clock::now();
        const std::vector<Triangle> triangles = convertTrianglesToHost(hostCudaTriangles);
        const auto convertEnd = std::chrono::steady_clock::now();

        std::cout << "Generated triangles: " << triangles.size() << '\n';

        const auto writeStart = std::chrono::steady_clock::now();
        const PlyWriter writer;
        if (auto result = writer.write(m_outputFile, triangles); !result) {
            return std::unexpected(result.error());
        }
        const auto writeEnd = std::chrono::steady_clock::now();

        std::cout << "Wrote PLY: " << m_outputFile << '\n';
        std::cout << "CUDA setup time: "
                  << std::chrono::duration<double, std::milli>(setupEnd - setupStart).count() << " ms\n";
        std::cout << "CUDA GPU time: "
                  << std::chrono::duration<double, std::milli>(gpuEnd - gpuStart).count() << " ms\n";
        std::cout << "CUDA download time: "
                  << std::chrono::duration<double, std::milli>(downloadEnd - downloadStart).count() << " ms\n";
        std::cout << "CUDA convert time: "
                  << std::chrono::duration<double, std::milli>(convertEnd - convertStart).count() << " ms\n";
        std::cout << "CUDA write time: "
                  << std::chrono::duration<double, std::milli>(writeEnd - writeStart).count() << " ms\n";
        std::cout << "CUDA total time: "
                  << std::chrono::duration<double, std::milli>(writeEnd - totalStart).count() << " ms\n";
    } catch (const std::exception& exception) {
        return std::unexpected(std::string("CUDA kernel execution failed: ") + exception.what());
    }

    return {};
}
