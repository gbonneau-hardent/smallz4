#include "matchlz4.h"

LZ4Sequence::LZ4Sequence(uint32_t in_litLength, uint32_t in_matchLength, uint32_t in_matchOffset, const unsigned char* in_data, bool in_lastToken)
   : litLength(in_litLength)
   , matchLength(in_matchLength)
   , matchOffset(in_matchOffset)
   , data(in_data)
   , lastToken(in_lastToken)
{
}

SmallLZ4::SmallLZ4(uint32_t litLength, uint32_t matchLength, uint32_t matchOffset, const unsigned char* data, bool lastToken)
   : LZ4Sequence(litLength, matchLength, matchOffset, data, lastToken)
{
   uint32_t tokenSize = 1;
   uint32_t offsetSize = 2;
   uint32_t tagLiteral = litLength < 15 ? 0 : ((MaxLengthCode - 15) / 255) + 1;
   uint32_t tagMatch = matchLength < 15 ? 0 : ((MaxLengthCode - 15) / 255) + 1;

   lz4Seq.reserve(tokenSize + offsetSize + tagLiteral + tagMatch);

   // token consists of match length and number of literals, let's start with match length ...
   unsigned char token = (matchLength < 15) ? (unsigned char)matchLength : 15;

   // >= 15 literals ? (extra bytes to store length)
   if (litLength < 15)
   {
      // add number of literals in higher four bits
      token |= litLength << 4;
      lz4Seq.push_back(token);
   }
   else
   {
      // set all higher four bits, the following bytes with determine the exact number of literals
      lz4Seq.push_back(token | 0xF0);

      // 15 is already encoded in token
      int encodeNumLiterals = int(litLength) - 15;

      // emit 255 until remainder is below 255
      while (encodeNumLiterals >= MaxLengthCode)
      {
         lz4Seq.push_back(MaxLengthCode);
         encodeNumLiterals -= MaxLengthCode;
      }
      // and the last byte (can be zero, too)
      lz4Seq.push_back((unsigned char)encodeNumLiterals);
   }
   // copy literals
   if (litLength > 0)
   {
      lz4Seq.insert(lz4Seq.end(), data, data + litLength);
   }
   if (!lastToken) {  // If match length == 0 it is the last sequence

      lz4Seq.push_back(matchOffset & 0xFF);
      lz4Seq.push_back(matchOffset >> 8);

      // >= 15+4 bytes matched
      if (matchLength >= 15)
      {
         // 15 is already encoded in token
         matchLength -= 15;
         // emit 255 until remainder is below 255
         while (matchLength >= MaxLengthCode)
         {
            lz4Seq.push_back(MaxLengthCode);
            matchLength -= MaxLengthCode;
         }
         // and the last byte (can be zero, too)
         lz4Seq.push_back((unsigned char)matchLength);
      }
   }
}