#pragma once

#include "MarchingCube/MarchingCubeTable.h"
#include "DataCenter/ReadData.h"

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
    unsigned int width() const;
    unsigned int height() const;
    unsigned int depth() const;
    float at(unsigned int x, unsigned int y, unsigned int z) const;
    Vec3 position(unsigned int x, unsigned int y, unsigned int z) const;
    
private:

    bool getHeaderData();
    void getData();
    void processThread(FileChunk chunk);

    std::string m_inputFile;
    unsigned int m_width{};
    unsigned int m_height{};
    unsigned int m_depth{};
    Vec3 m_origin{};
    Vec3 m_spacing{1.0f, 1.0f, 1.0f};
    std::vector<float> m_intensityValues;

};   
 
