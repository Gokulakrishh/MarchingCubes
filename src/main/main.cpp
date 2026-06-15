#include "Cpu/CpuRunner.h"
#include "Cpu/CpuParallelRunner.h"
#include "Cuda/CudaRunner.h"
#include "Heterogeneous/HeterogeneousRunner.h"
#include "MarchingCube/MarchingCubeRunner.h"

#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv)
{
    // input file, output file, model(cpu/cuda/heterogeneous)
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <input.txt> <output.ply> <cpu|cpu-parallel|cuda|heterogeneous> <isoValue>\n";
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    std::string model = argv[3];
    float isoValue = std::stof(argv[4]);

    if (!inputFile.ends_with(".txt")) {
        std::cerr << "Input file must be a .txt file\n";
        return 1;
    }

    if (!outputFile.ends_with(".ply")) {
        std::cerr << "Output file must be a .ply file\n";
        return 1;
    }

    std::unique_ptr<IMarchingCubesRunner> runner;

    if (model == "cpu") {
        runner = std::make_unique<CpuRunner>(inputFile, outputFile, isoValue);
    } else if (model == "cpu-parallel") {
        runner = std::make_unique<CpuParallelRunner>(inputFile, outputFile, isoValue);
    } else if (model == "cuda") {
        runner = std::make_unique<CudaRunner>(inputFile, outputFile, isoValue);
    } else if (model == "heterogeneous") {
        runner = std::make_unique<HeterogeneousRunner>(inputFile, outputFile, isoValue);
    } else {
        std::cerr << "Model must be cpu, cpu-parallel, cuda, or heterogeneous\n";
        return 1;
    }

    runner->run();
    return 0;
}
