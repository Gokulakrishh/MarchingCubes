#include "Heterogeneous/HeterogeneousRunner.h"

#include <iostream>

HeterogeneousRunner::HeterogeneousRunner(const std::string& inputFile, const std::string& outputFile)
{
    std::cout << "Input: " << inputFile << '\n';
    std::cout << "Output: " << outputFile << '\n';
}

void HeterogeneousRunner::run()
{
    std::cout << "Running heterogeneous marching cubes\n";
}
