#pragma once

#include "MarchingCube/MarchingCubeRunner.h"

#include <expected>
#include <string>

class HeterogeneousRunner : public IMarchingCubesRunner
{
public:
    HeterogeneousRunner(const std::string& inputFile, const std::string& outputFile, float isoValue);
    std::expected<void, std::string> run() override;
};