#include <vector>
#include <iostream>

#include "matchlz4.h"
#include "HWMatchEngine.h"

#define CHUNKSIZE  4096
#define MINMLENGHT 4
#define NB_BYTE    8
#define NB_CELL    (CHUNKSIZE/NB_BYTE)

void match_cell_model::init()
{
    large_counter = 0;
    for (int ii = 0; ii < NB_BYTE; ii++)
    {
        small_counter[ii] = 0;
        large_count_status[ii] = 0;
        data[ii] = 0;
        history_reg[ii].valid = 0;
        for (int jj = 0; jj < NB_BYTE; jj++)
        {
        comparator[ii][jj] = 0;
        }
    }
    for (int ii = 0; ii < 2 * NB_BYTE - 1; ii++)
        history[ii].valid = 0;
}

void match_cell_model::loadDataHistory(unsigned char* data_in, bool last_data_in, History* history_in, History* history_out)
{


    for (int ii = NB_BYTE; ii < 2 * NB_BYTE - 1; ii++)
    {
        history[ii] = history_in[ii - NB_BYTE];
    }
    for (int ii = 0; ii < NB_BYTE; ii++)
    {
        history_reg_next[ii] = history_in[ii];
    }
    for (int ii = 0; ii < NB_BYTE; ii++)
    {
        history[ii] = history_reg[ii];
    }

    for (int ii = 0; ii < NB_BYTE; ii++) {
        data[ii] = data_in[ii];
        history_out[ii] = history_reg[ii];
    }

    last_data = last_data_in;

}


void match_cell_model::compareDataHisoty()
{
    if (verbose) printf("\nCOMPARATORS\n");
    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        for (int offset = 0; offset < NB_BYTE; offset++)
        {
        if (verbose) printf("(%d==%d)", data[pos], history[NB_BYTE - 1 + pos - offset].history);
        if (verbose)printf(".%d", history[NB_BYTE - 1 + pos - offset].valid);
        if ((data[pos] == history[NB_BYTE - 1 + pos - offset].history) && (history[NB_BYTE - 1 + pos - offset].valid) && !((pos == NB_BYTE - 1) && (last_data == 1)))
            comparator[offset][pos] = 1;
        else
            comparator[offset][pos] = 0;
        if (verbose) printf("-%d\t", comparator[offset][pos]);
        }
        if (verbose) printf("\n");
    }
    if (verbose) printf("\n");
}

void match_cell_model::updateSmallCounter()
{
    if (verbose) printf("\nSMALL COUNTERS\n");
    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        match_list[pos].valid = 0;
        match_list[pos].length = 0;
        match_list[pos].offset = 0;
        for (int offset = 0; offset < NB_BYTE; offset++)
        {
        if (comparator[offset][pos])
        {
            if ((large_count_status[offset] != 1 || small_counter[offset] < NB_BYTE) && (small_counter[offset] != 2 * NB_BYTE - 1))
                small_counter[offset] ++;
        }
        else
        {
            if ((small_counter[offset] >= 4) && (match_list[pos].length < small_counter[offset]) && !(large_count_status[match_list[pos].offset] && !large_count_status[offset]))
            {
                match_list[pos].valid = 1;
                match_list[pos].length = small_counter[offset];
                match_list[pos].offset = offset;
                match_list[pos].pos = pos;
            }
            small_counter[offset] = 0;
            //large_count_status[offset] = 0;
        }
        if (verbose) printf(" %d", small_counter[offset]);
        }
        if (verbose) printf("\n");
    }
    if (verbose) printf("\n");
}

void match_cell_model::updateLargeCounterAndStatus()
{

    bool candidate_large_count_status[NB_BYTE];

    for (int offset = 0; offset < NB_BYTE; offset++)
    {
        if (small_counter[offset] >= NB_BYTE)
        candidate_large_count_status[offset] = 1;
        else
        candidate_large_count_status[offset] = 0;
    }

    bool not_completed = 0;
    // Check if all active LCS has been de-activated (Look for completion of large sequence)
    for (int offset = 0; offset < NB_BYTE; offset++)
    {
        if ((large_count_status[offset]) && (candidate_large_count_status[offset]))
        {
        not_completed = 1;
        }
    }



    if (not_completed == 0) // Completed!!!
    {
        for (int offset = 0; offset < NB_BYTE; offset++)
        {
        if (verbose) printf("%d=>", large_count_status[offset]);
        large_count_status[offset] = candidate_large_count_status[offset];
        if (verbose) printf("%d - ", large_count_status[offset]);
        }
        if (verbose) printf("\n");
        large_counter = 0;
    }
    else
    {
        for (int offset = 0; offset < NB_BYTE; offset++)
        {
            if (verbose) printf("%d=>", large_count_status[offset]);
            if (candidate_large_count_status[offset]==0)
            large_count_status[offset] = 0;
            if (verbose) printf("%d - ", large_count_status[offset]);
        }
        if (verbose) printf("\n");
        large_counter++;
    }


}

void match_cell_model::findLargeMatch()
{
    large_match.valid = 0;
    large_match.length = 0;

    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        if ((match_list[pos].valid) && (large_count_status[match_list[pos].offset]) && (match_list[pos].length>=NB_BYTE)) // Large Match Candidate
        {
        if (verbose) printf("LM candidate: %d length=%d pos=%d\n", pos, match_list[pos].length, match_list[pos].pos);
        if (large_match.length < (match_list[pos].length + match_list[pos].pos))
        {
            if (verbose) printf("PASSING LM candidate: %d length=%d pos=%d\n", pos, match_list[pos].length, match_list[pos].pos);
            large_match.valid = 1;
            large_match.length = match_list[pos].length + match_list[pos].pos;
            large_match.offset = match_list[pos].offset;
            large_match.pos = match_list[pos].pos;
        }
        }
    }
    // Update length
    large_match.length = large_match.length + (large_counter * NB_BYTE);
}

void match_cell_model::processClock()
{
    compareDataHisoty();
    updateSmallCounter();
    findLargeMatch();
    updateLargeCounterAndStatus();

    for (int ii = 0; ii < NB_BYTE; ii++)
    {
        history_reg[ii] = history_reg_next[ii];
    }
}

Matchstruct match_cell_model::getMatch(int pos)
{
    return match_list[pos];
}

Matchstruct match_cell_model::getLargeMatch()
{
    return large_match;
}


void match_detection_model::init()
{
    for (int ii = 0; ii < CHUNKSIZE; ii++)
    {
        matchList[ii].valid = 0;
        matchList[ii].length = 0;
        matchList[ii].offset = 0;
        matchList[ii].pos = 0;
    }
    cycle = 0;

    for (int cell = 0; cell < NB_CELL; cell++)
    {
        match_cell[cell].init();
    }

    match_cell[0].verbose = 0;
}

void match_detection_model::loadData(unsigned char* data)
{
    for (int ii = 0; ii < NB_BYTE; ii++)
    {
        input_string[ii] = data[ii];
    }
}

void match_detection_model::processCycle()
{

    for (int ii = 0; ii < NB_BYTE; ii++)
    {
        current_history[ii].history = input_string[ii];
        current_history[ii].valid = 1;
    }


    for (int cell = 0; cell < NB_CELL; cell++)
    {
        match_cell[cell].loadDataHistory(input_string, (cycle == NB_CELL - 1), current_history, next_history);
        for (int ii = 0; ii < NB_BYTE; ii++) current_history[ii] = next_history[ii];
    }

    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        standard_match[pos].valid = 0;
        standard_match[pos].length = 0;
    }
    large_match.valid = 0;
    large_match.length = 0;

    for (int cell = 0; cell < NB_CELL; cell++)
    {
        match_cell[cell].processClock();

        for (int pos = 0; pos < NB_BYTE; pos++)
        {
        match = match_cell[cell].getMatch(pos);

        if (match.valid)
        {
            //if (verbose) printf("UNFILTERED SMALL  MATCH: pos:%d offset:%d length:%d\n", match.pos + (NB_BYTE * cycle), match.offset + (NB_BYTE * cell), match.length);
        }

        if (match.valid && standard_match[pos].length < match.length)
        {
            standard_match[pos].valid = 1;
            standard_match[pos].offset = match.offset + (NB_BYTE * cell);
            standard_match[pos].length = match.length;
            standard_match[pos].pos = match.pos + (NB_BYTE * cycle);
        }

        }

        match = match_cell[cell].getLargeMatch();

        if (match.valid)
        {
        //if (verbose) printf("UNFILTERED LARGE  MATCH: pos:%d offset:%d length:%d\n", match.pos + (NB_BYTE * cycle), match.offset + (NB_BYTE * cell), match.length);
        }

        if (match.valid && large_match.length < match.length)
        {
        large_match.valid = 1;
        large_match.offset = match.offset + (NB_BYTE * cell);
        large_match.length = match.length;
        large_match.pos = match.pos + (NB_BYTE * cycle);
        }
    }

    // Append Matches to matchlist
    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        if (standard_match[pos].valid)
        {
        if (verbose) printf("SMALL MATCH: pos:%d offset:%d length:%d\n", standard_match[pos].pos, standard_match[pos].offset, standard_match[pos].length);
        matchList[cycle * NB_BYTE + pos] = standard_match[pos];
        }
    }
    if (large_match.valid)
    {
        if (verbose) printf("LARGE  MATCH: pos:%d offset:%d length:%d\n", large_match.pos, large_match.offset, large_match.length);
        matchList[large_match.pos] = large_match;
    }

    cycle++;
    if (cycle > 0 && cycle < 0) {
        printf("CYCLE: %d\n", cycle);
        verbose = 1;
        match_cell[1].verbose = 1;
    }
    else {
        verbose = 0;
        match_cell[1].verbose = 0;
    }

}

Matchstruct* match_detection_model::getStandardMatch()
{
    return standard_match;
}

Matchstruct* match_detection_model::getLargeMatch()
{
    return &large_match;
}

Matchstruct* match_detection_model::getMatchList()
{
    return matchList;
}


void hw_model_compress(std::vector<Match>& matches, const uint64_t& blockSize, const unsigned char* dataBlock)
{
   std::cout << "Using HW Cmodel to Compress" << std::endl;

   for (unsigned int ii = 0; ii < matches.size(); ii++) {
        //printf("MATCH_ORI:\t%d\tD:\t%d\tL:\t%d\n", ii, matches[ii].distance, matches[ii].length);
   }

   // ERASE the Current MATCHLIST
   for (unsigned int ii = 0; ii < matches.size(); ii++) {
      matches[ii].distance = 0;
      matches[ii].length = 0;
   }

   // Prepare data
   unsigned char input_string[CHUNKSIZE];
   for (unsigned int ii = 0; ii < CHUNKSIZE; ii++) {
      if (ii < blockSize) input_string[ii] = dataBlock[ii]; else input_string[ii] = 0;
      //printf("DATA\t%d\t%d\n", ii, input_string[ii]);
   }


   // Match Detection Model
   match_detection_model match_detection;
   match_detection.init();
   for (int ii = 0; ii < NB_CELL; ii++) {
      match_detection.loadData(input_string + ii * NB_BYTE);
      match_detection.processCycle();
   }


   // CONVERT HW matches to SMALL_LZ4 matches
   Matchstruct* matchList;
   matchList = match_detection.getMatchList();
   for (int pos = 0; pos < CHUNKSIZE; pos++) {

      if (matchList[pos].valid) {
         //printf("-----  MATCH: pos:%d offset:%d length:%d\n", matchList[pos].pos, matchList[pos].offset, matchList[pos].length);
         if (((uint64_t)(matchList[pos].pos - matchList[pos].length) < blockSize) && ((matchList[pos].pos - matchList[pos].length) > 0)) {

            if (matchList[pos].pos <= (long int)(blockSize - 6)) {
               matches[matchList[pos].pos - matchList[pos].length].distance = matchList[pos].offset + 1;
               matches[matchList[pos].pos - matchList[pos].length].length = matchList[pos].length;
            }
            else {
               matches[matchList[pos].pos - matchList[pos].length].distance = matchList[pos].offset + 1;
               matches[matchList[pos].pos - matchList[pos].length].length = (blockSize - 5 - (matchList[pos].pos - matchList[pos].length));
            }

         }
      }
   }
   for (unsigned int ii = 1; ii < matches.size(); ii++) {
      if ((matches[ii - 1].length > 4) && (matches[ii].length <= (matches[ii - 1].length - 1))) {
         matches[ii].length = matches[ii - 1].length - 1;
         matches[ii].distance = matches[ii - 1].distance;
      }
   }

   for (unsigned int ii = 0; ii < matches.size(); ii++) {
      //printf("MATCH_NEW:\t%d\tD:\t%d\tL:\t%d\n", ii, matches[ii].distance, matches[ii].length);
   }


}
