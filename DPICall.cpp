#include "corpuslz4.h"

// extern "C" chandle CreateContext()
// {
//    auto test = std::make_shared<CorpusLZ4>();
//    test->Reference();

//    return reinterpret_cast<chandle>(test.get());
// };

// extern "C" chandle DestroyContext(chandle in_corpuslz4)
// {
//    CorpusLZ4* corpuslz4 = dynamic_cast<CorpusLZ4*>(reinterpret_cast<CorpusLZ4*>(in_corpuslz4));

//    if (corpuslz4 == nullptr) {
//       return corpuslz4;
//    }

//    std::shared_ptr<CorpusLZ4> shrCorpuslz4 = std::dynamic_pointer_cast<CorpusLZ4>(reinterpret_cast<CorpusLZ4*>(corpuslz4)->shared_from_this());
//    corpuslz4->DestroyContext();
//    corpuslz4 = nullptr;

//    return nullptr;
// }


// extern "C" uint32_t InitCompression(chandle corpusLZ4, uint64_t chunkIndex) {

//    CorpusLZ4 * ptrCorpuslz4 = dynamic_cast<CorpusLZ4*>(reinterpret_cast<CorpusLZ4*>(corpusLZ4));

//    if (ptrCorpuslz4 == nullptr) {
//       return 0;
//    }
//    return ptrCorpuslz4->InitCompression(*ptrCorpuslz4->getContextlz4().get(), *ptrCorpuslz4->getComplz4().get(), chunkIndex);
// }


// extern "C" int32_t InitDecompression(chandle corpusLZ4, uint64_t chunkIndex) {

//    CorpusLZ4* ptrCorpuslz4 = dynamic_cast<CorpusLZ4*>(reinterpret_cast<CorpusLZ4*>(corpusLZ4));

//    if (ptrCorpuslz4 == nullptr) {
//       return 0;
//    }
//    return ptrCorpuslz4->InitDecompression(*ptrCorpuslz4->getContextlz4().get(), *ptrCorpuslz4->getDecomplz4().get(), chunkIndex);
// }


// extern "C" int32_t InitReader(chandle corpusLZ4, chandle compFile) {

//    CorpusLZ4* ptrCorpuslz4 = dynamic_cast<CorpusLZ4*>(reinterpret_cast<CorpusLZ4*>(corpusLZ4));

//    if (ptrCorpuslz4 == nullptr) {
//       return 0;
//    }
//    if (compFile != ptrCorpuslz4->getComplz4()->compFile.get()) {
//       return 0;
//    }
//    return ptrCorpuslz4->InitReader(*ptrCorpuslz4->getContextlz4().get(), *ptrCorpuslz4->getComplz4().get(), ptrCorpuslz4->getComplz4()->compFile);
// }


// extern "C" int32_t Compress(chandle corpusLZ4, uint32_t chunckIndex) {

//    CorpusLZ4* ptrCorpuslz4 = dynamic_cast<CorpusLZ4*>(reinterpret_cast<CorpusLZ4*>(corpusLZ4));

//    if (ptrCorpuslz4 == nullptr) {
//       return 0;
//    }
//    return ptrCorpuslz4->Compress(*ptrCorpuslz4->getContextlz4().get(), *ptrCorpuslz4->getComplz4().get(), chunckIndex);
// }


// extern "C" int32_t Decompress(chandle corpusLZ4, uint32_t chunckIndex) {

//    CorpusLZ4* ptrCorpuslz4 = dynamic_cast<CorpusLZ4*>(reinterpret_cast<CorpusLZ4*>(corpusLZ4));

//    if (ptrCorpuslz4 == nullptr) {
//       return 0;
//    }
//    return ptrCorpuslz4->Decompress(*ptrCorpuslz4->getContextlz4().get(), *ptrCorpuslz4->getDecomplz4().get(), chunckIndex);
// }


// extern "C" int32_t DumpFileStat(chandle corpusLZ4) {

//    CorpusLZ4* ptrCorpuslz4 = dynamic_cast<CorpusLZ4*>(reinterpret_cast<CorpusLZ4*>(corpusLZ4));

//    if (ptrCorpuslz4 == nullptr) {
//       return 0;
//    }
//    return ptrCorpuslz4->DumpFileStat(*ptrCorpuslz4->getContextlz4().get());
// }


// extern "C" int32_t DumpChunkStat(chandle corpusLZ4, uint32_t chunckIndex) {

//    CorpusLZ4* ptrCorpuslz4 = dynamic_cast<CorpusLZ4*>(reinterpret_cast<CorpusLZ4*>(corpusLZ4));

//    if (ptrCorpuslz4 == nullptr) {
//       return 0;
//    }
//    return ptrCorpuslz4->DumpChunkStat(*ptrCorpuslz4->getContextlz4().get(), *ptrCorpuslz4->getComplz4().get(), *ptrCorpuslz4->getDecomplz4().get(), chunckIndex);
// }


// extern "C" int32_t Close(chandle corpusLZ4) {

//    CorpusLZ4* ptrCorpuslz4 = dynamic_cast<CorpusLZ4*>(reinterpret_cast<CorpusLZ4*>(corpusLZ4));

//    if (ptrCorpuslz4 == nullptr) {
//       return 0;
//    }
//    return ptrCorpuslz4->Close(*ptrCorpuslz4->getContextlz4().get(), *ptrCorpuslz4->getComplz4().get(), *ptrCorpuslz4->getDecomplz4().get());
// }


// extern "C" chandle getNextFile(chandle corpusLZ4) {

//    CorpusLZ4* ptrCorpuslz4 = dynamic_cast<CorpusLZ4*>(reinterpret_cast<CorpusLZ4*>(corpusLZ4));

//    if (ptrCorpuslz4 == nullptr) {
//       return 0;
//    }
//    auto compFile = ptrCorpuslz4->getNextFile(*ptrCorpuslz4->getComplz4().get());

//    return compFile.get();
// }
