#include "Heterogeneous/HeterogeneousRunner.h"

#include <expected>
#include <iostream>
#include <string>

HeterogeneousRunner::HeterogeneousRunner(const std::string& inputFile, const std::string& outputFile, float isoValue)
{
    (void)isoValue;
    std::cout << "Input: " << inputFile << '\n';
    std::cout << "Output: " << outputFile << '\n';
}

std::expected<void, std::string> HeterogeneousRunner::run()
{
    std::cout << "Running heterogeneous marching cubes\n";
    return {};
}
