#ifndef __DPI_CALL__
#define __DPI_CALL__

#ifndef chandle
#define chandle void *
#endif // !chandle


#ifdef __cplusplus

#include <cstdint>
#include <memory>

struct ContextLZ4;
struct LZ4CompReader;
struct LZ4DecompReader;

class DPICall
{
   public:

      virtual chandle CreateContext()                                                                                                         = 0;
      virtual void DestroyContext()                                                                                                           = 0;

      virtual int32_t ParseOption(int argc, const char* argv[], ContextLZ4& contextLZ4)                                                       = 0;
      virtual int32_t InitCompression(ContextLZ4& lz4Context, LZ4CompReader& lz4Reader, uint64_t chunkIndex)                                  = 0;
      virtual int32_t InitDecompression(ContextLZ4& lz4Context, LZ4DecompReader& lz4DecompReader, uint64_t chunkIndex)                        = 0;
      virtual int32_t InitReader(ContextLZ4& lz4Context, LZ4CompReader& lz4Reader, std::shared_ptr<std::ifstream>& inputFile)                 = 0;
      virtual int32_t Compress(ContextLZ4& contextLZ4, LZ4CompReader& lz4Reader, uint32_t chunckIndex)                                        = 0;
      virtual int32_t Decompress(ContextLZ4& contextLZ4, LZ4DecompReader& lz4DecompReader, uint32_t chunckIndex)                              = 0;
      virtual int32_t DumpFileStat(ContextLZ4& contextLZ4)                                                                                    = 0;
      virtual int32_t DumpChunkStat(ContextLZ4& contextLZ4, LZ4CompReader& lz4Reader, LZ4DecompReader& lz4DecompReader, uint32_t chunckIndex) = 0;
      virtual int32_t Close(ContextLZ4& contextLZ4, LZ4CompReader& lz4Reader, LZ4DecompReader& lz4DecompReader)                               = 0;

      virtual std::shared_ptr<std::ifstream> getNextFile(LZ4CompReader& lz4Reader)                                                            = 0;

   private:

};

//extern "C" int32_t comp_c_model(int argc, const char* argv[]);

#else

// #include <stdint.h>


// chandle CreateContext();
// chandle DestroyContext(chandle in_corpuslz4);

// int32_t ParseOption(int argc, const char* argv[], chandle corpusLZ4);
// int32_t InitCompression(chandle corpusLZ4, uint64_t chunkIndex);
// int32_t InitDecompression(chandle corpusLZ4, uint64_t chunkIndex);
// int32_t InitReader(chandle corpusLZ4, chandle compFile);
// int32_t Compress(chandle corpusLZ4, uint32_t chunckIndex);
// int32_t Decompress(chandle corpusLZ4, uint32_t chunckIndex);
// int32_t DumpFileStat(chandle corpusLZ4);
// int32_t DumpChunkStat(chandle corpusLZ4, uint32_t chunckIndex);
// int32_t Close(chandle corpusLZ4);

// chandle getNextFile(chandle corpusLZ4);

#endif
#endif
