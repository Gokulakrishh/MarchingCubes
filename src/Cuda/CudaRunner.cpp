#include "Cuda/CudaRunner.h"

#include <iostream>

void CudaRunner::Run(const std::string& inputFile, const std::string& outputFile)
{
    std::cout << "Running CUDA marching cubes\n";
    std::cout << "Input: " << inputFile << '\n';
    std::cout << "Output: " << outputFile << '\n';
}
