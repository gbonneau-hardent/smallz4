#pragma once

#include <list>
#include <memory>
#include <map>
#include <fstream>
#include <iostream>

#include "DPICall.h"
#include "cxxopts.h"
#include "complz4.h"
#include "decomplz4.h"


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
   bool     isStatGnuplot;

   std::string corpusSet;
   std::string fileName;
   std::string command;
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

   COMP_SEARCH_MATCH matchEngine;

} ContextLZ4;


class CorpusLZ4 : public DPICall, public std::enable_shared_from_this<DPICall>
{
public:

   CorpusLZ4() {
   }

   ~CorpusLZ4() {

      std::cout << "";
   }


   std::shared_ptr<CorpusLZ4> Reference()
   {
      if (selfReference == nullptr) {
         auto dpiCallReference = shared_from_this();
         selfReference = std::dynamic_pointer_cast<CorpusLZ4>(dpiCallReference);
      }
      return selfReference;
   }

   virtual chandle CreateContext();
   virtual void DestroyContext();

   virtual int32_t ParseOption(int argc, const char* argv[], ContextLZ4& contextLZ4);
   virtual int32_t InitCompression(ContextLZ4& lz4Context, LZ4CompReader& lz4Reader, uint64_t chunkIndex);
   virtual int32_t InitDecompression(ContextLZ4& lz4Context, LZ4DecompReader& lz4DecompReader, uint64_t chunkIndex);
   virtual int32_t InitReader(ContextLZ4& lz4Context, LZ4CompReader& lz4Reader, std::shared_ptr<std::ifstream>& inputFile);
   virtual int32_t Compress(ContextLZ4& contextLZ4, LZ4CompReader& lz4Reader, uint32_t chunckIndex);
   virtual int32_t Decompress(ContextLZ4& contextLZ4, LZ4DecompReader& lz4DecompReader, uint32_t chunckIndex);
   virtual int32_t DumpFileStat(ContextLZ4& contextLZ4);
   virtual int32_t DumpChunkStat(ContextLZ4& contextLZ4, LZ4CompReader& lz4Reader, LZ4DecompReader& lz4DecompReader, uint32_t chunckIndex);
   virtual int32_t Close(ContextLZ4& contextLZ4, LZ4CompReader& lz4Reader, LZ4DecompReader& lz4DecompReader);

   virtual std::shared_ptr<std::ifstream> getNextFile(LZ4CompReader& lz4Reader);

   void dumpDiff(const std::shared_ptr<char>& compBuffer, const std::shared_ptr<char>& decompBuffer, uint32_t chunkSize) const;

   std::shared_ptr<ContextLZ4> getContextlz4()     { return ptrContextLZ4; }
   std::shared_ptr<LZ4CompReader> getComplz4()     { return ptrLZ4Reader; }
   std::shared_ptr<LZ4DecompReader> getDecomplz4() { return ptrLZ4DecompReader; }

private:

   std::shared_ptr<CorpusLZ4> selfReference;
   std::shared_ptr<ContextLZ4> ptrContextLZ4;
   std::shared_ptr<LZ4CompReader> ptrLZ4Reader;
   std::shared_ptr<LZ4DecompReader> ptrLZ4DecompReader;
};

