#pragma once

#include "MarchingCube/MarchingCubeRunner.h"

#include <string>

class HeterogeneousRunner : public IMarchingCubesRunner
{
public:
    void Run(const std::string& inputFile, const std::string& outputFile) override;
};
