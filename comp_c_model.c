#include <stdint.h>
#include "DPICall.h"

#define true 1

int32_t comp_c_model(int argc, const char* argv[])
{
   chandle corpuslz4 = CreateContext();

   ParseOption(argc, argv, corpuslz4);

   for (uint32_t chunckIndex = 0; chunckIndex < contextLZ4.chunkSize.size(); chunckIndex++) {

      InitCompression(corpuslz4, chunckIndex);
      InitDecompression(corpuslz4, chunckIndex);

      chandle compFile = NULL;

      while (compFile = getNextFile(corpuslz4)) {

         uint64_t numLoop = 0;
         InitReader(corpuslz4, compFile);

         while (true) {
            if (contextLZ4.maxChunk != 0) {
               if (numLoop++ > contextLZ4.maxChunk) {
                  break;
               }
            }
            Compress(corpuslz4, chunckIndex);
            if (lz4Reader.dataEof) {
               break;
            }
            lz4DecompReader.available = lz4Reader.dataCompressSize;
            lz4DecompReader.compBuffer = lz4Reader.compBuffer.get();
            lz4DecompReader.decompPos = 0;

            Decompress(corpuslz4, chunckIndex);

            int retCmp = strncmp(lz4Reader.fileBuffer.get(), lz4DecompReader.decompBuffer.get(), contextLZ4.chunkSize[chunckIndex]);
            if (retCmp != 0) {
               exit(-2);
            }
         }
         DumpFileStat(corpuslz4);
      }
      DumpChunkStat(corpuslz4, chunckIndex);
   }
   Close(corpuslz4);

   corpuslz4 = DestroyContext(corpuslz4);

   return 1;
}