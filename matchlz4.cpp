#pragma push_macro("NDEBUG")
#undef NDEBUG
#include <cassert>
#pragma pop_macro("NDEBUG")

#include <iostream>
#include "matchlz4.h"

LZ4Sequence::LZ4Sequence(uint64_t in_litLength, uint64_t in_matchLength, uint64_t in_matchOffset, const unsigned char* in_data, bool in_lastToken)
   : litLength(in_litLength)
   , matchLength(in_matchLength)
   , matchOffset(in_matchOffset)
   , data(in_data)
   , lastToken(in_lastToken)
{
}

uint64_t LZ4Sequence::getMatchOffset() const
{
   return matchOffset;
}

uint64_t LZ4Sequence::getMatchLength() const
{
   return matchLength + MinMatchLength;
}

uint64_t LZ4Sequence::getLiteralLength() const
{
   return litLength;
}

const unsigned char* LZ4Sequence::getLiteralData() const
{
   return  lz4Literal.data();
}

bool LZ4Sequence::isLastToken() const
{
   return lastToken;
}


uint8_t LZ4Sequence::getToken() const
{
   return token;
}

SmallLZ4 SmallLZ4::setSequence(uint64_t litLength, uint64_t matchLength, uint64_t matchOffset, const unsigned char* data, bool lastToken)
{
   return SmallLZ4(litLength, matchLength, matchOffset, data, lastToken);
}

SmallLZ4::SmallLZ4(uint64_t litLength, uint64_t matchLength, uint64_t matchOffset, const unsigned char* data, bool lastToken)
   : LZ4Sequence(litLength, matchLength, matchOffset, data, lastToken)
{
   uint64_t tokenSize = 1;
   uint64_t offsetSize = 2;
   uint64_t tagLiteral = litLength < 15 ? 0 : ((MaxLengthCode - 15) / 255) + 1;
   uint64_t tagMatch = matchLength < 15 ? 0 : ((MaxLengthCode - 15) / 255) + 1;

   lz4Seq.reserve(tokenSize + offsetSize + tagLiteral + tagMatch);

   // token consists of match length and number of literals, let's start with match length ...
   token = (matchLength < 15) ? (unsigned char)matchLength : 15;

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
      token |= 0xF0;
      lz4Seq.push_back(token);

      // 15 is already encoded in token
      uint64_t encodeNumLiterals = litLength - 15;

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
      lz4Literal.reserve(litLength);
      lz4Literal.insert(lz4Literal.end(), data, data + litLength);
      lz4Seq.insert(lz4Seq.end(), data, data + litLength);
   }
   if (!lastToken) {  // If match length == 0 it is the last sequence

      lz4Seq.push_back(uint8_t(matchOffset & 0xff));
      lz4Seq.push_back(uint8_t(matchOffset >> 8));

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
 
#pragma optimize( "", off )

uint64_t sequenceCall = 0;

SmallLZ4 SmallLZ4::getSequence(const unsigned char* in_data, uint64_t blockSize)
{
   sequenceCall++;
   if (sequenceCall == 2329964) {
      std::cout << "";
   }

   const unsigned char* data = in_data;
   uint64_t blockOffset = 0;

   unsigned char token = *data++;
   blockOffset++;

   // determine number of literals
   unsigned int numLiterals = token >> 4;

   if (numLiterals == 15) {  // number of literals length encoded in more than 1 byte
      unsigned char current;
      do {
         current = *data++;
         numLiterals += current;
         blockOffset++;
      } while (current == 255);
   }
   
   const unsigned char* literalData = data;
   std::vector<unsigned char> lz4Literal;
   lz4Literal.reserve(numLiterals);
   lz4Literal.insert(lz4Literal.end(), data, data + numLiterals);

   data += numLiterals;
   blockOffset += numLiterals;

   // last token has only literals
   if (blockOffset >= blockSize) {
      if (blockOffset == blockSize) {
         return SmallLZ4(numLiterals, 0, 0, literalData, true);
      }
      assert(false);
   }
   // match distance is encoded in two bytes (little endian)

   uint32_t matchOffset = *data++;
   matchOffset |= (unsigned int)*data++ << 8;
   blockOffset += 2;
   assert(matchOffset != 0);

   // match length (always >= 4, therefore length is stored minus 4)
   unsigned int matchLength = 4 + (token & 0x0F);

   if (matchLength == 4 + 0x0F)
   {
      unsigned char current;
      do // match length encoded in more than 1 byte
      {
         current = *data++;
         matchLength += current;
         blockOffset++;
      } while (current == 255);
   }
   // uint32_t litLength, uint32_t matchLength, uint32_t matchOffset, const unsigned char* data, bool lastToken
   return SmallLZ4(numLiterals, matchLength - MinMatchLength, matchOffset, literalData, false);
}
#pragma optimize( "", on )