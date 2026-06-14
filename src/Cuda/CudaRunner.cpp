#include "Cuda/CudaRunner.h"

#include <iostream>

CudaRunner::CudaRunner(const std::string& inputFile, const std::string& outputFile)
{
    std::cout << "Input: " << inputFile << '\n';
    std::cout << "Output: " << outputFile << '\n';
}

void CudaRunner::run()
{
    std::cout << "Running cuda marching cubes\n";
}
