#include "DataCenter/ScalarData.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

inline bool readFloat(std::string_view& cursor, float& value)
{
    while (!cursor.empty() && std::isspace(static_cast<unsigned char>(cursor.front())) != 0) {
        cursor.remove_prefix(1);
    }

    auto result = std::from_chars(cursor.data(), cursor.data() + cursor.size(), value);

    if (result.ec != std::errc()) {
        return false;
    }

    cursor.remove_prefix(result.ptr - cursor.data());
    return true;
}

inline bool parseScalarLine(const std::string& line, float& x, float& y, float& z, float& intensity)
{
    std::string_view cursor = line;
    return readFloat(cursor, x) &&
           readFloat(cursor, y) &&
           readFloat(cursor, z) &&
           readFloat(cursor, intensity);
}

ScalarData::ScalarData(const std::string& inputFile)
    : m_inputFile(inputFile)
{
    std::cout << "Input: " << inputFile << '\n';
    if (!getHeaderData()) {
        return;
    }
    getData();
}

bool ScalarData::valid() const
{
    return !m_error.has_value();
}

const std::optional<std::string>& ScalarData::error() const
{
    return m_error;
}

unsigned int ScalarData::width() const
{
    return m_width;
}

unsigned int ScalarData::height() const
{
    return m_height;
}

unsigned int ScalarData::depth() const
{
    return m_depth;
}

float ScalarData::at(unsigned int x, unsigned int y, unsigned int z) const
{
    if (x >= m_width || y >= m_height || z >= m_depth || m_intensityValues.empty()) {
        return 0.0f;
    }

    return m_intensityValues[x + m_width * (y + m_height * z)];
}

Vec3 ScalarData::position(unsigned int x, unsigned int y, unsigned int z) const
{
    return {
        m_origin.x + static_cast<float>(x) * m_spacing.x,
        m_origin.y + static_cast<float>(y) * m_spacing.y,
        m_origin.z + static_cast<float>(z) * m_spacing.z
    };
}

bool ScalarData::getHeaderData()
{
    std::ifstream file(m_inputFile);
    if (!file) {
        m_error = "Cannot open input file: " + m_inputFile;
        return false;
    }

    bool foundGrid{};
    bool foundSpacing{};
    bool foundOrigin{};

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (line[0] != '#') {
            break;
        }

        if (line.starts_with("# Grid:")) {
            std::istringstream stream(line.substr(7));
            char firstSeparator{};
            char secondSeparator{};

            if (!(stream >> m_width >> firstSeparator >> m_height >> secondSeparator >> m_depth) ||
                firstSeparator != 'x' || secondSeparator != 'x') {
                m_error = "Invalid grid header: " + line;
                return false;
            }

            foundGrid = true;
            continue;
        }

        if (line.starts_with("# Voxel spacing:")) {
            std::istringstream stream(line.substr(16));
            char firstSeparator{};
            char secondSeparator{};

            if (!(stream >> m_spacing.x >> firstSeparator >> m_spacing.y >> secondSeparator >> m_spacing.z) ||
                firstSeparator != 'x' || secondSeparator != 'x') {
                m_error = "Invalid voxel spacing header: " + line;
                return false;
            }

            foundSpacing = true;
            continue;
        }

        if (line.starts_with("# origin_mm")) {
            const auto separatorPosition = line.find('=');
            if (separatorPosition == std::string::npos) {
                m_error = "Invalid origin header: " + line;
                return false;
            }

            std::istringstream stream(line.substr(separatorPosition + 1));
            if (!(stream >> m_origin.x >> m_origin.y >> m_origin.z)) {
                m_error = "Invalid origin header: " + line;
                return false;
            }

            foundOrigin = true;
        }
    }

    if (!foundGrid) {
        m_error = "Grid header was not found in: " + m_inputFile;
        return false;
    }

    if (!foundSpacing) {
        m_error = "Voxel spacing header was not found in: " + m_inputFile;
        return false;
    }

    if (!foundOrigin) {
        m_error = "Origin header was not found in: " + m_inputFile;
        return false;
    }

    m_intensityValues.resize(m_width * m_height * m_depth, 0.0f);
    return true;
}

void ScalarData::getData()
{
    std::error_code error;
    const auto filesize = std::filesystem::file_size(m_inputFile, error);
    if (error) {
        m_error = "Cannot get input file size: " + m_inputFile;
        return;
    }

    if (filesize == 0) {
        return;
    }
    // Avoid starting worker threads for very small file ranges.
    constexpr std::uintmax_t minChunkBytes = 256ULL * 1024;
    const auto hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    const auto maxThreadsByFile = std::max<std::uintmax_t>(1, filesize / minChunkBytes);
    const auto nthreads = std::min<std::uintmax_t>(hardwareThreads, maxThreadsByFile);
    const auto chunkSize = filesize / nthreads;
    const auto threadCount = static_cast<unsigned int>(nthreads);
    
    std::vector<FileChunk> chunks;
    std::vector<std::future<std::expected<void, std::string>>> workerThreads;
    chunks.reserve(threadCount);
    workerThreads.reserve(threadCount);

    for (unsigned int i = 0; i < threadCount; ++i) {
        const auto begin = i * chunkSize;
        const auto end = (i == threadCount - 1) ? filesize : begin + chunkSize;
        chunks.push_back(FileChunk{begin, end});
    }

    for (const FileChunk chunk : chunks) {
        workerThreads.push_back(std::async(std::launch::async, &ScalarData::processThread, this, chunk));
    }

    for (auto& workerThread : workerThreads) {
        if (auto result = workerThread.get(); !result && !m_error) {
            m_error = result.error();
        }
    }
}

std::expected<void, std::string> ScalarData::processThread(FileChunk chunk)
{
    std::ifstream file(m_inputFile, std::ios::binary);
    if (!file) {
        return std::unexpected("Cannot open input file: " + m_inputFile);
    }

    file.seekg(static_cast<std::streamoff>(chunk.begin));

    if (chunk.begin != 0) {
        std::string partialLine;
        std::getline(file, partialLine);
    }

    std::string line;
    while (file) {
        const auto lineStart = file.tellg();
        if (lineStart == std::streampos(-1) ||
            static_cast<std::uintmax_t>(lineStart) >= chunk.end) {
            break;
        }

        if (!std::getline(file, line)) {
            break;
        }

        if (line.empty() || line[0] == '#') {
            continue;
        }

        float x{};
        float y{};
        float z{};
        float intensity{};
        if (!parseScalarLine(line, x, y, z, intensity)) {
            continue;
        }

        const auto i = static_cast<long long>(std::llround((x - m_origin.x) / m_spacing.x));
        const auto j = static_cast<long long>(std::llround((y - m_origin.y) / m_spacing.y));
        const auto k = static_cast<long long>(std::llround((z - m_origin.z) / m_spacing.z));

        if (i < 0 || j < 0 || k < 0 ||
            i >= static_cast<long long>(m_width) ||
            j >= static_cast<long long>(m_height) ||
            k >= static_cast<long long>(m_depth)) {
            continue;
        }

        const unsigned int gridX = static_cast<unsigned int>(i);
        const unsigned int gridY = static_cast<unsigned int>(j);
        const unsigned int gridZ = static_cast<unsigned int>(k);
        const unsigned int index = gridX + m_width * (gridY + m_height * gridZ);
        m_intensityValues[index] = intensity;
    }

    return {};
}
