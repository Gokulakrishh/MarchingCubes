#include "Cpu/CpuRunner.h"
#include "Cuda/CudaRunner.h"
#include "Heterogeneous/HeterogeneousRunner.h"
#include "MarchingCube/MarchingCubeRunner.h"

#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv)
{
    // input file, output file, model(cpu/cuda/heterogeneous)
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <input.txt> <output.ply> <cpu|cuda|heterogeneous>\n";
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    std::string model = argv[3];

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
        runner = std::make_unique<CpuRunner>();
    } else if (model == "cuda") {
        runner = std::make_unique<CudaRunner>();
    } else if (model == "heterogeneous") {
        runner = std::make_unique<HeterogeneousRunner>();
    } else {
        std::cerr << "Model must be cpu, cuda, or heterogeneous\n";
        return 1;
    }

    runner->Run(inputFile, outputFile);
    return 0;
}
