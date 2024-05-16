#pragma once

#include <memory>
#include "matchlz4.h"

class RambusLZ4 : public LZ4Sequence
{
public:
   static std::shared_ptr<LZ4Sequence> setSequence(uint64_t litLength, uint64_t matchLength, uint64_t matchOffset, const unsigned char* data, bool lastToken);
   static std::shared_ptr<LZ4Sequence> getSequence(const unsigned char* data, uint64_t blockSize);

private:

   RambusLZ4(uint64_t litLength, uint64_t matchLength, uint64_t matchOffset, const unsigned char* data, bool lastToken);
}
