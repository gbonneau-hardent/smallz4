#pragma once

#include <cstdint>
#include <vector>

#include "matchlz4.h"

void hw_model_compress(std::vector<Match>& matches, const uint64_t& blockSize, const unsigned char* dataBlock);
