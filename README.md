# Marching Cubes

This project implements the Marching Cubes surface reconstruction algorithm in C++23 and CUDA/Thrust.

It reads a regular scalar field from a text file, extracts an isosurface, generates triangles, and writes the result as an ASCII `.ply` mesh.


## Current Large Benchmark

These are algorithm measurements, not complete application runtime.

The current comparison uses a larger local CT scalar field:

- Grid: `128 x 128 x 267`
- Generated triangles: `1,515,424`

Lower is better. One `#` represents approximately `33 ms`.

```text
CPU, 1 thread    | ##################################################  1635.44 ms
CPU, 8 threads   | #######                                              238.70 ms
CPU, 16 threads  | #####                                                159.89 ms
GPU, Thrust      | #                                                     43.84 ms
```

For this workload, the heterogeneous GPU path is:

- `37.3x` faster than the sequential CPU implementation.
- `5.4x` faster than the 8-thread CPU implementation.
- `3.6x` faster than the 16-thread CPU implementation.

## Build

Requirements:

- CMake 3.22 or newer.
- A C++23 compiler.
- CUDA 12.4 or newer.

CUDA is required. CMake configuration stops with an error when a supported CUDA compiler is unavailable.

```bash
cmake -S . -B build
cmake --build build -j
```

When multiple CUDA versions are installed, select the supported NVCC explicitly:

```bash
cmake --fresh -S . -B build \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-13.3/bin/nvcc
cmake --build build -j
```

## Run

```bash
./build/MarchingCubes <input.txt> <output.ply> <cpu|cpu-parallel|cuda|heterogeneous> <isoValue> [cpu-parallel-threads]
```

The optional final argument caps worker threads for `cpu-parallel` runs. If omitted, `cpu-parallel` uses the available hardware thread count.

Examples:

```bash
./build/MarchingCubes files/input.txt output.ply cpu 0.45
./build/MarchingCubes files/input.txt output.ply cpu-parallel 0.45 8
./build/MarchingCubes files/input.txt output.ply heterogeneous 0.45
```

## Historical CPU Benchmark

The earlier CPU comparison used `files/input.txt`, a `69 x 64 x 72` grid producing `58,320` triangles at iso value `0.45`.

Lower is better. One `#` represents approximately `4 ms`.

```text
Intel i7-4870HQ, CPU 1     | ##########################################  169.18 ms
Intel i7-4870HQ, CPU 8     | ########                                    33.52 ms
Core Ultra 7 255H, CPU 1   | ###################                         74.11 ms
Core Ultra 7 255H, CPU 8   | ######                                      23.44 ms
Core Ultra 7 255H, CPU 16  | #####                                       19.90 ms
```

<p>
  <img src="files/snapshot00.png" alt="Rendered Marching Cubes brain mesh view 1" width="49%">
  <img src="files/snapshot01.png" alt="Rendered Marching Cubes brain mesh view 2" width="49%">
</p>


## Algorithm Reference

The Marching Cubes implementation is based on Paul Bourke's polygonising scalar field reference:

https://paulbourke.net/geometry/polygonise/
