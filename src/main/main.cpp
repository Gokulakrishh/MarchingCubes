#include "Cpu/CpuRunner.h"
#include "Cpu/CpuParallelRunner.h"
#include "Cuda/CudaRunner.h"
#include "Heterogeneous/HeterogeneousRunner.h"
#include "MarchingCube/MarchingCubeRunner.h"

#include <charconv>
#include <expected>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

int main(int argc, char** argv)
{
    // input file, output file, model(cpu/cuda/heterogeneous)
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <input.txt> <output.ply> <cpu|cpu-parallel|cuda|heterogeneous> <isoValue>\n";
        return 1;
    }

    const std::filesystem::path inputFile = argv[1];
    const std::filesystem::path outputFile = argv[2];
    const std::string model = argv[3];

    float isoValue{};
    const std::string_view isoValueText = argv[4];
    const char* const isoValueBegin = isoValueText.data();
    const char* const isoValueEnd = isoValueBegin + isoValueText.size();
    const auto [parsedEnd, parseError] = std::from_chars(isoValueBegin, isoValueEnd, isoValue);
    if (parseError != std::errc{} || parsedEnd != isoValueEnd) {
        std::cerr << "Iso value must be a valid floating point number\n";
        return 1;
    }

    if (inputFile.extension() != ".txt") {
        std::cerr << "Input file must be a .txt file\n";
        return 1;
    }

    if (outputFile.extension() != ".ply") {
        std::cerr << "Output file must be a .ply file\n";
        return 1;
    }

    std::unique_ptr<IMarchingCubesRunner> runner;

    if (model == "cpu") {
        runner = std::make_unique<CpuRunner>(inputFile.string(), outputFile.string(), isoValue);
    } else if (model == "cpu-parallel") {
        runner = std::make_unique<CpuParallelRunner>(inputFile.string(), outputFile.string(), isoValue);
    } else if (model == "cuda") {
        runner = std::make_unique<CudaRunner>(inputFile.string(), outputFile.string(), isoValue);
    } else if (model == "heterogeneous") {
        runner = std::make_unique<HeterogeneousRunner>(inputFile.string(), outputFile.string(), isoValue);
    } else {
        std::cerr << "Model must be cpu, cpu-parallel, cuda, or heterogeneous\n";
        return 1;
    }

    if (auto result = runner->run(); !result) {
        std::cerr << result.error() << '\n';
        return 1;
    }

    return 0;
}
