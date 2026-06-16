#include "Cpu/CpuParallelRunner.h"

#include "DataCenter/PlyWriter.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <expected>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

CpuParallelRunner::CpuParallelRunner(const std::string& inputFile, const std::string& outputFile, float isoValue)
    : m_scalarData(inputFile),
      m_outputFile(outputFile),
      m_isoValue(isoValue)
{
    //std::cout << "Output: " << outputFile << '\n';
}

std::expected<void, std::string> CpuParallelRunner::run()
{
    std::cout << "Running parallel CPU marching cubes\n";
    //const auto totalStart = std::chrono::steady_clock::now();

    if (!m_scalarData.valid()) {
        return std::unexpected(*m_scalarData.error());
    }

    if (m_scalarData.width() < 2 || m_scalarData.height() < 2 || m_scalarData.depth() < 2) {
        return std::unexpected("Scalar field is too small to build cubes");
    }

    const unsigned int cubeDepthCount = m_scalarData.depth() - 1;
    const unsigned int hardwareThreads = std::max(1U, std::thread::hardware_concurrency());
    const unsigned int threadCount = std::min(hardwareThreads, cubeDepthCount);
    const unsigned int chunkSize = (cubeDepthCount + threadCount - 1) / threadCount;

    std::vector<std::vector<Triangle>> threadTriangles(threadCount);

    const auto algorithmStart = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> workers;
        workers.reserve(threadCount);

        for (unsigned int thread = 0; thread < threadCount; ++thread) {
            const unsigned int chunkDepthBegin = thread * chunkSize;
            const unsigned int chunkDepthEnd = std::min(cubeDepthCount, chunkDepthBegin + chunkSize);

            workers.emplace_back(&CpuParallelRunner::processThread, this, chunkDepthBegin, chunkDepthEnd, std::ref(threadTriangles[thread]));
        }
    }

    const std::size_t triangleCount = std::ranges::fold_left(
        threadTriangles,
        std::size_t{},
        [](std::size_t count, const std::vector<Triangle>& localTriangles) {
            return count + localTriangles.size();
        });

    std::vector<Triangle> triangles;
    triangles.reserve(triangleCount);
    for (const std::vector<Triangle>& localTriangles : threadTriangles) {
        triangles.insert(triangles.end(), localTriangles.begin(), localTriangles.end());
    }
    const auto algorithmEnd = std::chrono::steady_clock::now();

    std::cout << "Threads: " << threadCount << '\n';
    std::cout << "Generated triangles: " << triangles.size() << '\n';

    const auto writeStart = std::chrono::steady_clock::now();
    const PlyWriter writer;
    if (auto result = writer.write(m_outputFile, triangles); !result) {
        return std::unexpected(result.error());
    }
    const auto writeEnd = std::chrono::steady_clock::now();

    std::cout << "Wrote PLY: " << m_outputFile << '\n';
    std::cout << "CPU parallel algorithm time: " << std::chrono::duration<double, std::milli>(algorithmEnd - algorithmStart).count() << " ms\n";
    std::cout << "CPU parallel write time: " << std::chrono::duration<double, std::milli>(writeEnd - writeStart).count() << " ms\n";
    //std::cout << "CPU parallel total time: " << std::chrono::duration<double, std::milli>(writeEnd - totalStart).count() << " ms\n";
    return {};
}

void CpuParallelRunner::processThread(unsigned int chunkDepthBegin, unsigned int chunkDepthEnd, std::vector<Triangle>& triangles) const
{
    const unsigned int cubeCount = m_scalarData.width() * m_scalarData.height() * (chunkDepthEnd - chunkDepthBegin);
    triangles.reserve(std::min(cubeCount, 250'000U));

    for (unsigned int z = chunkDepthBegin; z < chunkDepthEnd; ++z) {
        for (unsigned int y = 0; y < m_scalarData.height() - 1; ++y) {
            for (unsigned int x = 0; x < m_scalarData.width() - 1; ++x) {
                const GridCell cell = buildGridCell(x, y, z);
                const int cubeIndex = calculateCubeIndex(cell);

                if (cubeIndex == 0 || cubeIndex == 255) {
                    continue;
                }

                const std::array<Vec3, 12> edgeIntersections = calculateEdgeIntersections(cell, cubeIndex);
                buildTriangles(cubeIndex, edgeIntersections, triangles);
            }
        }
    }
}

int CpuParallelRunner::calculateCubeIndex(const GridCell& cell) const
{
    int cubeIndex = 0;

    for (const unsigned int corner : cornerIndices) {
        if (cell.values[corner] < m_isoValue) {
            cubeIndex |= 1 << corner;
        }
    }

    return cubeIndex;
}

std::array<Vec3, 12> CpuParallelRunner::calculateEdgeIntersections(const GridCell& cell, int cubeIndex) const
{
    std::array<Vec3, 12> edgeIntersections{};
    const int edgeMask = EdgeTable[cubeIndex];

    if (edgeMask == 0) {
        return edgeIntersections;
    }

    for (const unsigned int edge : edgeIndices) {
        if ((edgeMask & (1 << edge)) != 0) {
            const auto& edgeCorners = EdgeToVertices[edge];
            const unsigned int firstCorner = static_cast<unsigned int>(edgeCorners.first);
            const unsigned int secondCorner = static_cast<unsigned int>(edgeCorners.second);

            edgeIntersections[edge] = interpolate(cell.positions[firstCorner], cell.values[firstCorner], cell.positions[secondCorner], cell.values[secondCorner]);
        }
    }

    return edgeIntersections;
}

void CpuParallelRunner::buildTriangles(int cubeIndex, const std::array<Vec3, 12>& edgeIntersections, std::vector<Triangle>& triangles) const
{
    const int* triangleEdges = TriangleEdgeTable[cubeIndex];

    for (const unsigned int triangleNumber : triangleIndices) {
        const unsigned int triangleEdge = triangleNumber * 3;
        if (triangleEdges[triangleEdge] == -1) {
            break;
        }

        Triangle triangle;
        for (const unsigned int vertex : triangleVertexIndices) {
            const unsigned int edge = static_cast<unsigned int>(triangleEdges[triangleEdge + vertex]);
            triangle.vertices[vertex] = edgeIntersections[edge];
        }

        triangles.push_back(triangle);
    }
}

Vec3 CpuParallelRunner::interpolate(const Vec3& firstPosition, float firstValue, const Vec3& secondPosition, float secondValue) const
{
    if (std::fabs(m_isoValue - firstValue) < 0.00001f) {
        return firstPosition;
    }

    if (std::fabs(m_isoValue - secondValue) < 0.00001f) {
        return secondPosition;
    }

    if (std::fabs(firstValue - secondValue) < 0.00001f) {
        return firstPosition;
    }

    Vec3 interpolatePosition;
    const float ratio = std::clamp((m_isoValue - firstValue) / (secondValue - firstValue), 0.0f, 1.0f);
    interpolatePosition.x = firstPosition.x + ratio * (secondPosition.x - firstPosition.x);
    interpolatePosition.y = firstPosition.y + ratio * (secondPosition.y - firstPosition.y);
    interpolatePosition.z = firstPosition.z + ratio * (secondPosition.z - firstPosition.z);
    return interpolatePosition;
}

GridCell CpuParallelRunner::buildGridCell(unsigned int x, unsigned int y, unsigned int z) const
{
    GridCell cell;
    for (const unsigned int corner : cornerIndices) {
        const auto& offset = cornerOffsets[corner];
        const unsigned int sampleX = x + offset[0];
        const unsigned int sampleY = y + offset[1];
        const unsigned int sampleZ = z + offset[2];

        cell.positions[corner] = m_scalarData.position(sampleX, sampleY, sampleZ);
        cell.values[corner] = m_scalarData.at(sampleX, sampleY, sampleZ);
    }

    return cell;
}
