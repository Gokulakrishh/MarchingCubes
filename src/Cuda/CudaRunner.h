#pragma once

#include "MarchingCube/MarchingCubeRunner.h"

#include <string>

class CudaRunner : public IMarchingCubesRunner
{
public:
    CudaRunner(const std::string& inputFile, const std::string& outputFile, float isoValue);
    void run() override;
};
