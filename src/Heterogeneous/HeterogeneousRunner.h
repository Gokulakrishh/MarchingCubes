#pragma once

#include "MarchingCube/MarchingCubeRunner.h"

#include <cstddef>
#include <expected>
#include <string>

struct DeviceVec3
{
    float x{};
    float y{};
    float z{};
};

struct DeviceTriangle
{
    DeviceVec3 vertices[3]{};
};

struct GpuContext
{
    const float* values{};
    unsigned int width{};
    unsigned int height{};
    unsigned int depth{};
    float isoValue{};

    DeviceVec3 origin{};
    DeviceVec3 spacing{};

    const int* edgeTable{};
    const int* triangleEdgeTable{};
};

class HeterogeneousRunner : public IMarchingCubesRunner
{
public:
    HeterogeneousRunner(const std::string& inputFile, const std::string& outputFile, float isoValue);
    std::expected<void, std::string> run() override;

private:
    std::string m_inputFile;
    std::string m_outputFile;
    float m_isoValue{};
};
