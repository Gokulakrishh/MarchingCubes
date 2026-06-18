#pragma once

#include "MarchingCube/MarchingCubeTable.h"
#include "DataCenter/ReadData.h"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

struct FileChunk {
      std::uintmax_t begin{};
      std::uintmax_t end{};
  };

class ScalarData : public IReadData
{
public:
    ScalarData(const std::string& inputFile);
    unsigned int width() const;
    unsigned int height() const;
    unsigned int depth() const;
    float at(unsigned int x, unsigned int y, unsigned int z) const;
    Vec3 position(unsigned int x, unsigned int y, unsigned int z) const;
    bool valid() const;
    const std::optional<std::string>& error() const;
    const std::vector<float>& values() const
{
    return m_intensityValues;
}
    
private:

    bool getHeaderData();
    void getData();
    std::expected<void, std::string> processThread(FileChunk chunk);

    std::string m_inputFile;
    unsigned int m_width{};
    unsigned int m_height{};
    unsigned int m_depth{};
    Vec3 m_origin{};
    Vec3 m_spacing{1.0f, 1.0f, 1.0f};
    std::vector<float> m_intensityValues;
    std::optional<std::string> m_error;

};   
 
