#pragma once

#include "MarchingCube/MarchingCubeRunner.h"

#include <expected>
#include <string>

class CudaRunner : public IMarchingCubesRunner
{
public:
    CudaRunner(const std::string& inputFile, const std::string& outputFile, float isoValue);
    std::expected<void, std::string> run() override;
};
