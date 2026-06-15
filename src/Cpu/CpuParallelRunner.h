#pragma once

#include "DataCenter/ScalarData.h"
#include "MarchingCube/MarchingCubeRunner.h"

#include <array>
#include <string>
#include <vector>

class CpuParallelRunner : public IMarchingCubesRunner
{
public:
    CpuParallelRunner(const std::string& inputFile, const std::string& outputFile, float isoValue);
    void run() override;

private:
    void processThread(unsigned int chunkBegin, unsigned int chunkEnd, std::vector<Triangle>& triangles) const;
    int calculateCubeIndex(const GridCell& cell) const;
    std::array<Vec3, 12> calculateEdgeIntersections(const GridCell& cell, int cubeIndex) const;
    void buildTriangles(int cubeIndex, const std::array<Vec3, 12>& edgeIntersections, std::vector<Triangle>& triangles) const;
    Vec3 interpolate(const Vec3& firstPosition, float firstValue, const Vec3& secondPosition, float secondValue) const;
    GridCell buildGridCell(unsigned int x, unsigned int y, unsigned int z) const;

    ScalarData m_scalarData;
    std::string m_outputFile;
    float m_isoValue {};
};
