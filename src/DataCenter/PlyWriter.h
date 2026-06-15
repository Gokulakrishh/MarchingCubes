#pragma once

#include "DataCenter/WriteData.h"

class PlyWriter : public IWriteData
{
public:
    bool write(const std::string& outputFile, const std::vector<Triangle>& triangles) const override;
};
