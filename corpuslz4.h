#pragma once

#include <list>
#include <memory>
#include <map>
#include <fstream>

#include "cxxopts.h"


typedef struct ContextLZ4
{
   uint32_t windowSize;
   uint32_t dataSize;
   uint64_t maxChunk;
   uint32_t matchAlgo;

   double   threshold;
   bool     isRounding;
   bool     isDumpOffset;
   bool     isDumpLength;
   bool     isDumpComp;

   std::string corpusSet;
   std::string fileName;
   std::vector<uint32_t> chunkSize;
   std::map <std::string, uint64_t> ratioStat;
   std::map<uint32_t, uint32_t> compLossStatistic;
   std::list<std::pair<uint32_t, uint32_t>> compLossCloud;
   std::vector<std::shared_ptr<std::map<uint32_t, uint32_t>>> chunkAllCompStat = { 5, nullptr };

   std::ofstream statFile;
   std::ofstream statFileDist;
   std::ofstream statFileLength;
   std::ofstream statFileDistLength;
   std::ofstream statFileNewL4;
   std::ofstream dumpFileNewL4;

} ContextLZ4;


class CorpusLZ4
{
public:

   int32_t ParseOption(int argc, const char* argv[], ContextLZ4& contextLZ4);
   int32_t InitCompression(ContextLZ4& lz4Context, LZ4CompReader& lz4Reader, uint64_t chunkIndex);
   int32_t InitDecompression(ContextLZ4& lz4Context, LZ4DecompReader& lz4DecompReader, uint64_t chunkIndex);
   int32_t InitReader(ContextLZ4& lz4Context, LZ4CompReader& lz4Reader, std::shared_ptr<std::ifstream>& inputFile);
   int32_t Compress(ContextLZ4& contextLZ4, LZ4CompReader& lz4Reader, uint32_t chunckIndex);
   int32_t Decompress(ContextLZ4& contextLZ4, LZ4DecompReader& lz4DecompReader, uint32_t chunckIndex);
   int32_t DumpFileStat(ContextLZ4& contextLZ4);
   int32_t DumpChunkStat(ContextLZ4& contextLZ4, LZ4CompReader& lz4Reader, LZ4DecompReader& lz4DecompReader, uint32_t chunckIndex);
   int32_t Close(ContextLZ4& contextLZ4, LZ4CompReader& lz4Reader, LZ4DecompReader& lz4DecompReader);

private:

};

