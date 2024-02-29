#pragma once

#include <cstdint>

typedef uint64_t Length;   // a block can be up to 4 MB, so uint32_t would suffice but uint64_t is quite a bit faster on my x64 machine
typedef uint16_t Distance; // matches must start within the most recent 64k

typedef struct Match
{
   Length   length;     // length of match
   Distance distance;   // start of match
   char     character;  // character
} Match;

// write several bytes, see sendBytesToOut() in smallz4.cpp for a basic implementation
typedef void   (*COMP_SEARCH_MATCH)(std::vector<Match>& matches, const uint64_t& blockSize, const unsigned char* dataBlock);

