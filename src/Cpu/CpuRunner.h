#pragma once

#include "DataCenter/ScalarData.h"
#include "MarchingCube/MarchingCubeRunner.h"

#include <string>

class CpuRunner : public IMarchingCubesRunner
{

public:
    CpuRunner(const std::string& inputFile, const std::string& outputFile);
    void run() override;
private:
    ScalarData m_scalarData;
    std::string m_outputFile;
};
