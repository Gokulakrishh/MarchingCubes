#pragma once

#include <cuda_runtime.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cuda_runner_detail
{
inline constexpr float kIsoEpsilon = 0.00001f;
inline constexpr unsigned int kCornerCount = 8;
inline constexpr unsigned int kEdgeCount = 12;
inline constexpr unsigned int kTriangleSlotCount = 5;
inline constexpr unsigned int kTriangleVertexCount = 3;
inline constexpr unsigned int kTriangleEdgeRowWidth = 16;
inline constexpr unsigned int kThreadsPerBlock = 256;

struct CudaVec3
{
    float x{};
    float y{};
    float z{};
};

struct CudaTriangle
{
    CudaVec3 vertices[kTriangleVertexCount]{};
};

struct KernelContext
{
    const float* values{};
    unsigned int width{};
    unsigned int height{};
    unsigned int depth{};
    float isoValue{};
    CudaVec3 origin{};
    CudaVec3 spacing{};
};

inline void checkCuda(cudaError_t result, std::string_view operation)
{
    if (result != cudaSuccess) {
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(result));
    }
}

template <typename T>
class DeviceBuffer
{
public:
    DeviceBuffer() = default;

    explicit DeviceBuffer(std::size_t count)
    {
        if (count != 0) {
            checkCuda(cudaMalloc(reinterpret_cast<void**>(&m_data), count * sizeof(T)), "cudaMalloc");
        }
    }

    ~DeviceBuffer()
    {
        if (m_data != nullptr) {
            cudaFree(m_data);
        }
    }

    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
    DeviceBuffer(DeviceBuffer&&) = delete;
    DeviceBuffer& operator=(DeviceBuffer&&) = delete;

    T* data()
    {
        return m_data;
    }

    const T* data() const
    {
        return m_data;
    }

private:
    T* m_data{};
};

template <typename T>
class PinnedHostBuffer
{
public:
    explicit PinnedHostBuffer(std::size_t count)
        : m_count(count)
    {
        if (count != 0) {
            checkCuda(cudaMallocHost(reinterpret_cast<void**>(&m_data), count * sizeof(T)), "cudaMallocHost");
        }
    }

    ~PinnedHostBuffer()
    {
        if (m_data != nullptr) {
            cudaFreeHost(m_data);
        }
    }

    PinnedHostBuffer(const PinnedHostBuffer&) = delete;
    PinnedHostBuffer& operator=(const PinnedHostBuffer&) = delete;
    PinnedHostBuffer(PinnedHostBuffer&&) = delete;
    PinnedHostBuffer& operator=(PinnedHostBuffer&&) = delete;

    T* data()
    {
        return m_data;
    }

    const T* data() const
    {
        return m_data;
    }

    std::size_t size() const
    {
        return m_count;
    }

private:
    T* m_data{};
    std::size_t m_count{};
};

class CudaStream
{
public:
    CudaStream()
    {
        checkCuda(cudaStreamCreate(&m_stream), "cudaStreamCreate");
    }

    ~CudaStream()
    {
        if (m_stream != nullptr) {
            cudaStreamDestroy(m_stream);
        }
    }

    CudaStream(const CudaStream&) = delete;
    CudaStream& operator=(const CudaStream&) = delete;

    cudaStream_t get() const
    {
        return m_stream;
    }

private:
    cudaStream_t m_stream{};
};
} // namespace cuda_runner_detail
