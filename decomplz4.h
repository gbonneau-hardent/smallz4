#pragma once

#include <list>
#include <memory>
#include <map>

// ==================== I/O INTERFACE ====================


// read one byte from input, see getByteFromIn()  for a basic implementation
typedef unsigned char (*DECOMP_GET_BYTE)  (void* userPtr);
// write several bytes,      see sendBytesToOut() for a basic implementation
typedef void          (*DECOMP_SEND_BYTES)(const unsigned char*, unsigned int, void* userPtr);

void unlz4_userPtr(DECOMP_GET_BYTE getByte, DECOMP_SEND_BYTES sendBytes, const char* dictionary, void* userPtr);
void unlz4error(const char* msg);

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

   uint64_t available = 0;
   uint64_t decompPos = 0;
   uint64_t chunkSize = 0;

   std::map<uint64_t, uint64_t> mapDist;
   std::map<uint64_t, uint64_t> mapLength;
   std::map<uint64_t, std::map<uint64_t, uint64_t>> mapDistLength;
};
