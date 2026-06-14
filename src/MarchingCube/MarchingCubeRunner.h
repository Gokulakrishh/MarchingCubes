#pragma once

#include <string>

class IMarchingCubesRunner
{
public:
    virtual ~IMarchingCubesRunner() = default;
    virtual void run() = 0;
};
