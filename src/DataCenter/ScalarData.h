#pragma once

#include "MarchingCube/MarchingCubeTable.h"
#include "DataCenter/ReadData.h"

#include <cstddef>
#include <cstdint>
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
    float at(std::size_t x, std::size_t y, std::size_t z) const;
    
private:

    bool getHeaderData();
    void getData();
    void processThread(FileChunk chunk);

    std::string m_inputFile;
    std::size_t m_width{};
    std::size_t m_height{};
    std::size_t m_depth{};
    Vec3 m_origin{};
    Vec3 m_spacing{1.0f, 1.0f, 1.0f};
    std::vector<float> m_intensityValues;

};   
 
