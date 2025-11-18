#pragma once
#include "common.h"
#include "object.h"
#include <vector>
using namespace Object;

struct ByteCode
{
    std::vector<uint8_t> block;
    std::vector<BaseUP> pool;
};