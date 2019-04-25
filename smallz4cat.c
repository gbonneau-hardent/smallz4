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
// Corrupted data causes a call to error().

// suppress warnings when compiled by Visual C++
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>  // stdin/stdout/stderr, fopen, ...
#include <stdlib.h> // exit()
#include <string.h> // memcpy

#ifndef FALSE
#define FALSE 0
#define TRUE  1
#endif

/// error handler
void error(const char* msg)
{
  // smaller static binary than fprintf(stderr, "ERROR: %s\n", msg);
  fputs("ERROR: ", stderr);
  fputs(msg,       stderr);
  fputc('\n',      stderr);
  exit(1);
}


// ==================== I/O INTERFACE ====================


// read one byte from input, see getByteFromIn()  for a basic implementation
typedef unsigned char (*GET_BYTE)  ();
// write several bytes,      see sendBytesToOut() for a basic implementation
typedef void          (*SEND_BYTES)(const unsigned char*, unsigned int);

/// input stream,  usually stdin
static FILE* in = NULL;
/// read a single byte (with simple buffering)
static unsigned char getByteFromIn()
{
  // modify buffer size as you like ... for most use cases, bigger buffer aren't faster anymore - and even reducing to 1 byte works !
#define READ_BUFFER_SIZE 4*1024
  static unsigned char readBuffer[READ_BUFFER_SIZE];
  static unsigned int  pos       = 0;
  static unsigned int  available = 0;

  // refill buffer
  if (pos == available)
  {
    pos = 0;
    available = fread(readBuffer, 1, READ_BUFFER_SIZE, in);
    if (available == 0)
      error("out of data");
  }

  // return a byte
  return readBuffer[pos++];
}

/// output stream, usually stdout
static FILE* out = NULL;
/// write a block of bytes
static void sendBytesToOut(const unsigned char* data, unsigned int numBytes)
{
  if (data != NULL && numBytes > 0)
    fwrite(data, 1, numBytes, out);
}


// ==================== LZ4 DECOMPRESSOR ====================

/// decompress everything in input stream (accessed via getByte) and write to output stream (via sendBytes)
void unlz4(GET_BYTE getByte, SEND_BYTES sendBytes, const char* dictionary)
{
  // signature
  unsigned char signature1 = getByte();
  unsigned char signature2 = getByte();
  unsigned char signature3 = getByte();
  unsigned char signature4 = getByte();
  unsigned int  signature  = (signature4 << 24) | (signature3 << 16) | (signature2 << 8) | signature1;
  unsigned char isModern   = (signature == 0x184D2204);
  unsigned char isLegacy   = (signature == 0x184C2102);
  if (!isModern && !isLegacy)
    error("invalid signature");

  unsigned char hasBlockChecksum   = FALSE;
  unsigned char hasContentSize     = FALSE;
  unsigned char hasContentChecksum = FALSE;
  unsigned char hasDictionaryID    = FALSE;
  if (isModern)
  {
    // flags
    unsigned char flags = getByte();
    hasBlockChecksum   = flags & 16;
    hasContentSize     = flags &  8;
    hasContentChecksum = flags &  4;
    hasDictionaryID    = flags &  1;

    // only version 1 file format
    unsigned char version = flags >> 6;
    if (version != 1)
      error("only LZ4 file format version 1 supported");

    // ignore blocksize
    getByte();

    if (hasContentSize)
    {
      // ignore, skip 8 bytes
      getByte(); getByte(); getByte(); getByte();
      getByte(); getByte(); getByte(); getByte();
    }
    if (hasDictionaryID)
    {
      // ignore, skip 4 bytes
      getByte(); getByte(); getByte(); getByte();
    }

    // ignore header checksum (xxhash32 of everything up this point & 0xFF)
    getByte();
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
    FILE* dict = fopen(dictionary, "rb");
    if (!dict)
      error("cannot open dictionary");

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
    unsigned int blockSize = getByte();
    blockSize |= (unsigned int)getByte() <<  8;
    blockSize |= (unsigned int)getByte() << 16;
    blockSize |= (unsigned int)getByte() << 24;

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
      unsigned int numWritten  = 0;
      while (blockOffset < blockSize)
      {
        // get a token
        unsigned char token = getByte();
        blockOffset++;

        // determine number of literals
        unsigned int numLiterals = token >> 4;
        if (numLiterals == 15)
        {
          // number of literals length encoded in more than 1 byte
          unsigned char current;
          do
          {
            current = getByte();
            numLiterals += current;
            blockOffset++;
          } while (current == 255);
        }

        blockOffset += numLiterals;

        // copy all those literals
        if (pos + numLiterals < HISTORY_SIZE)
        {
          // fast loop
          while (numLiterals-- > 0)
            history[pos++] = getByte();
        }
        else
        {
          // slow loop
          while (numLiterals-- > 0)
          {
            history[pos++] = getByte();

            // flush output buffer
            if (pos == HISTORY_SIZE)
            {
              sendBytes(history, HISTORY_SIZE);
              numWritten += HISTORY_SIZE;
              pos = 0;
            }
          }
        }

        // last token has only literals
        if (blockOffset == blockSize)
          break;

        // match distance is encoded in two bytes (little endian)
        unsigned int delta = getByte();
        delta |= (unsigned int)getByte() << 8;
        // zero isn't allowed
        if (delta == 0)
          error("invalid offset");
        blockOffset += 2;

        // match length (always >= 4, therefore length is stored minus 4)
        unsigned int matchLength = 4 + (token & 0x0F);
        if (matchLength == 4 + 0x0F)
        {
          unsigned char current;
          do // match length encoded in more than 1 byte
          {
            current = getByte();
            matchLength += current;
            blockOffset++;
          } while (current == 255);
        }

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
              sendBytes(history, HISTORY_SIZE);
              numWritten += HISTORY_SIZE;
              pos = 0;
            }
            // wrap-around of read location
            referencePos %= HISTORY_SIZE;
          }
        }
      }

      // all legacy blocks must be completely filled - except for the last one
      if (isLegacy && numWritten + pos < 8*1024*1024)
        break;
    }
    else
    {
      // copy uncompressed data and add to history, too (if next block is compressed and some matches refer to this block)
      while (blockSize-- > 0)
      {
        // copy a byte ...
        history[pos++] = getByte();
        // ... until buffer is full => send to output
        if (pos == HISTORY_SIZE)
        {
          sendBytes(history, HISTORY_SIZE);
          pos = 0;
        }
      }
    }

    if (hasBlockChecksum)
    {
      // ignore checksum, skip 4 bytes
      getByte(); getByte(); getByte(); getByte();
    }
  }

  if (hasContentChecksum)
  {
    // ignore checksum, skip 4 bytes
    getByte(); getByte(); getByte(); getByte();
  }

  // flush output buffer
  sendBytes(history, pos);
}


// ==================== COMMAND-LINE HANDLING ====================


/// parse command-line
int main(int argc, const char* argv[])
{
  // default input/output streams
  in = stdin; out = stdout;
  const char* dictionary = NULL;

  // first command-line parameter is our input filename / but ignore "-" which stands for STDIN
  int parameter;
  for (parameter = 1; parameter < argc; parameter++)
  {
    const char* current = argv[parameter];
    // dictionary
    if (current[0] == '-' && current[1] == 'D')
    {
      if (parameter + 1 >= argc)
        error("no dictionary filename found");
      dictionary = argv[++parameter];
      continue;
    }

    // filename
    // read from STDIN, default behavior
    if (current[0] != '-' && current[1] != '\0')
    {
      // already have a filename - at most one filename is allowed (except for dictionary) ?
      if (in != stdin)
        error("can only decompress one file at a time");
      // get handle
      in = fopen(argv[1], "rb");
      if (!in)
        error("file not found");
    }
  }

  // and go !
  unlz4(getByteFromIn, sendBytesToOut, dictionary);
  return 0;
}
