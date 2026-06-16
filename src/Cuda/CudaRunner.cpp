#include "Cuda/CudaRunner.h"

#include <expected>
#include <iostream>
#include <string>

CudaRunner::CudaRunner(const std::string& inputFile, const std::string& outputFile, float isoValue)
{
    (void)isoValue;
    std::cout << "Input: " << inputFile << '\n';
    std::cout << "Output: " << outputFile << '\n';
}

std::expected<void, std::string> CudaRunner::run()
{
    std::cout << "Running cuda marching cubes\n";
    return {};
}
