#pragma once

#include <inttypes.h> // uint16_t, uint32_t, ...
#include <cstdlib>    // size_t
#include <vector>
#include <memory>
#include <list>
#include <map>
#include <fstream>

#include "matchlz4.h"

// write several bytes, see sendBytesToOut() in smallz4.cpp for a basic implementation
typedef void   (*COMP_SEARCH_MATCH)(std::vector<Match>& matches, const uint64_t& blockSize, const unsigned char* dataBlock);

struct LZ4CompReader
{
   LZ4CompReader()
   {
      std::memset(fileBuffer.get(), 0, 131072);
      std::memset(compBuffer.get(), 0, 131072);
      std::memset(inputBuffer.get(), 0, 131072);
   }

   std::shared_ptr<char> fileBuffer = std::shared_ptr<char>(new char[131072]);
   std::shared_ptr<char> compBuffer = std::shared_ptr<char>(new char[131072]);
   std::shared_ptr<char> inputBuffer = std::shared_ptr<char>(new char[131072]);

   uint64_t totalChunkCount = 0;
   uint64_t totalChunkCompress = 0;
   uint64_t totalSizeRead = 0;
   uint64_t totalSizeReadStat = 0;
   uint64_t totalSizeCompressStat = 0;
   uint64_t totalSizeCompress = 0;
   uint64_t totalMBytes = 0;
   uint64_t lastMBytes = 0;
   uint64_t loopCount = 0;
   uint64_t dataChunkSize = 0;
   uint64_t dataReadSize = 0;
   uint64_t dataCompressSize = 0;
   uint64_t compThreshold = 0;
   uint64_t chunkSize = 0;
   uint64_t srcSize = 0;
   uint32_t maxCompSize = 0;
   bool     dataEof = false;

   std::string corpusName = "";
   std::map<uint32_t, uint32_t> compStatistic;
   std::shared_ptr<std::istream> srcFile = nullptr;
   std::map <std::istream*, std::string> corpusFileSet;
   std::list<std::shared_ptr<std::ifstream>> corpusList;
   std::list<std::shared_ptr<std::ifstream>>::iterator iterFile;
};


class smallz4
{
public:

   // read  several bytes, see getBytesFromIn() in smallz4.cpp for a basic implementation
   typedef size_t(*COMP_GET_BYTES) (void* data, size_t numBytes, void* userPtr);

   // write several bytes, see sendBytesToOut() in smallz4.cpp for a basic implementation
   typedef void   (*COMP_SEND_BYTES)(const void* data, size_t numBytes, void* userPtr);

   /// version string
   static const char* const getVersion();

   // compression level thresholds, made public because I display them in the help screen ...
   enum
   {
      /// greedy mode for short chains (compression level <= 3) instead of optimal parsing / lazy evaluation
      ShortChainsGreedy = 3,
      /// lazy evaluation for medium-sized chains (compression level > 3 and <= 6)
      ShortChainsLazy = 6
   };

   // ----- END OF PUBLIC INTERFACE -----
private:

   // ----- constants and types -----

   enum
   {
      /// each match's length must be >= 4
      MinMatch = 4,
      /// a literal needs one byte
      JustLiteral = 1,
      /// last match must not be closer than 12 bytes to the end
      BlockEndNoMatch = 12,
      /// last 5 bytes must be literals, no matching allowed
      BlockEndLiterals = 5,

      /// match finder's hash table size (2^HashBits entries, must be less than 32)
      HashBits = 20,
      HashSize = 1 << HashBits,

      /// input buffer size, can be any number but zero ;-)
      BufferSize = 64 * 1024,

      /// maximum match distance, must be power of 2 minus 1
      MaxDistance = 65535,
      /// marker for "no match"
      EndOfChain = 0,
      /// stop match finding after MaxChainLength steps (default is unlimited => optimal parsing)
      MaxChainLength = MaxDistance,

      /// significantly speed up parsing if the same byte is repeated a lot, may cause sub-optimal compression
      MaxSameLetter = 19 + 255 * 256, // was: 19 + 255,

      /// maximum block size as defined in LZ4 spec: { 0,0,0,0,64*1024,256*1024,1024*1024,4*1024*1024 }
      /// I only work with the biggest maximum block size (7)
      //  note: xxhash header checksum is precalculated only for 7, too
      MaxBlockSizeId = 7,
      MaxBlockSize = 4 * 1024 * 1024,

      /// legacy format has a fixed block size of 8 MB
      MaxBlockSizeLegacy = 8 * 1024 * 1024,

      /// number of literals and match length is encoded in several bytes, max. 255 per byte
      MaxLengthCode = 255
   };

   //  ----- one and only variable ... -----

   /// how many matches are checked in findLongestMatch, lower values yield faster encoding at the cost of worse compression ratio
   unsigned short maxChainLength;

   //  ----- code -----


   /// create new compressor (only invoked by lz4)
   explicit smallz4(unsigned short newMaxChainLength = MaxChainLength)
      : maxChainLength(newMaxChainLength) // => no limit, but can be changed by setMaxChainLength
   {
   };


   /// return true, if the four bytes at *a and *b match
   inline static bool match4(const void* const a, const void* const b)
   {
      return *(const uint32_t*)a == *(const uint32_t*)b;
   };

   /// simple hash function, input: 32 bits, output: HashBits bits (by default: 20)
   inline static uint32_t getHash32(uint32_t fourBytes)
   {
      // taken from https://en.wikipedia.org/wiki/Linear_congruential_generator
      const uint32_t HashMultiplier = 48271;
      return ((fourBytes * HashMultiplier) >> (32 - HashBits)) & (HashSize - 1);
   };

   public:
      
 
   /// compress everything in input stream (accessed via getByte) and write to output stream (via send)
   static void lz4(COMP_GET_BYTES getBytes, COMP_SEND_BYTES sendBytes, COMP_SEARCH_MATCH matchAlgorithm,
      unsigned short maxChainLength,
      const std::vector<unsigned char>& dictionary, // predefined dictionary
      bool useLegacyFormat = false,                 // old format is 7 bytes smaller if input < 8 MB
      void* userPtr = NULL,
      bool isLess64Illegal = false);
 
private:

   /// compress everything in input stream (accessed via getByte) and write to output stream (via send)
   static void lz4(COMP_GET_BYTES getBytes, COMP_SEND_BYTES sendBytes, COMP_SEARCH_MATCH matchAlgorithm, unsigned short maxChainLength = MaxChainLength, bool useLegacyFormat = false, void* userPtr = NULL);

   void compress(COMP_GET_BYTES getBytes, COMP_SEND_BYTES sendBytes, COMP_SEARCH_MATCH matchAlgorithm, const std::vector<unsigned char>& dictionary, bool useLegacyFormat, void* userPtr, bool isLess64Illegal) const;

   //void hw_model_compress(std::vector<smallz4::Match>& matches, const uint64_t& blockSize, const unsigned char* dataBlock) const;

   static void estimateCosts(std::vector<Match>& matches);

   static std::vector<unsigned char> selectBestMatches(const std::vector<Match>& matches, const unsigned char* const data);

   Match findLongestMatch(const unsigned char* const data, uint64_t pos, uint64_t begin, uint64_t end, const Distance* const chain) const;

   Match findMatch(uint32_t index, uint32_t matchIndex, std::vector<Match>& matches) const;

   bool findMatchFarther(uint32_t refIndex, std::vector<Match>& matches) const;

};