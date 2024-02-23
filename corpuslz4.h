#pragma once

#include <list>
#include <memory>
#include <map>

#include "cxxopts.h"

typedef struct lz4Token
{
   unsigned char token;
   unsigned short literalLength;
   unsigned short offset;
   unsigned short matchLength;
   std::list<unsigned char> literal;

} lz4Token;


struct LZ4DecompReader
{
   std::shared_ptr<char> decompBuffer = std::shared_ptr<char>(new char[131072]);
   char* compBuffer;
   unsigned int  available;
   unsigned int  decompPos;

   std::map<uint64_t, uint64_t> chunkStat;
   std::list<lz4Token> listSequence;
   std::map<uint64_t, uint64_t> mapDist;
   std::map<uint64_t, uint64_t> mapLength;
   std::map<uint64_t, std::map<uint64_t, uint64_t>> mapDistLength;
};


struct LZ4CompReader
{
   LZ4CompReader()
   {
      std::memset(fileBuffer.get(), 0, 131072);
      std::memset(compBuffer.get(), 0, 131072);
   }

   std::shared_ptr<char> fileBuffer = std::shared_ptr<char>(new char[131072]);
   std::shared_ptr<char> compBuffer = std::shared_ptr<char>(new char[131072]);

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
   bool     dataEof = false;

   std::shared_ptr<std::ifstream> srcFile = nullptr;
};


struct LZ4Corpus
{
   LZ4DecompReader decompReader;
   std::shared_ptr<cxxopts::Options> optionArg;
};