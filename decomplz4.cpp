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

#pragma push_macro("NDEBUG")
#undef NDEBUG
#include <cassert>
#pragma pop_macro("NDEBUG")


#include <stdio.h>  // stdin/stdout/stderr, fopen, ...
#include <stdlib.h> // exit()
#include <string.h> // memcpy
#include <iostream>

#include "matchlz4.h"
#include "decomplz4.h"

#ifndef FALSE
#define FALSE 0
#define TRUE  1
#endif

/// error handler
void unlz4error(const char* msg)
{
   // smaller static binary than fprintf(stderr, "ERROR: %s\n", msg);
   fputs("ERROR: ", stderr);
   fputs(msg, stderr);
   fputc('\n', stderr);
   exit(1);
}



// ==================== LZ4 DECOMPRESSOR ====================

/// decompress everything in input stream (accessed via getByte) and write to output stream (via sendBytes)
void unlz4_userPtr(const std::shared_ptr<LZ4Factory> & lz4Factory, DECOMP_GET_BYTE getByte, DECOMP_SEND_BYTES sendBytes, const char* dictionary, void* userPtr)
{
   // signature
   // 
   LZ4DecompReader* lz4Reader = reinterpret_cast<LZ4DecompReader*>(userPtr);

   std::map<uint64_t, uint64_t>& mapDist = lz4Reader->mapDist;
   std::map<uint64_t, uint64_t>& mapLength = lz4Reader->mapLength;
   std::map<uint64_t, std::map<uint64_t, uint64_t>>& mapDistLength = lz4Reader->mapDistLength;

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
   uint64_t pos = 0;

   struct LZ4DecompReader* decompReader = (struct LZ4DecompReader*)userPtr;

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
         uint64_t blockOffset = 0;
         uint64_t numWritten = 0;

         while (blockOffset < blockSize)
         {
            //uint64_t origBlockOffset = blockOffset;

            std::shared_ptr<LZ4Sequence> lz4Sequence = lz4Factory->getSequence((const uint8_t *)(decompReader->compBuffer), blockSize - blockOffset);

            uint64_t seqSize = lz4Sequence->getSeqData().size();
            uint64_t distance = lz4Sequence->getMatchOffset();
            uint64_t numLiterals = lz4Sequence->getLiteralLength();
            uint64_t matchLength = lz4Sequence->getMatchLength();
            const uint8_t* literalData = lz4Sequence->getLiteralData();

            // copy all those literals
            if (pos + numLiterals < HISTORY_SIZE)
            {
               while (numLiterals-- > 0) {
                  unsigned char oneLiteral = *literalData++;
                  history[pos++] = oneLiteral;
               }
            }
            else {
               assert(false);
            }

            // last token has only literals
            if (lz4Sequence->isLastToken()) {
               decompReader->compBuffer += seqSize;
               decompReader->available -= seqSize;
               break;
            }
            // zero isn't allowed
            if (distance == 0) {
               assert(false);
               unlz4error("invalid offset");
            }
            mapDist[distance]++;
            mapLength[matchLength]++;
            mapDistLength[distance][matchLength]++;

            // copy match
            uint64_t referencePos = (pos >= distance) ? (pos - distance) : (HISTORY_SIZE + pos - distance);
            // start and end within the current 64k block ?
            if (pos + matchLength < HISTORY_SIZE && referencePos + matchLength < HISTORY_SIZE)
            {
               // read/write continuous block (no wrap-around at the end of history[])
               if (pos >= referencePos + matchLength || referencePos >= pos + matchLength)
               {
                  // non-overlapping
                  memcpy(history + pos, history + referencePos, matchLength);
                  pos += matchLength;
               }
               else {
                  // overlapping, slower byte-wise copy
                  while (matchLength-- > 0)
                     history[pos++] = history[referencePos++];
               }
            }
            else {
               assert(false);
            }
            decompReader->available -= seqSize;
            decompReader->compBuffer += seqSize;
            blockOffset += seqSize;
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
void unlz4(DECOMP_GET_BYTE getByte, DECOMP_SEND_BYTES sendBytes, const char* dictionary)
{
   //unlz4_userPtr(getByte, sendBytes, dictionary, NULL, );
}
