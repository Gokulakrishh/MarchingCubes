#include "Cpu/CpuRunner.h"

#include <iostream>

CpuRunner::CpuRunner(const std::string& inputFile, const std::string& outputFile)
    : m_scalarData(inputFile),
      m_outputFile(outputFile)
{
    std::cout << "Output: " << outputFile << '\n';
}

void CpuRunner::run()
{
    std::cout << "Running CPU marching cubes\n";
}
