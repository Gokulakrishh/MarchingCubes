#pragma once

#include "MarchingCube/MarchingCubeRunner.h"

#include <string>

class HeterogeneousRunner : public IMarchingCubesRunner
{
public:
    HeterogeneousRunner(const std::string& inputFile, const std::string& outputFile);
    void run() override;
};