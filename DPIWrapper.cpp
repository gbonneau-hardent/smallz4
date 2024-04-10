#include "DPIWrapper.h"


/// @brief Create and return an initialized compression model with a self reference to keep it alive duting the simulation
/// @return a handle to the model
extern "C" chandle InitCompression()
{
    auto match_detection = std::make_shared<match_detection_model>();
    match_detection->AddSelfReference();
    match_detection->init();

    return reinterpret_cast<chandle>(match_detection.get());
}

/// @brief Clear the compression memory
/// @param compression_model handle to the compression model
/// @return status of casting (0 success and 1 error)
extern "C" uint32_t ClearCompression(chandle compression_model)
{
    match_detection_model* match_detection = dynamic_cast<match_detection_model*>(reinterpret_cast<match_detection_model*>(compression_model));

    if (match_detection == nullptr) {
      return 1;
    }
    match_detection->init();

    return 0;
}

/// @brief Destroy the object by removing all reference to it (since DPI chandle are not garbage collected by SystemVerilog)
/// @param compression_model
/// @return a void pointer
extern "C" chandle DestroyCompression(chandle compression_model)
{
   match_detection_model* match_detection = dynamic_cast<match_detection_model*>(reinterpret_cast<match_detection_model*>(compression_model));

   if (match_detection == nullptr) {
      return match_detection;
   }

   printf("Deleting the compression_model\n");

   match_detection->RemoveSelfReference();
   match_detection = nullptr;

   return nullptr;
}

/// @brief Send data to model and process it
/// @param data array of NB_BYTES bytes
/// @param compression_model handle to the compression model
/// @return status of casting (0 success and 1 error)
extern "C" uint32_t PushData(unsigned char data[NB_BYTE], chandle compression_model)
{
    match_detection_model *match_detection = dynamic_cast<match_detection_model *>(reinterpret_cast<match_detection_model *>(compression_model));

    if (match_detection == nullptr) {
        return 1;
    }

    match_detection->loadData(data);
    match_detection->processCycle();

    printf("verbose: %u\n", match_detection->verbose);

    return 0;
}

/// @brief Get the list of match
/// @param match_list match list after compressing whole page
/// @param compression_model handle to the compression model
/// @return status of casting (0 success and 1 error)
extern "C" uint32_t GetMatch(Matchstruct** match_list, chandle compression_model)
{
    match_detection_model *match_detection = dynamic_cast<match_detection_model *>(reinterpret_cast<match_detection_model *>(compression_model));

    if (match_detection == nullptr) {
        return 1;
    }

    *match_list = match_detection->getMatchList();

    // for (size_t i = 0; i < CHUNKSIZE; i++)
    // {
    //     if (match_list[i].valid)
    //     {
    //         printf("MRV: match_list[%llu].valid = %u\n", i, match_list[i].valid);
    //         printf("MRV: match_list[%llu].pos = %u\n", i, match_list[i].pos);
    //         printf("MRV: match_list[%llu].offset = %u\n", i, match_list[i].offset);
    //         printf("MRV: match_list[%llu].length = %u\n", i, match_list[i].length);
    //     }

    // }

    return 0;
}
