#pragma once

#include <cstdint>
#include <vector>
#include <memory>

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

   LZ4Sequence(uint64_t litLength, uint64_t matchLength, uint64_t matchOffset, const unsigned char* data, bool lastToken);
   virtual ~LZ4Sequence() {}

   virtual const std::vector<unsigned char>& getSeqData() const { return lz4Seq; }
   virtual uint8_t  getToken() const;
   virtual uint64_t getMatchOffset() const;
   virtual uint64_t getMatchLength() const;
   virtual uint64_t getLiteralLength() const;
   virtual bool     isLastToken() const;
   virtual const unsigned char* getLiteralData() const;

   static const uint64_t MaxLengthCode = 255;
   static const uint64_t MinMatchLength = 4;

protected:

   uint8_t  token;
   uint64_t litLength;
   uint64_t matchLength;
   uint64_t matchOffset;
   bool     lastToken;

   const unsigned char* const data;

   std::vector<unsigned char> lz4Literal;
   std::vector<unsigned char> lz4Seq;
};


class LZ4Factory
{
public:

   virtual std::shared_ptr<LZ4Sequence> setSequence(uint64_t litLength, uint64_t matchLength, uint64_t matchOffset, const unsigned char* data, bool lastToken) = 0;
   virtual std::shared_ptr<LZ4Sequence> getSequence(const unsigned char* data, uint64_t blockSize) = 0;
};


class SmallLZ4Factory : public LZ4Factory
{
public:
   virtual std::shared_ptr<LZ4Sequence> setSequence(uint64_t litLength, uint64_t matchLength, uint64_t matchOffset, const unsigned char* data, bool lastToken);
   virtual std::shared_ptr<LZ4Sequence> getSequence(const unsigned char* data, uint64_t blockSize);

private:

};


class SmallLZ4 : public LZ4Sequence
{
public:

   SmallLZ4(uint64_t litLength, uint64_t matchLength, uint64_t matchOffset, const unsigned char* data, bool lastToken);
};


