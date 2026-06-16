#pragma once

#include <expected>
#include <string>

class IMarchingCubesRunner
{
public:
    virtual ~IMarchingCubesRunner() = default;
    virtual std::expected<void, std::string> run() = 0;
};
