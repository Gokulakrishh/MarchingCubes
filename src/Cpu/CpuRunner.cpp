#include "Cpu/CpuRunner.h"

#include <iostream>

void CpuRunner::Run(const std::string& inputFile, const std::string& outputFile)
{
    std::cout << "Running CPU marching cubes\n";
    std::cout << "Input: " << inputFile << '\n';
    std::cout << "Output: " << outputFile << '\n';
}
