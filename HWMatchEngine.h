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
    void updateMatchList();
    void updateLargeCounterAndStatus();
    void processClock();
    Matchstruct getMatch(int pos);

    int  small_counter[NB_BYTE];
    bool large_count_status[NB_BYTE];
    int  large_counter;
    unsigned char data[NB_BYTE + 1];
    History history[NB_BYTE * 2 + 1 - 1];
    History history_reg[NB_BYTE + 1];
    History history_reg_next[NB_BYTE + 1];
    bool comparator[NB_BYTE][NB_BYTE + 1];
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

    }

    void init();
    void loadData(unsigned char* data);
    void processCycle();

    unsigned char input_string[NB_BYTE + 1];

    match_cell_model match_cell[NB_CELL];

    History  history[CHUNKSIZE];
    History  current_history[NB_BYTE + 1];
    History  next_history[NB_BYTE + 1];

    Matchstruct* getMatchList();
    Matchstruct* getMatchListStartPos();
    Matchstruct matchList[CHUNKSIZE];
    Matchstruct matchList_startpos[CHUNKSIZE];

    bool verbose = 0;
    int cycle = 0;
    int vcycle = 0;

private:
    std::shared_ptr<match_detection_model> selfReference;

};


void hw_model_compress(std::vector<Match>& matches, const uint64_t& blockSize, const unsigned char* dataBlock);
