#pragma once

#include <cstdint>
#include <vector>

typedef uint64_t Length;   // a block can be up to 4 MB, so uint32_t would suffice but uint64_t is quite a bit faster on my x64 machine
typedef uint16_t Distance; // matches must start within the most recent 64k

typedef struct Match
{
   Length   length;     // length of match
   Distance distance;   // start of match
   char     character;  // character
} Match;


class LZ4Sequence
{
public:

   LZ4Sequence(uint32_t litLength, uint32_t matchLength, uint32_t matchOffset, const unsigned char* data, bool lastToken);
   virtual ~LZ4Sequence() {}

   virtual const std::vector<unsigned char>& getSequence() const { return lz4Seq; }

protected:

   static const uint32_t MaxLengthCode = 255;

   uint32_t litLength;
   uint32_t matchLength;
   uint32_t matchOffset;
   bool     lastToken;
   const unsigned char* const data;

   std::vector<unsigned char> lz4Seq;
};


class SmallLZ4 : public LZ4Sequence
{
public:
   SmallLZ4(uint32_t litLength, uint32_t matchLength, uint32_t matchOffset, const unsigned char* data, bool lastToken);
};


