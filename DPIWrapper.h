#pragma once

#include "HWMatchEngine.h"
#include <cstdint>
#include <memory>
#include <string.h>

#ifndef chandle
#define chandle void *
#endif // !chandle

extern "C" chandle InitCompression();
extern "C" uint32_t PushData(unsigned char data[NB_BYTE], chandle compression_model);
extern "C" uint32_t GetMatch(Matchstruct** match_list, chandle compression_model);
