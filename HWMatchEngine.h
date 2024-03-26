#pragma once

#include <cstdint>
#include <vector>

#include "matchlz4.h"


#define CHUNKSIZE  4096
#define MINMLENGHT 4
#define NB_BYTE    8
#define NB_CELL    (CHUNKSIZE/NB_BYTE)

struct Matchstruct
{
    bool valid;
    int  pos;
    int  offset;
    int  length;
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
    void findLargeMatch();
    void processClock();
    Matchstruct getMatch(int pos);
    Matchstruct getLargeMatch();

    int  small_counter[NB_BYTE];
    bool large_count_status[NB_BYTE];
    int  large_counter;
    unsigned char data[NB_BYTE];
    History history[NB_BYTE * 2 - 1];
    History history_reg[NB_BYTE];
    History history_reg_next[NB_BYTE];
    bool comparator[NB_BYTE][NB_BYTE];
    Matchstruct match_list[NB_BYTE];
    Matchstruct large_match;
    bool last_data = 0;
    bool verbose = 0;

};

class match_detection_model
{
public:

    void init();
    void loadData(unsigned char* data);
    void processCycle();
    Matchstruct* getStandardMatch();
    Matchstruct* getLargeMatch();
    Matchstruct* getMatchList();

    bool verbose = 0;
    Matchstruct matchList[CHUNKSIZE];
    int cycle = 0;

    Matchstruct match;
    Matchstruct standard_match[NB_BYTE];
    Matchstruct large_match;

    match_cell_model match_cell[NB_CELL];

    History  history[CHUNKSIZE];
    History  current_history[NB_BYTE];
    History  next_history[NB_BYTE];

    unsigned char input_string[NB_BYTE];

};


void hw_model_compress(std::vector<Match>& matches, const uint64_t& blockSize, const unsigned char* dataBlock);
