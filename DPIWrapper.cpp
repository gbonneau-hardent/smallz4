#include "DPIWrapper.h"


/// @brief Create and return an initialized compression model
/// @return a handle to the model
extern "C" chandle InitCompression()
{
    auto match_detection = std::make_shared<match_detection_model>();
    match_detection->init();

    return reinterpret_cast<chandle>(match_detection.get());
}

/// @brief Send data to model and process it
/// @param data array of NB_BYTES bytes
/// @param compression_model handle to the compression model
/// @return status of casting (1 success and 0 error)
extern "C" uint32_t PushData(unsigned char data[NB_BYTE], chandle compression_model)
{
    match_detection_model *match_detection = dynamic_cast<match_detection_model *>(reinterpret_cast<match_detection_model *>(compression_model));

    if (compression_model) {
        return 0;
    }

    match_detection->loadData(data);
    match_detection->processCycle();

    return 1;
}

/// @brief Get the list of match
/// @param match_list match list after compressing whole page
/// @param compression_model handle to the compression model
/// @return status of casting (1 success and 0 error)
extern "C" uint32_t GetMatch(Matchstruct* match_list, chandle compression_model)
{
    match_detection_model *match_detection = dynamic_cast<match_detection_model *>(reinterpret_cast<match_detection_model *>(compression_model));

    if (compression_model) {
        return 0;
    }

    match_list = match_detection->getMatchList();

    return 1;
}
