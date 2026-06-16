#pragma once

#include <expected>
#include "DataCenter/WriteData.h"

class PlyWriter : public IWriteData
{
public:
    std::expected<void, std::string> write(const std::string& outputFile, const std::vector<Triangle>& triangles) const override;
};
