#pragma once

#include <cstdint>
#include <vector>
#include <memory>

#include "matchlz4.h"


#define CHUNKSIZE  4096
#define MINMLENGHT 4
#define NB_BYTE    8
#define NB_CELL    (CHUNKSIZE/NB_BYTE)

struct Matchstruct
{
    bool valid;
    int  offset;
    int  length;
    int  large_counter;
};

struct History
{
    bool valid;
    unsigned char history;
};

class match_cell_model
{
public:

    void init();
    void loadDataHistory(unsigned char* data_in, bool last_data_in, History* history_in, History* history_out);
    void compareDataHisoty();
    void updateSmallCounter();
    void updateLargeCounterAndStatus();
    void processClock();
    Matchstruct getMatch(int pos);

    int  small_counter[NB_BYTE];
    bool large_count_status[NB_BYTE];
    int  large_counter;
    unsigned char data[NB_BYTE];
    History history[NB_BYTE * 2 - 1];
    History history_reg[NB_BYTE];
    History history_reg_next[NB_BYTE];
    bool comparator[NB_BYTE][NB_BYTE];
    Matchstruct match_list[NB_BYTE];
    bool last_data = 0;

    bool verbose = 0;

};

class match_detection_model: public std::enable_shared_from_this<match_detection_model>
{
public:

    std::shared_ptr<match_detection_model> AddSelfReference()
    {
        if (selfReference == nullptr) {
            auto dpiCallReference = shared_from_this();
            selfReference = std::dynamic_pointer_cast<match_detection_model>(dpiCallReference);
        }
        return selfReference;
    }

    void RemoveSelfReference() {

        selfReference = nullptr;
    }

    ~match_detection_model() {
        printf("Delete the match_detection_model\n");
    }

    void init();
    void loadData(unsigned char* data);
    void processCycle();

    unsigned char input_string[NB_BYTE];

    match_cell_model match_cell[NB_CELL];

    History  history[CHUNKSIZE];
    History  current_history[NB_BYTE];
    History  next_history[NB_BYTE];

    Matchstruct* getMatchList();
    Matchstruct* getMatchListStartPos();
    Matchstruct matchList[CHUNKSIZE];
    Matchstruct matchList_startpos[CHUNKSIZE];

    bool verbose = 0;
    int cycle = 0;

    unsigned int call_counter = 0; // WHAT IS THAT?

private:
    std::shared_ptr<match_detection_model> selfReference;

};


void hw_model_compress(std::vector<Match>& matches, const uint64_t& blockSize, const unsigned char* dataBlock);
