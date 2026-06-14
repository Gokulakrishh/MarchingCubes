#pragma once

#include <string>

class IMarchingCubesRunner
{
public:
    virtual ~IMarchingCubesRunner() = default;
    virtual void Run(const std::string& inputFile, const std::string& outputFile) = 0;
};
