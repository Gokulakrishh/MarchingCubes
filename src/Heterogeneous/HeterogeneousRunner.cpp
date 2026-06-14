#include "Heterogeneous/HeterogeneousRunner.h"

#include <iostream>

void HeterogeneousRunner::Run(const std::string& inputFile, const std::string& outputFile)
{
    std::cout << "Running heterogeneous marching cubes\n";
    std::cout << "Input: " << inputFile << '\n';
    std::cout << "Output: " << outputFile << '\n';
}
