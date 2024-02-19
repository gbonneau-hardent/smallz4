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


// //////////////////////////////////////////////////////////
// smallz4cat.c
// Copyright (c) 2016-2019 Stephan Brumme. All rights reserved.
// see https://create.stephan-brumme.com/smallz4/
//
// "MIT License":
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// This program is a shorter, more readable, albeit slower re-implementation of lz4cat ( https://github.com/Cyan4973/xxHash )

// compile: gcc smallz4cat.c -O3 -o smallz4cat -Wall -pedantic -std=c99 -s
// The static 8k binary was compiled using Clang and dietlibc (see https://www.fefe.de/dietlibc/ )

// Limitations:
// - skippable frames and legacy frames are not implemented (and most likely never will)
// - checksums are not verified (see https://create.stephan-brumme.com/xxhash/ for a simple implementation)

// Replace getByteFromIn() and sendToOut() by your own code if you need in-memory LZ4 decompression.
// Corrupted data causes a call to unlz4error().

// suppress warnings when compiled by Visual C++
//#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>  // stdin/stdout/stderr, fopen, ...
#include <stdlib.h> // exit()
#include <string.h> // memcpy

#ifndef FALSE
#define FALSE 0
#define TRUE  1
#endif

/// error handler
static void unlz4error(const char* msg)
{
   // smaller static binary than fprintf(stderr, "ERROR: %s\n", msg);
   fputs("ERROR: ", stderr);
   fputs(msg, stderr);
   fputc('\n', stderr);
   exit(1);
}


// ==================== I/O INTERFACE ====================


// read one byte from input, see getByteFromIn()  for a basic implementation
typedef unsigned char (*GET_BYTE)  (void* userPtr);
// write several bytes,      see sendBytesToOut() for a basic implementation
typedef void          (*SEND_BYTES)(const unsigned char*, unsigned int, void* userPtr);


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
   char * compBuffer;
   unsigned int  available;
   unsigned int  decompPos;

   std::map<uint64_t, uint64_t> chunkStat;
   std::list<lz4Token> listSequence;
   std::map<uint64_t, uint64_t> mapDist;
   std::map<uint64_t, uint64_t> mapLength;
   std::map<uint64_t, std::map<uint64_t, uint64_t>> mapDistLength;
};

/// read a single byte (with simple buffering)
static unsigned char getByteFromIn(void* userPtr) // parameter "userPtr" not needed
{
   struct LZ4DecompReader* user = (struct LZ4DecompReader*)userPtr;

   if (user->available == 0) {
         unlz4error("out of data");
   }
   user->available--;
   return *user->compBuffer++;
}

/// write a block of bytes
static void sendBytesToOut(const unsigned char* data, unsigned int numBytes, void* userPtr)
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


// ==================== LZ4 DECOMPRESSOR ====================




/// decompress everything in input stream (accessed via getByte) and write to output stream (via sendBytes)
void unlz4_userPtr(GET_BYTE getByte, SEND_BYTES sendBytes, const char* dictionary, void* userPtr )
{
   // signature
   // 
   LZ4DecompReader * lz4Reader = reinterpret_cast<LZ4DecompReader *>(userPtr);

   std::map<uint64_t, uint64_t> & chunkStat                         = lz4Reader->chunkStat;
   std::list<lz4Token> & listSequence                               = lz4Reader->listSequence;
   std::map<uint64_t, uint64_t> & mapDist                           = lz4Reader->mapDist;
   std::map<uint64_t, uint64_t> & mapLength                         = lz4Reader->mapLength;
   std::map<uint64_t, std::map<uint64_t, uint64_t>> & mapDistLength = lz4Reader->mapDistLength;

   unsigned char signature1 = getByte(userPtr);
   unsigned char signature2 = getByte(userPtr);
   unsigned char signature3 = getByte(userPtr);
   unsigned char signature4 = getByte(userPtr);
   unsigned int  signature = (signature4 << 24) | (signature3 << 16) | (signature2 << 8) | signature1;
   unsigned char isModern = (signature == 0x184D2204);
   unsigned char isLegacy = (signature == 0x184C2102);
   if (!isModern && !isLegacy)
      unlz4error("invalid signature");

   unsigned char hasBlockChecksum = FALSE;
   unsigned char hasContentSize = FALSE;
   unsigned char hasContentChecksum = FALSE;
   unsigned char hasDictionaryID = FALSE;
   if (isModern)
   {
      // flags
      unsigned char flags = getByte(userPtr);
      hasBlockChecksum = flags & 16;
      hasContentSize = flags & 8;
      hasContentChecksum = flags & 4;
      hasDictionaryID = flags & 1;

      // only version 1 file format
      unsigned char version = flags >> 6;
      if (version != 1)
         unlz4error("only LZ4 file format version 1 supported");

      // ignore blocksize
      char numIgnore = 1;

      // ignore, skip 8 bytes
      if (hasContentSize)
         numIgnore += 8;
      // ignore, skip 4 bytes
      if (hasDictionaryID)
         numIgnore += 4;

      // ignore header checksum (xxhash32 of everything up this point & 0xFF)
      numIgnore++;

      // skip all those ignored bytes
      while (numIgnore--)
         getByte(userPtr);
   }

   // don't lower this value, backreferences can be 64kb far away
#define HISTORY_SIZE 64*1024
  // contains the latest decoded data
   unsigned char history[HISTORY_SIZE];
   // next free position in history[]
   unsigned int  pos = 0;

   // dictionary compression is a recently introduced feature, just move its contents to the buffer
   if (dictionary != NULL)
   {
      // open dictionary
      FILE* dict;
      fopen_s(&dict, dictionary, "rb");
      if (!dict)
         unlz4error("cannot open dictionary");

      // get dictionary's filesize
      fseek(dict, 0, SEEK_END);
      long dictSize = ftell(dict);
      // only the last 64k are relevant
      long relevant = dictSize < 65536 ? 0 : dictSize - 65536;
      fseek(dict, relevant, SEEK_SET);
      if (dictSize > 65536)
         dictSize = 65536;
      // read it and store it at the end of the buffer
      fread(history + HISTORY_SIZE - dictSize, 1, dictSize, dict);
      fclose(dict);
   }

   // parse all blocks until blockSize == 0
   while (1)
   {
      // block size
      unsigned int blockSize = getByte(userPtr);
      blockSize |= (unsigned int)getByte(userPtr) << 8;
      blockSize |= (unsigned int)getByte(userPtr) << 16;
      blockSize |= (unsigned int)getByte(userPtr) << 24;

      // highest bit set ?
      unsigned char isCompressed = isLegacy || (blockSize & 0x80000000) == 0;
      if (isModern)
         blockSize &= 0x7FFFFFFF;

      // stop after last block
      if (blockSize == 0)
         break;

      if (isCompressed)
      {

         // decompress block
         unsigned int blockOffset = 0;
         unsigned int numWritten = 0;
         unsigned int boundary64 = 0;
         unsigned int lastBoundary64 = 0;
         unsigned int numSequence64 = 0;
         
         while (blockOffset < blockSize)
         {
            listSequence.emplace_back();
            lz4Token& tokenSequence = listSequence.back();

            // get a token
            if ((boundary64 - lastBoundary64) >= 64) {                              
               if (numSequence64 == 1) {                // A sequence might be greater than 64 if all literals with no matches. In which case it count as 1.
                  chunkStat[numSequence64]++;
                  numSequence64 = 0;
               }
               else {
                  chunkStat[numSequence64 - 1]++;
                  numSequence64 = 1;
                };
               lastBoundary64 = boundary64;
            }
            if (numSequence64 >= 64) {
               std::cout << "";
            }
            numSequence64++;
            unsigned char token = getByte(userPtr);
            blockOffset++;
            boundary64++;

            tokenSequence.token = token;

            // determine number of literals
            unsigned int numLiterals = token >> 4;
            if (numLiterals == 15)
            {
               // number of literals length encoded in more than 1 byte
               unsigned char current;
               do
               {
                  current = getByte(userPtr);
                  numLiterals += current;
                  blockOffset++;
                  boundary64++;
               } while (current == 255);
            }
            tokenSequence.literalLength = numLiterals;

            blockOffset += numLiterals;
            boundary64 += numLiterals;

            // copy all those literals
            if (pos + numLiterals < HISTORY_SIZE)
            {
               // fast loop
               while (numLiterals-- > 0) {
                  unsigned char oneLiteral = getByte(userPtr);
                  history[pos++] = oneLiteral;
                  tokenSequence.literal.push_back(oneLiteral);
               }
            }
            else
            {
               // slow loop
               while (numLiterals-- > 0)
               {
                  history[pos++] = getByte(userPtr);

                  // flush output buffer
                  if (pos == HISTORY_SIZE)
                  {
                     assert(false);
                     sendBytes(history, HISTORY_SIZE, userPtr);
                     numWritten += HISTORY_SIZE;
                     pos = 0;
                  }
               }
            }

            // last token has only literals
            if (blockOffset == blockSize)
               break;

            // match distance is encoded in two bytes (little endian)
            unsigned int delta = getByte(userPtr);
            delta |= (unsigned int)getByte(userPtr) << 8;
            // zero isn't allowed
            if (delta == 0)
               unlz4error("invalid offset");
            blockOffset += 2;
            boundary64 += 2;

            // match length (always >= 4, therefore length is stored minus 4)
            unsigned int matchLength = 4 + (token & 0x0F);

            if (matchLength == 4 + 0x0F)
            {
               unsigned char current;
               do // match length encoded in more than 1 byte
               {
                  current = getByte(userPtr);
                  matchLength += current;
                  blockOffset++;
                  boundary64++;
               } while (current == 255);
            }
            tokenSequence.offset = delta;
            tokenSequence.matchLength = matchLength;

            mapDist[delta]++;
            mapLength[matchLength]++;
            mapDistLength[delta][matchLength]++;

            // copy match
            unsigned int referencePos = (pos >= delta) ? (pos - delta) : (HISTORY_SIZE + pos - delta);
            // start and end within the current 64k block ?
            if (pos + matchLength < HISTORY_SIZE && referencePos + matchLength < HISTORY_SIZE)
            {
               // read/write continuous block (no wrap-around at the end of history[])
               // fast copy
               if (pos >= referencePos + matchLength || referencePos >= pos + matchLength)
               {
                  // non-overlapping
                  memcpy(history + pos, history + referencePos, matchLength);
                  pos += matchLength;
               }
               else
               {
                  // overlapping, slower byte-wise copy
                  while (matchLength-- > 0)
                     history[pos++] = history[referencePos++];
               }
            }
            else
            {
               // either read or write wraps around at the end of history[]
               while (matchLength-- > 0)
               {
                  // copy single byte
                  history[pos++] = history[referencePos++];

                  // cannot write anymore ? => wrap around
                  if (pos == HISTORY_SIZE)
                  {
                     // flush output buffer
                     assert(false);
                     sendBytes(history, HISTORY_SIZE, userPtr);
                     numWritten += HISTORY_SIZE;
                     pos = 0;
                  }
                  // wrap-around of read location
                  referencePos %= HISTORY_SIZE;
               }
            }
         }

         // all legacy blocks must be completely filled - except for the last one
         if (isLegacy && numWritten + pos < 8 * 1024 * 1024)
            break;
      }
      else
      {
         // copy uncompressed data and add to history, too (if next block is compressed and some matches refer to this block)
         while (blockSize-- > 0)
         {
            // copy a byte ...
            history[pos++] = getByte(userPtr);
            // ... until buffer is full => send to output
            if (pos == HISTORY_SIZE)
            {
               assert(false);
               sendBytes(history, HISTORY_SIZE, userPtr);
               pos = 0;
            }
         }
      }

      if (hasBlockChecksum)
      {
         // ignore checksum, skip 4 bytes
         getByte(userPtr); getByte(userPtr); getByte(userPtr); getByte(userPtr);
      }
   }

   if (hasContentChecksum)
   {
      // ignore checksum, skip 4 bytes
      getByte(userPtr); getByte(userPtr); getByte(userPtr); getByte(userPtr);
   }

   // flush output buffer
   sendBytes(history, pos, userPtr);
}

/// old interface where getByte and sendBytes use global file handles
void unlz4(GET_BYTE getByte, SEND_BYTES sendBytes, const char* dictionary)
{
   //unlz4_userPtr(getByte, sendBytes, dictionary, NULL, );
}


// ==================== COMMAND-LINE HANDLING ====================

#ifndef boolean
#define boolean bool
#endif

#include "smallz4.h"

struct LZ4Reader
{
   LZ4Reader()
   {
      std::memset(fileBuffer.get(), 0, 131072);
      std::memset(compBuffer.get(), 0, 131072);
   }

   std::shared_ptr<char> fileBuffer = std::shared_ptr<char>(new char [131072]);
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

size_t getBytesFromIn(void* data, size_t numBytes, void* userPtr)
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
      lz4Reader->totalSizeRead += numToTread;
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


void sendBytesToOut(const void* data, size_t numBytes, void* userPtr)
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
         lz4Reader->totalSizeCompress++;
         lz4Reader->compBuffer.get()[lz4Reader->dataCompressSize++] = *compData++;
      }
   }
}


int main(int argc, const char* argv[])
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
      ("b,bias",      "Bias threshold that define the uncompressibility of a chunk",        cxxopts::value<double>()->default_value("1.25"))
      ("m,match",     "Algorithm that must be used for searching the matching tokens",      cxxopts::value<uint32_t>()->default_value("0"))
      ("x,max_chunk", "Maximum number of chunk that must be compressed per file in corpus", cxxopts::value<uint64_t>()->default_value("0"))
      ("o,offset",    "Create histogram of search match offset",                            cxxopts::value<bool>()->default_value("false"))
      ("l,length",    "Create histogram of search match length",                            cxxopts::value<bool>()->default_value("false"))
      ("h,histogram", "Create histogram of compression chunk size",                         cxxopts::value<bool>()->default_value("true"))
      ;

   uint32_t windowSize      = 65535;
   uint32_t dataSize        = 0;
   uint32_t matchAlgo       = 0;
   uint64_t maxChunk        = 0;
   bool     isDumpOffset    = false;
   bool     isDumpLength    = false;
   bool     isDumpComp      = true;
   double   threshold       = 1.25;

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

   //std::string listFileName("C:\\Users\\gbonneau\\git\\zlib\\data\\calgary_corpus.txt");
   //std::string listFileName("C:\\Users\\gbonneau\\git\\zlib\\data\\enwik9.txt");
   //std::string listFileName("C:\\Users\\gbonneau\\git\\zlib\\data\\silicia_corpus.txt");
   //std::string listFileName("C:\\Users\\gbonneau\\git\\zlib\\data\\video_media.txt");
   //std::string listFileName("C:\\Users\\gbonneau\\git\\zlib\\data\\protein_corpus.txt");
   //std::string listFileName("C:\\Users\\gbonneau\\git\\zlib\\data\\random.txt");
   //std::string listFileName("C:\\Users\\gbonneau\\git\\zlib\\data\\lukas_2d_8.txt");
   //std::string listFileName("C:\\Users\\gbonneau\\git\\zlib\\data\\lukas_2d_16.txt");
   //std::string listFileName("C:\\Users\\gbonneau\\git\\zlib\\data\\lukas_3d.txt");
   //std::string listFileName("C:\\Users\\gbonneau\\git\\zlib\\data\\canterbury_corpus.txt");

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

      statFile << " # Source File Name = " << srcStatName << std::endl << std::endl;
      statFile << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << std::endl;
      statFileDist << "# Source File Name = " << srcStatName << std::endl << std::endl;
      statFileDist << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << std::endl;
      statFileLength << "# Source File Name = " << srcStatName << std::endl << std::endl;
      statFileLength << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << std::endl;
      statFileDistLength << "# Source File Name = " << srcStatName << std::endl << std::endl;
      statFileDistLength << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << std::endl;
      statFileNewL4 << "# Source File Name = " << srcStatName << std::endl << std::endl;
      statFileNewL4 << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << std::endl;
      dumpFileNewL4 << "# Source File Name = " << srcStatName << std::endl << std::endl;
      dumpFileNewL4 << "# Memory Chunck Size = " << chunkSize[chunckIndex] << std::endl << std::endl;

      for (uint32_t i = 0; i < (chunkSize[chunckIndex] + 64); i++) {
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
         statFileNewL4 << "# Statistic of compression loss with modified algorithm " << fileName << std::endl << std::endl;

         dumpFileNewL4 << "# File Compressed : " << fileName << std::endl;
         dumpFileNewL4 << "# Statistic of compression loss with modified algorithm " << fileName << std::endl << std::endl;

         std::shared_ptr<std::map<uint32_t, uint32_t>> compLossStatistic = std::shared_ptr<std::map<uint32_t, uint32_t>>(new std::map<uint32_t, uint32_t>);
         std::shared_ptr<std::list<std::pair<uint32_t, uint32_t>>> compLossCloud = std::shared_ptr<std::list<std::pair<uint32_t, uint32_t>>>(new std::list<std::pair<uint32_t, uint32_t>>);

         uint64_t numLoop = 0;

         while (true) {
            if (maxChunk != 0) {
               if (numLoop++ > maxChunk) {
                  break;
               }
            }
            smallz4::lz4(getBytesFromIn, sendBytesToOut, windowSize, dictionary, useLegacy, &lz4Reader);

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

            if (lz4Reader.dataCompressSize <= compThreshold) {

               lz4Reader.totalChunkCompress++;
               lz4Reader.totalSizeReadStat += lz4Reader.dataReadSize;
               lz4Reader.totalSizeCompressStat += lz4Reader.dataCompressSize;

               ratio = (((double)chunkSize[chunckIndex] / (double)lz4Reader.dataCompressSize)) * 100.0;
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
            (*chunkAllCompStat[chunckIndex])[lz4Reader.dataCompressSize]++;
            lz4Reader.totalChunkCount++;
            lz4Reader.totalSizeRead += lz4Reader.dataReadSize;
            lz4Reader.totalSizeCompress += lz4Reader.dataCompressSize;

            lz4DecompReader.available = lz4Reader.dataCompressSize;
            lz4DecompReader.compBuffer = lz4Reader.compBuffer.get();
            lz4DecompReader.decompPos = 0;

            std::list<lz4Token> listSequenceLess64Illegal;

            unlz4_userPtr(getByteFromIn, sendBytesToOut, nullptr, &lz4DecompReader);

            int retCmp = std::strncmp(lz4Reader.fileBuffer.get(), lz4DecompReader.decompBuffer.get(), chunkSize[chunckIndex]);
            if (retCmp != 0) {
               assert(false);
               exit(-6);
            }
            uint32_t dataCompressSize = lz4Reader.dataCompressSize;

            if (dataCompressSize <= compThreshold) {

               ratioLoss = 100.0 * (ratio - ratioNewLZ4) / ratio;
               int64_t intNewLZ4Ratio = ratioLoss * 10.0;
               ratioLoss = double(intNewLZ4Ratio) / 10.0;

               (*compLossCloud).push_back({uint32_t(ratio), uint32_t(ratioNewLZ4)});
               (*compLossStatistic)[intNewLZ4Ratio]++;
            }
            lz4Reader.dataCompressSize = 0;
            lz4Reader.dataReadSize = 0;  
         }
         statFileNewL4 << "# Number of entried for this statistic = " << (*compLossCloud).size() << std::endl << std::endl;
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

      statFileDist << "# Distance in byte" << "," << "Number of matches" << "," << "Total % of matches" << std::endl << std::endl;

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

      statFileLength << "# Length in byte" << "," << "Number of length" << "," << "Total % of Length" << std::endl << std::endl;

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

      for (uint32_t i = 1; i < (chunkSize[chunkSize.size()-1] + 64); i++) {
         std::stringstream ss;
         ss << i;
         for (int32_t chunckIndex = chunkSize.size()-1; 0 <= chunckIndex; --chunckIndex) {
            if ((chunkSize[chunckIndex] + 64) > i) {
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

      statFile << std::fixed << std::setprecision(1) << std::endl;
      statFile << "# Total chunks (size = " << chunkSize[chunckIndex] << ") processed = " << lz4Reader.totalChunkCount << " chunks compressed = " << lz4Reader.totalChunkCompress << " (" << (lz4Reader.totalChunkCompress * 100.0) / lz4Reader.totalChunkCount << ")" << std::endl;
      statFile << std::fixed << std::setprecision(2);

      statFile << std::endl << "# Compression ratio with threshold (" << threshold << ") = " << (double)lz4Reader.totalSizeReadStat / (double)lz4Reader.totalSizeCompressStat << std::endl;
      statFile << "# Compression ratio global (all chunk) = " << (double)lz4Reader.totalSizeRead / (double)lz4Reader.totalSizeCompress << std::endl << std::endl;

      statFile.close();
      statFileDist.close();
      statFileLength.close();
      statFileDistLength.close();
      statFileNewL4.close();
      dumpFileNewL4.close();
   }


   //for (uint32_t i = 1; i < (dataChunk[chunckIndex] + 64); i++) {
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
