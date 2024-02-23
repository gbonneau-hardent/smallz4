// corpuslz4.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

//#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <map>
#include <list>
#include <vector>
#include <sstream>
#include <bitset>
#include <iomanip>
#include <cassert>

#include <sys/stat.h>

#include "cxxopts.h"
#include "complz4.h"
#include "decomplz4.h"


std::string testLZ4("0123456789abkzde_0123456bczefthi01234567abcdefgh0123456789ABCDEFGHIJKLMNOP");

size_t getBytesFromInTest(void* data, size_t numBytes, void* userPtr)
{
   static bool readStatus = false;

   LZ4Reader* lz4Reader = (LZ4Reader*)userPtr;

   if (readStatus) {
      lz4Reader->dataEof = true;
      return 0;
   }

   memcpy((char*)data, testLZ4.c_str(), testLZ4.size());
   memcpy(lz4Reader->fileBuffer.get() + lz4Reader->dataReadSize, data, testLZ4.size());

   readStatus = true;

   return testLZ4.size();
}

size_t compBytesFromIn(void* data, size_t numBytes, void* userPtr)
{
   if (data && numBytes > 0) {
      LZ4Reader* lz4Reader = (LZ4Reader*)userPtr;
      uint64_t remainToRead = lz4Reader->dataChunkSize - lz4Reader->dataReadSize;
      uint64_t numToTread = remainToRead >= numBytes ? numBytes : remainToRead;
      if (numToTread == 0) {
         return 0;
      }
      try {
         lz4Reader->srcFile->read((char*)data, numToTread);
         memcpy(lz4Reader->fileBuffer.get() + lz4Reader->dataReadSize, data, numToTread);
      }
      catch (std::system_error& e) {
         if (lz4Reader->srcFile->eof()) {
            std::cout << std::endl;
            lz4Reader->dataEof = true;
            return 0;
         }
         std::cerr << e.code().message() << std::endl;
         exit(-4);
      }
      lz4Reader->dataReadSize += numToTread;
      lz4Reader->totalMBytes = lz4Reader->totalSizeRead >> 20;
      if (lz4Reader->lastMBytes != lz4Reader->totalMBytes) {
         lz4Reader->lastMBytes = lz4Reader->totalMBytes;
         std::cout << " \r MBytes Read = " << lz4Reader->totalMBytes;
      }
      lz4Reader->loopCount++;
      return numToTread;
   }
   return 0;
}


void compBytesToOut(const void* data, size_t numBytes, void* userPtr)
{
   LZ4Reader* lz4Reader = (LZ4Reader*)userPtr;
   if (data && numBytes > 0)
   {
      char* compData = (char*)data;

      for (unsigned int i = 0; i < numBytes; i++) {
         if (lz4Reader->dataCompressSize == 131072) {
            std::cerr << "Compression buffer overflow" << std::endl;
            exit(-6);
         }
         lz4Reader->compBuffer.get()[lz4Reader->dataCompressSize++] = *compData++;
      }
   }
}


/// read a single byte (with simple buffering)
static unsigned char decompByteFromIn(void* userPtr) // parameter "userPtr" not needed
{
   struct LZ4DecompReader* user = (struct LZ4DecompReader*)userPtr;

   if (user->available == 0) {
      unlz4error("out of data");
   }
   user->available--;
   return *user->compBuffer++;
}

/// write a block of bytes
static void decompBytesToOut(const unsigned char* data, unsigned int numBytes, void* userPtr)
{
   struct LZ4DecompReader* user = (struct LZ4DecompReader*)userPtr;
   if (data != nullptr && numBytes > 0) {
      for (unsigned int i = 0; i < numBytes; i++) {
         if (user->decompPos >= 131072) {
            unlz4error("decomp buffer overflow");
            return;
         }
         user->decompBuffer.get()[user->decompPos++] = *data++;
      }
   }
}


constexpr uint32_t lz4MaxExpand = 16;

int32_t main(int argc, const char* argv[])
{
   std::unique_ptr<cxxopts::Options> allocated(new cxxopts::Options(argv[0], "Simulation application to compute the minimum number of trials window n needed to reach the minimum observations o having probability p\n\nExample command line arguments: -p .00025844 -o 4 -t 0.95 -c .00000002166\n"));
   auto& options = *allocated;

   bool apple = false;

   options.set_width(120)
      .set_tab_expansion()
      .add_options()
      ("c,chunk",     "Data compression chunk size",                                        cxxopts::value<std::vector<uint32_t>>()->default_value("256,512,1024,2048,4096"))
      ("w,window",    "Window matching search size",                                        cxxopts::value<uint32_t>()->default_value("65535"))
      ("d,data",      "Data compression size",                                              cxxopts::value<uint32_t>()->default_value("0"))
      ("s,set",       "Set of file that define the corpus for compression",                 cxxopts::value<std::string>()->default_value(".\\data\\silicia_corpus.txt"))
      ("b,bias",      "Bias threshold that define the uncompressibility of a chunk",        cxxopts::value<double>()->default_value("0.9"))
      ("m,match",     "Algorithm that must be used for searching the matching tokens",      cxxopts::value<uint32_t>()->default_value("0"))
      ("r,round",     "Compression size to be rounded to closest 64 bytes",                 cxxopts::value<bool>()->default_value("false"))
      ("x,max_chunk", "Maximum number of chunk that must be compressed per file in corpus", cxxopts::value<uint64_t>()->default_value("0"))
      ("o,offset",    "Create histogram of search match offset",                            cxxopts::value<bool>()->default_value("false"))
      ("l,length",    "Create histogram of search match length",                            cxxopts::value<bool>()->default_value("false"))
      ("h,histogram", "Create histogram of compression chunk size",                         cxxopts::value<bool>()->default_value("false"))
      ;

   uint32_t windowSize      = 65535;
   uint32_t dataSize        = 0;
   uint32_t matchAlgo       = 0;
   uint64_t maxChunk        = 0;
   bool     isDumpOffset    = false;
   bool     isDumpLength    = false;
   bool     isDumpComp      = true;
   bool     isRounding      = false;
   double   threshold       = 0.9;

   std::string corpusSet("");
   std::vector<uint32_t> chunkSize;

   try {
      cxxopts::ParseResult result = options.parse(argc, argv);

      chunkSize     = result["c"].as<std::vector<uint32_t>>();
      windowSize    = result["w"].as<uint32_t>();
      dataSize      = result["d"].as<uint32_t>();
      maxChunk      = result["x"].as<uint64_t>();
      corpusSet     = result["s"].as<std::string>();
      threshold     = result["b"].as<double>();
      matchAlgo     = result["m"].as<uint32_t>();
      isRounding    = result["r"].as<bool>();
      isDumpOffset  = result["o"].as<bool>();
      isDumpLength  = result["l"].as<bool>();
      isDumpComp    = result["h"].as<bool>();
   }
   catch (const cxxopts::exceptions::exception& ex)
   {
      std::cout << "error parsing options: " << ex.what() << std::endl;
      std::cout << std::endl << options.help({ "" }) << std::endl;
      return false;
   }

   // Validate arguments

   for (auto size : chunkSize) {
      std::bitset<17> bitSize(size);
      if (bitSize.count() != 1 || size > 65536) {
         std::cout << "Invalid chunk size argument: compression chunk size must be a power of 2 and less equal than 65536" << std::endl;
         exit(1);
      }
   }

   std::bitset<17> bitWindow(windowSize+1);
   if (bitWindow.count() != 1 || windowSize > 65535) {
      std::cout << "Invalid window size argument: Window matching search size must be a power of 2 less one and less equal than 65535" << std::endl;
      exit(1);
   }

   if (dataSize != 0) {
      for (auto size : chunkSize) {
         if (dataSize % size != 0) {
            std::cout << "Invalid data size argument: data size must be a multiple of chunck size" << std::endl;
            exit(1);
         }
      }
   }

   LZ4Reader lz4Reader;
   LZ4DecompReader lz4DecompReader;

   std::map<uint64_t, uint64_t>& mapDist = lz4DecompReader.mapDist;
   std::map<uint64_t, uint64_t>& mapLength = lz4DecompReader.mapLength;
   std::map<uint64_t, std::map<uint64_t, uint64_t>>& mapDistLength = lz4DecompReader.mapDistLength;

   std::vector<unsigned char> dictionary;
   bool useLegacy = false;

   static uint64_t numDist = 0;
   static uint64_t numLength = 0;

   //uint32_t dataChunk[] = { 256,512,1024,2048,4096,8192,16384,32768,65536 };

   std::list<std::shared_ptr<std::ifstream>> corpusList;
   std::shared_ptr<unsigned char> fileBuffer = std::shared_ptr<unsigned char>(new unsigned char[65536]);
   std::map <std::string, uint64_t> ratioStat;
   std::map <std::ifstream*, std::string> corpusFileName;

   std::string srcPathFileName;
   std::ifstream listFile;

   listFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

   try {
      listFile.open(corpusSet.c_str());
   }
   catch (std::system_error& e) {
      std::cerr << e.code().message() << std::endl;
      exit(-1);
   }

   while (true) {
      srcPathFileName.clear();
      try {
         std::getline(listFile, srcPathFileName);
      }
      catch (std::system_error& e) {
         if (listFile.eof()) {
            break;
         }
         std::cerr << e.code().message() << std::endl;
         exit(-2);
      }
      std::shared_ptr<std::ifstream> srcFile = std::make_shared<std::ifstream>();
      srcFile->exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
      corpusList.push_back(srcFile);

      try {
         srcFile->open(srcPathFileName.c_str(), std::ios::in | std::ios::binary);
      }
      catch (std::system_error& e) {
         std::cerr << e.code().message() << std::endl;
         exit(-3);
      }
      corpusFileName[srcFile.get()] = srcPathFileName;
   }
   listFile.close();

   std::string srcStatName = corpusSet.substr(corpusSet.find_last_of("/\\") + 1);

   std::vector<std::shared_ptr<std::map<uint32_t, uint32_t>>> chunkAllCompStat(chunkSize.size(), nullptr);

   for (uint32_t chunckIndex = 0; chunckIndex < chunkSize.size(); chunckIndex++)
   {
      std::shared_ptr<std::map<uint32_t, uint32_t>> compStatistic = std::shared_ptr<std::map<uint32_t, uint32_t>>(new std::map<uint32_t, uint32_t>);
      chunkAllCompStat[chunckIndex] = std::make_shared<std::map<uint32_t, uint32_t>>();

      std::string statFileName = std::string("lz4_") + srcStatName + "_" + std::to_string(chunkSize[chunckIndex]) + ".csv";
      std::string statFileDistName = std::string("lz4_dist_") + srcStatName + "_" + std::to_string(chunkSize[chunckIndex]) + ".csv";
      std::string statFileLengthName = std::string("lz4_length_") + srcStatName + "_" + std::to_string(chunkSize[chunckIndex]) + ".csv";
      std::string statFileDistLengthName = std::string("lz4_dist_length_") + srcStatName + "_" + std::to_string(chunkSize[chunckIndex]) + ".csv";
      std::string statFileNameNewLZ4 = std::string("lz4_new_") + srcStatName + "_" + std::to_string(chunkSize[chunckIndex]) + ".csv";
      std::string dumpFileNameNewLZ4 = std::string("lz4_new_") + srcStatName + "_" + std::to_string(chunkSize[chunckIndex]) + ".txt";

      std::ofstream statFile;
      std::ofstream statFileDist;
      std::ofstream statFileLength;
      std::ofstream statFileDistLength;
      std::ofstream statFileNewL4;
      std::ofstream dumpFileNewL4;

      statFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      statFileDist.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      statFileLength.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      statFileNewL4.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      dumpFileNewL4.exceptions(std::ofstream::failbit | std::ofstream::badbit);

      try {
         statFile.open(statFileName.c_str(), std::ios::out | std::ios::trunc);
         statFileDist.open(statFileDistName.c_str(), std::ios::out | std::ios::trunc);
         statFileLength.open(statFileLengthName.c_str(), std::ios::out | std::ios::trunc);
         statFileDistLength.open(statFileDistLengthName.c_str(), std::ios::out | std::ios::trunc);
         statFileNewL4.open(statFileNameNewLZ4.c_str(), std::ios::out | std::ios::trunc);
         dumpFileNewL4.open(dumpFileNameNewLZ4.c_str(), std::ios::out | std::ios::trunc);
      }
      catch (std::system_error& e) {
         std::cerr << "Failed to open File " << statFileName << std::endl;
         std::cerr << e.code().message() << std::endl;
         exit(-4);
      }

      statFile << "# Source File Name = " << srcStatName << std::endl << "#" << std::endl;
      statFile << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << "#" << std::endl;
      statFileDist << "# Source File Name = " << srcStatName << std::endl << "#" << std::endl;
      statFileDist << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << "#" << std::endl;
      statFileLength << "# Source File Name = " << srcStatName << std::endl << "#" << std::endl;
      statFileLength << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << "#" << std::endl;
      statFileDistLength << "# Source File Name = " << srcStatName << std::endl << "#" << std::endl;
      statFileDistLength << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << "#" << std::endl;
      statFileNewL4 << "# Source File Name = " << srcStatName << std::endl << "#" << std::endl;
      statFileNewL4 << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << "#" << std::endl;
      dumpFileNewL4 << "# Source File Name = " << srcStatName << std::endl << "#" << std::endl;
      dumpFileNewL4 << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << std::endl;

      std::cout << isRounding << std::endl;

      uint32_t maxCompSize = chunkSize[chunckIndex] + lz4MaxExpand;
      maxCompSize = isRounding ? (((maxCompSize + 63) >> 6) << 6) : maxCompSize;

      for (uint32_t i = 0; i <= maxCompSize; i++) {
         (*compStatistic)[i] = 0;
         (*chunkAllCompStat[chunckIndex])[i] = 0;

      }

      uint64_t compThreshold = (double)chunkSize[chunckIndex] / threshold;

      lz4Reader.totalChunkCount = 0;
      lz4Reader.totalChunkCompress = 0;
      lz4Reader.totalSizeRead = 0;
      lz4Reader.totalSizeReadStat = 0;
      lz4Reader.totalSizeCompress = 0;
      lz4Reader.totalSizeCompressStat = 0;
      lz4Reader.dataCompressSize = 0;
      lz4Reader.dataReadSize = 0;
      lz4Reader.dataCompressSize = 0;
      lz4Reader.totalMBytes = 0;
      lz4Reader.lastMBytes = 0;
      lz4Reader.loopCount = 0;
      lz4Reader.dataEof = false;
      lz4Reader.dataChunkSize = chunkSize[chunckIndex];

      mapDist.clear();
      mapLength.clear();
      mapDistLength.clear();

      for (auto iterFile = corpusList.begin(); iterFile != corpusList.end(); ++iterFile) {

         lz4Reader.srcFile = *iterFile;
         lz4Reader.srcFile->clear();
         lz4Reader.srcFile->seekg(0);

         lz4Reader.dataEof = false;
         lz4Reader.dataCompressSize = 0;
         lz4Reader.dataReadSize = 0;

         std::string fileName = corpusFileName[(*iterFile).get()];
         bool isDebugFile = fileName.find("mozilla") != std::string::npos;
         if (!isDebugFile) {
            //continue;
         }

         ratioStat[fileName] = 0;
         statFileNewL4 << "# File Compressed : " << fileName << std::endl;
         statFileNewL4 << "# Statistic of compression loss with modified algorithm " << fileName << std::endl << "#" << std::endl;

         dumpFileNewL4 << "# File Compressed : " << fileName << std::endl;
         dumpFileNewL4 << "# Statistic of compression loss with modified algorithm " << fileName << std::endl << "#" << std::endl;

         std::shared_ptr<std::map<uint32_t, uint32_t>> compLossStatistic = std::shared_ptr<std::map<uint32_t, uint32_t>>(new std::map<uint32_t, uint32_t>);
         std::shared_ptr<std::list<std::pair<uint32_t, uint32_t>>> compLossCloud = std::shared_ptr<std::list<std::pair<uint32_t, uint32_t>>>(new std::list<std::pair<uint32_t, uint32_t>>);

         uint64_t numLoop = 0;

         while (true) {
            if (maxChunk != 0) {
               if (numLoop++ > maxChunk) {
                  break;
               }
            }
            smallz4::lz4(compBytesFromIn, compBytesToOut, windowSize, dictionary, useLegacy, &lz4Reader);

            if (lz4Reader.dataEof) {
               break;
            }

            if (lz4Reader.dataReadSize != chunkSize[chunckIndex]) {
               std::cout << "Exiting with error lz4Reader.dataReadSize != dataChunk[chunckIndex]" << std::endl;
               exit(-5);
            }

            double ratio = 0.0;
            double ratioNewLZ4 = 0.0;
            double ratioLoss = 0.0;
            uint64_t intRatio = 0;
            uint64_t intCompressSize = 0;

            uint64_t roundCompressSize = lz4Reader.dataCompressSize;
            roundCompressSize = isRounding ? ((roundCompressSize + 63) >> 6) << 6 : roundCompressSize;
            roundCompressSize = roundCompressSize <= compThreshold ? roundCompressSize : maxCompSize;

            if (roundCompressSize <= compThreshold) {

               lz4Reader.totalChunkCompress++;
               lz4Reader.totalSizeReadStat += lz4Reader.dataReadSize;
               lz4Reader.totalSizeCompressStat += roundCompressSize;

               ratio = (((double)chunkSize[chunckIndex] / (double)roundCompressSize)) * 100.0;
               intRatio = ratio;
               intRatio = intRatio / 5;
               intRatio = intRatio * 5;
               double compChunckSize = (double)chunkSize[chunckIndex] / ((double)intRatio/100.0);
               intCompressSize = compChunckSize;

               // We want to know which files from the corpus set yield the greatest ratio in some range.
               if ((ratio/100.0 > 3.75) && (ratio/100.0 < 4.75)) {
                  ratioStat[fileName]++;
               }
               (*compStatistic)[intCompressSize]++;
            }
            (*chunkAllCompStat[chunckIndex])[roundCompressSize]++;
            lz4Reader.totalChunkCount++;
            lz4Reader.totalSizeRead += lz4Reader.dataReadSize;
            lz4Reader.totalSizeCompress += roundCompressSize;

            lz4DecompReader.available = lz4Reader.dataCompressSize;
            lz4DecompReader.compBuffer = lz4Reader.compBuffer.get();
            lz4DecompReader.decompPos = 0;

            std::list<lz4Token> listSequenceLess64Illegal;

            unlz4_userPtr(decompByteFromIn, decompBytesToOut, nullptr, &lz4DecompReader);

            int retCmp = std::strncmp(lz4Reader.fileBuffer.get(), lz4DecompReader.decompBuffer.get(), chunkSize[chunckIndex]);
            if (retCmp != 0) {
               assert(false);
               exit(-6);
            }

            if (roundCompressSize <= compThreshold) {

               ratioLoss = 100.0 * (ratio - ratioNewLZ4) / ratio;
               int64_t intNewLZ4Ratio = ratioLoss * 10.0;
               ratioLoss = double(intNewLZ4Ratio) / 10.0;

               (*compLossCloud).push_back({uint32_t(ratio), uint32_t(ratioNewLZ4)});
               (*compLossStatistic)[intNewLZ4Ratio]++;
            }
            lz4Reader.dataCompressSize = 0;
            lz4Reader.dataReadSize = 0;  
         }
         statFileNewL4 << "# Number of entried for this statistic = " << (*compLossCloud).size() << std::endl << "#" << std::endl;
         for (auto percLoss : (*compLossCloud)) {
            statFileNewL4 << "LZ4 ratio x 100," << percLoss.first << ", New LZ4 ratio x 100," << percLoss.second << std::endl;
         }
         statFileNewL4 << std::endl;
      }
      numDist = 0;

      for (auto iterDist = mapDist.begin(); iterDist != mapDist.end(); iterDist++) {
         numDist += iterDist->second;
      }
      uint32_t distIndex = 0;
      uint64_t distChunk = 0;
      uint64_t totDistChunk = 0;

      statFileDist << "# Distance in byte" << "," << "Number of matches" << "," << "Total % of matches" << std::endl << "#" << std::endl;

      for (auto iterDist = mapDist.begin(); iterDist != mapDist.end(); ) {

         distIndex++;

         if (iterDist->first != distIndex) {
            if ((distIndex % 32) == 0) {
               statFileDist << distIndex << "," << distChunk << "," << (double(totDistChunk) * 100.0) / double(numDist) << std::endl;
               distChunk = 0;
            }
            continue;
         }
         distChunk += iterDist->second;
         totDistChunk += iterDist->second;

         if ((distIndex % 32) == 0) {
            statFileDist << distIndex << "," << distChunk << "," << (double(totDistChunk) * 100.0) / double(numDist) << std::endl;
            distChunk = 0;
         }
         iterDist++;
      }
      if (distIndex != 4096) {
         statFileDist << ((distIndex + 31) >> 5 << 5) << "," << distChunk << "," << (double(totDistChunk) * 100.0) / double(numDist) << std::endl;
      }

      uint64_t numLength = 0;
      uint32_t lengthIndex = 0;
      uint64_t lengthChunk = 0;
      uint64_t totLengthChunk = 0;

      for (auto iterLength = mapLength.begin(); iterLength != mapLength.end(); iterLength++) {
         numLength += iterLength->second;
      }

      statFileLength << "# Length in byte" << "," << "Number of length" << "," << "Total % of Length" << std::endl << "#" << std::endl;

      for (auto iterLength = mapLength.begin(); iterLength != mapLength.end(); ) {

         lengthIndex++;

         if (iterLength->first != lengthIndex) {
            if ((lengthIndex % 1) == 0) {
               statFileLength << lengthIndex << "," << lengthChunk << "," << (double(totLengthChunk) * 100.0) / double(numLength) << std::endl;
               lengthChunk = 0;
            }
            continue;
         }
         lengthChunk += iterLength->second;
         totLengthChunk += iterLength->second;

         if ((lengthIndex % 1) == 0) {
            statFileLength << lengthIndex << "," << lengthChunk << "," << (double(totLengthChunk) * 100.0) / double(numLength) << std::endl;
            lengthChunk = 0;
         }
         iterLength++;
      }
      if (distIndex != 4096) {
         statFileLength << ((lengthIndex + 31) >> 5 << 5) << "," << lengthChunk << "," << (double(totLengthChunk) * 100.0) / double(numLength) << std::endl;
      }

      distIndex = 0;
      std::map<uint64_t, double> mapAvg;

      for (auto iterDist = mapDistLength.begin(); iterDist != mapDistLength.end(); iterDist++) {

         auto& offMapLength = iterDist->second;

         numLength = 0;
         lengthIndex = 0;
         lengthChunk = 0;
         totLengthChunk = 0;

         double avgLength = 0.0;

         for (auto iterLength = offMapLength.begin(); iterLength != offMapLength.end(); iterLength++) {

            lengthChunk += iterLength->second;
            totLengthChunk += iterLength->second * iterLength->first;
         }
         avgLength = double(totLengthChunk) / double(lengthChunk);
         lengthChunk = 0;
         mapAvg[iterDist->first] = avgLength;
      }

      auto iterMean = mapAvg.begin();

      for (auto iterDist = mapDistLength.begin(); iterDist != mapDistLength.end(); ) {

         distIndex++;

         if (iterDist->first != distIndex) {
            continue;
         }
         auto & offMapLength = iterDist->second;

         numLength = 0;
         lengthIndex = 0;
         lengthChunk = 0;
         totLengthChunk = 0;

         uint64_t minLength = 0xffffffffffffffff;
         uint64_t maxLength = 0x0;
         uint64_t sumItem = 0;

         double   avgLength = double(iterMean->second);
         double   stdDeviation = 0.0;

         for (auto iterLength = offMapLength.begin(); iterLength != offMapLength.end(); ) {

            lengthIndex++;

            if (iterLength->first != lengthIndex) {
               continue;
            }
            minLength = std::min(minLength, iterLength->first);
            maxLength = std::max(maxLength, iterLength->first);
            sumItem += iterLength->second;
            stdDeviation += iterLength->second*pow(iterLength->first - avgLength, 2);

            iterLength++;
         }
         stdDeviation = sqrt(stdDeviation / double(sumItem));

         statFileDistLength << distIndex << "," << minLength << "," << maxLength << "," << iterMean->second << "," << stdDeviation << std::endl;
         lengthChunk = 0;

         iterDist++;
         iterMean++;
      }
      uint64_t maxChunkCompSize = chunkSize[chunkSize.size() - 1] + lz4MaxExpand;
      maxChunkCompSize = isRounding ? ((maxChunkCompSize + 63) >> 6) << 6 : maxChunkCompSize;

      for (uint32_t i = 1; i <= maxChunkCompSize; i++) {

         std::stringstream ss;
         ss << i;
         for (int32_t chunckIndex = chunkSize.size()-1; 0 <= chunckIndex; --chunckIndex) {

            uint64_t maxCompSize = chunkSize[chunckIndex] + lz4MaxExpand;
            maxCompSize = isRounding ? ((maxCompSize + 63) >> 6) << 6 : maxCompSize;
            if (maxCompSize >= i) {
               if (chunkAllCompStat[chunckIndex] != nullptr) {
                  ss << "," << (*chunkAllCompStat[chunckIndex])[i];
               }
            }
         }
         statFile << std::fixed << std::setprecision(2) << ss.str() << std::endl;
      }
      std::cout << std::fixed << std::setprecision(1) << std::endl;
      std::cout << "Total chunks (size = " << chunkSize[chunckIndex] << ") processed = " << lz4Reader.totalChunkCount << ", chunks compressed = " << lz4Reader.totalChunkCompress << " (" << (lz4Reader.totalChunkCompress * 100.0) / lz4Reader.totalChunkCount << ")" << std::endl;
      std::cout << std::fixed << std::setprecision(2);

      std::cout << "Compression ratio with threshold (" << threshold << ") = " << (double)lz4Reader.totalSizeReadStat / (double)lz4Reader.totalSizeCompressStat << std::endl;
      std::cout << "Compression ratio global (all chunk) = " << (double)lz4Reader.totalSizeRead / (double)lz4Reader.totalSizeCompress << std::endl << std::endl;

      statFile << std::fixed << std::setprecision(1) << "#" << std::endl;
      statFile << "# Total chunks (size = " << chunkSize[chunckIndex] << ") processed = " << lz4Reader.totalChunkCount << " chunks compressed = " << lz4Reader.totalChunkCompress << " (" << (lz4Reader.totalChunkCompress * 100.0) / lz4Reader.totalChunkCount << ")" << std::endl;
      statFile << std::fixed << std::setprecision(2);

      statFile << "#" << std::endl << "# Compression ratio with threshold (" << threshold << ") = " << (double)lz4Reader.totalSizeReadStat / (double)lz4Reader.totalSizeCompressStat << std::endl;
      statFile << "# Compression ratio global (all chunk) = " << (double)lz4Reader.totalSizeRead / (double)lz4Reader.totalSizeCompress << std::endl << "#";

      statFile.close();
      statFileDist.close();
      statFileLength.close();
      statFileDistLength.close();
      statFileNewL4.close();
      dumpFileNewL4.close();
   }


   //for (uint32_t i = 1; i < maxCompSize; i++) {
   //   //if ((*allCompStatistic)[i] != 0) {
   //   std::cout << "compression size = " << i << ", static count = " << (*allCompStatistic)[i] << std::endl;
   //   statFile << std::fixed << std::setprecision(2) << i << "," << ((double)dataChunk[chunckIndex] / (double)i) << "," << (*allCompStatistic)[i] << std::endl;
   //   //}
   //}


   for (auto iterFile = corpusList.begin(); iterFile != corpusList.end(); ++iterFile) {
      std::shared_ptr<std::ifstream> srcFile = *iterFile;
      srcFile->close();
   }
}
