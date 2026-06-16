#pragma once

#include "MarchingCube/MarchingCubeTable.h"

#include <expected>
#include <string>
#include <vector>

class IWriteData
{
public:
    virtual ~IWriteData() = default;
    virtual std::expected<void, std::string> write(const std::string& outputFile, const std::vector<Triangle>& triangles) const = 0;
};
