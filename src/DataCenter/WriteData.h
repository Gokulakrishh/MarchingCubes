#pragma once

#include "MarchingCube/MarchingCubeTable.h"

#include <string>
#include <vector>

class IWriteData
{
public:
    virtual ~IWriteData() = default;
    virtual bool write(const std::string& outputFile, const std::vector<Triangle>& triangles) const = 0;
};
