#pragma once

#include <list>
#include <memory>
#include <map>

// ==================== I/O INTERFACE ====================


// read one byte from input, see getByteFromIn()  for a basic implementation
typedef unsigned char (*GET_BYTE)  (void* userPtr);
// write several bytes,      see sendBytesToOut() for a basic implementation
typedef void          (*SEND_BYTES)(const unsigned char*, unsigned int, void* userPtr);


void unlz4_userPtr(GET_BYTE getByte, SEND_BYTES sendBytes, const char* dictionary, void* userPtr);

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
