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
extern "C" uint32_t PushData(unsigned char data[NB_BYTE+1], chandle compression_model)
{
    match_detection_model *match_detection = dynamic_cast<match_detection_model *>(reinterpret_cast<match_detection_model *>(compression_model));

    if (match_detection == nullptr) {
        return 1;
    }

    match_detection->loadData(data);
    match_detection->processCycle();

    // printf("verbose: %u\n", match_detection->verbose);

    return 0;
}

/// @brief Get the list of cell match (one call per clk)
/// @param match_list match list of all the cell
/// @param compression_model handle to the compression model
/// @return status of casting (0 success and 1 error)
extern "C" uint32_t GetCellMatchArray(Matchstruct match_cell_list [CHUNKSIZE], chandle compression_model)
{
    match_detection_model *match_detection = dynamic_cast<match_detection_model *>(reinterpret_cast<match_detection_model *>(compression_model));
    // Matchstruct match_cell_list [CHUNKSIZE];

    if (match_detection == nullptr) {
        return 1;
    }


    for (size_t cell = 0; cell < NB_CELL; cell++)
    {
        for (size_t pos = 0; pos < NB_BYTE; pos++)
        {
            uint32_t pos1d = pos+(cell*NB_BYTE);
            match_cell_list[pos1d] = match_detection->match_cell[cell].getMatch(pos);

            // if ((*match_cell_list)[pos1d].valid)
            // {
            //     printf("GetCellMatchArray: match_cell_list[%u].valid = %u\n", pos1d, (*match_cell_list)[pos1d].valid);
            //     printf("GetCellMatchArray: match_cell_list[%u].offset = %u\n", pos1d, (*match_cell_list)[pos1d].offset);
            //     printf("GetCellMatchArray: match_cell_list[%u].length = %u\n", pos1d, (*match_cell_list)[pos1d].length);
            //     printf("GetCellMatchArray: match_cell_list[%u].large_counter = %u\n", pos1d, (*match_cell_list)[pos1d].large_counter);
            // }
        }
    }

    // *match_list = match_cell_list;

    // for (size_t idx = 0; idx < CHUNKSIZE; idx++)
    // {
    //     if ((*match_list)[idx].valid)
    //     {
    //         printf("GetCellMatchArray: match_list[%llu].valid = %u\n", idx, (*match_list)[idx].valid);
    //         printf("GetCellMatchArray: match_list[%llu].offset = %u\n", idx, (*match_list)[idx].offset);
    //         printf("GetCellMatchArray: match_list[%llu].length = %u\n", idx, (*match_list)[idx].length);
    //         printf("GetCellMatchArray: match_list[%llu].large_counter = %u\n", idx, (*match_list)[idx].large_counter);
    //     }

    // }

    return 0;
}

/// @brief Get the list of cell match after cell merge
/// @param match_list match list of all the cell merged
/// @param compression_model handle to the compression model
/// @return status of casting (0 success and 1 error)
extern "C" uint32_t GetCellMergedMatchArray(Matchstruct** match_cell_list, chandle compression_model)
{
    match_detection_model *match_detection = dynamic_cast<match_detection_model *>(reinterpret_cast<match_detection_model *>(compression_model));
    // Matchstruct match_cell_list [CHUNKSIZE];

    if (match_detection == nullptr) {
        return 1;
    }

    *match_cell_list = match_detection->getMatchList();


    // for (size_t idx = 0; idx < CHUNKSIZE; idx++)
    // {

    //     if ((*match_cell_list)[idx].valid)
    //     {
    //         printf("GetCellMergedMatchArray: match_cell_list[%lu].valid = %u\n", idx, (*match_cell_list)[idx].valid);
    //         printf("GetCellMergedMatchArray: match_cell_list[%lu].offset = %u\n", idx, (*match_cell_list)[idx].offset);
    //         printf("GetCellMergedMatchArray: match_cell_list[%lu].length = %u\n", idx, (*match_cell_list)[idx].length);
    //         printf("GetCellMergedMatchArray: match_cell_list[%lu].large_counter = %u\n", idx, (*match_cell_list)[idx].large_counter);
    //     }

    // }

    return 0;
}

/// @brief Get the list of match merged (one call per chunk)
/// @param match_list match list of all the cell merged
/// @param compression_model handle to the compression model
/// @return status of casting (0 success and 1 error)
extern "C" uint32_t GetMatch(Matchstruct** match_list, chandle compression_model)
{
    match_detection_model *match_detection = dynamic_cast<match_detection_model *>(reinterpret_cast<match_detection_model *>(compression_model));

    if (match_detection == nullptr) {
        return 1;
    }

    *match_list = match_detection->getMatchListStartPos();

    // for (size_t i = 0; i < CHUNKSIZE; i++)
    // {
    //     if (*match_list[i].valid)
    //     {
    //         printf("GetMatch: match_list[%llu].valid = %u\n", i, *match_list[i].valid);
    //         printf("GetMatch: match_list[%llu].offset = %u\n", i, *match_list[i].offset);
    //         printf("GetMatch: match_list[%llu].length = %u\n", i, *match_list[i].length);
    //         printf("GetMatch: match_list[%llu].large_counter = %u\n", i, *match_list[i].large_counter);
    //     }

    // }

    return 0;
}
