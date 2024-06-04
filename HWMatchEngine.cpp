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
    verbose = 0;
    large_counter = 0;
    for (int ii = 0; ii < NB_BYTE; ii++)
    {
        small_counter[ii] = 0;
        large_count_status[ii] = 0;
    }
    for (int ii = 0; ii < NB_BYTE + 1; ii++)
    {
        data[ii] = 0;
        history_reg[ii].valid = 0;
        for (int jj = 0; jj < NB_BYTE; jj++)
        {
            comparator[jj][ii] = 0;
        }
    }
    for (int ii = 0; ii < 2 * NB_BYTE +1 - 1; ii++)
        history[ii].valid = 0;
}

void match_cell_model::loadDataHistory(unsigned char* data_in, bool last_data_in, History* history_in, History* history_out)
{


    for (int ii = NB_BYTE; ii < 2 * NB_BYTE + 1 - 1; ii++)
    {
        history[ii] = history_in[ii - NB_BYTE];
    }
    for (int ii = 0; ii < NB_BYTE + 1; ii++)
    {
        history_reg_next[ii] = history_in[ii];
    }
    for (int ii = 0; ii < NB_BYTE; ii++)
    {
        history[ii] = history_reg[ii];
    }

    for (int ii = 0; ii < NB_BYTE + 1; ii++) { // Need one more byte to do falling edge detection at the comparator level
        data[ii] = data_in[ii];
        history_out[ii] = history_reg[ii];
    }

    last_data = last_data_in;

}


void match_cell_model::compareDataHisoty()
{
    if (verbose) printf("\nCOMPARATORS\n");
    for (int pos = 0; pos < NB_BYTE + 1; pos++)
    {
        for (int offset = 0; offset < NB_BYTE; offset++)
        {
        if (verbose) printf("(%d==%d)", data[pos], history[NB_BYTE - 1 + pos - offset].history);
        if (verbose) printf(".%d", history[NB_BYTE - 1 + pos - offset].valid);
        if ((data[pos] == history[NB_BYTE - 1 + pos - offset].history) && (history[NB_BYTE - 1 + pos - offset].valid) && !((pos == NB_BYTE) && (last_data == 1)))
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

    int merged_length;

    if (verbose) printf("\nSMALL COUNTERS\n");

    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        match_list[pos].valid = 0;
        match_list[pos].length = 0;
        match_list[pos].offset = 0;
        match_list[pos].large_counter = large_counter;

        for (int offset = 0; offset < NB_BYTE; offset++)
        {

            if (comparator[offset][pos])
            {
                if ((large_count_status[offset] != 1 || small_counter[offset] < NB_BYTE) && (small_counter[offset] != 2 * NB_BYTE - 1))
                    small_counter[offset] ++;

                if (comparator[offset][pos + 1] == 0)
                {
                    if (small_counter[offset] > 0) {

                        if ((large_count_status[offset] && (small_counter[offset] < NB_BYTE))) // When Large status was 1 but the cnt has been reset
                        {
                            if ((small_counter[offset] >= 4) && (match_list[pos].length < small_counter[offset]))
                            {
                                match_list[pos].valid = 1;
                                match_list[pos].length = small_counter[offset];
                                match_list[pos].offset = offset;
                                match_list[pos].large_counter = large_counter;
                            }
                        }
                        else
                        {
                            merged_length = (large_count_status[offset] * 2 * NB_BYTE) + small_counter[offset];
                            if ((small_counter[offset] >= 4) && (match_list[pos].length < merged_length))
                            {
                                match_list[pos].valid = 1;
                                match_list[pos].length = merged_length;
                                match_list[pos].offset = offset;
                                match_list[pos].large_counter = large_counter;
                            }
                        }
                    }
                }
            }
            else
            {
                small_counter[offset] = 0;
            }

            if (verbose) printf(" %d.", match_list[pos].valid);
            if (verbose) printf("%d", small_counter[offset]);
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

    if (verbose) printf("LARGE_CNT:%d\n", large_counter);
}



void match_cell_model::processClock()
{
    compareDataHisoty();
    updateSmallCounter();
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


void match_detection_model::init()
{
    for (int ii = 0; ii < CHUNKSIZE; ii++)
    {
        matchList[ii].valid = 0;
        matchList[ii].length = 0;
        matchList[ii].offset = 0;
        matchList[ii].large_counter = 0;

        matchList_startpos[ii].valid = 0;
        matchList_startpos[ii].length = 0;
        matchList_startpos[ii].offset = 0;
        matchList_startpos[ii].large_counter = 0;
    }
    cycle = 0;

    for (int cell = 0; cell < NB_CELL; cell++)
    {
        match_cell[cell].init();
    }
    verbose = 0;
}

void match_detection_model::loadData(unsigned char* data)
{
    for (int ii = 0; ii < NB_BYTE + 1 - (cycle==NB_CELL-1); ii++)
    {
        input_string[ii] = data[ii];
    }
}

void match_detection_model::processCycle()
{
    Matchstruct current_match;
    Matchstruct new_match[NB_BYTE];

    for (int ii = 0; ii < NB_BYTE + 1; ii++)
    {
        current_history[ii].history = input_string[ii];
        current_history[ii].valid = 1;
    }

    for (int cell = 0; cell < NB_CELL; cell++)
    {
        match_cell[cell].loadDataHistory(input_string, (cycle == NB_CELL - 1), current_history, next_history);
        for (int ii = 0; ii < NB_BYTE + 1; ii++) current_history[ii] = next_history[ii];
    }

    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        new_match[pos].valid = 0;
        new_match[pos].offset = 0;
        new_match[pos].length = 0;
        new_match[pos].offset = 0;
        new_match[pos].large_counter = 0;
    }

    int largest_large_counter = 0;

    for (int cell = 0; cell < NB_CELL; cell++)
    {
        match_cell[cell].processClock();
        for (int pos = 0; pos < NB_BYTE; pos++)
        {
            current_match = match_cell[cell].getMatch(pos);
            if (largest_large_counter < current_match.large_counter)
            {
                largest_large_counter = current_match.large_counter;
            }
        }
    }

    bool is_large_match;

    //printf("CYCLE:%d LARGEST LARGE COUNTER:%d\n", cycle, largest_large_counter);

    for (int cell = 0; cell < NB_CELL; cell++)
    {
        for (int pos = 0; pos < NB_BYTE; pos++)
        {
            current_match = match_cell[cell].getMatch(pos);
            if ((cell == 8) && (current_match.valid)) {
                //printf("VCYCLE:%d POS:%d D:%d L:%d XC:%d\n", vcycle, cycle * NB_BYTE + pos, current_match.offset + (NB_BYTE * cell), current_match.length , current_match.large_counter);
            }

            is_large_match = (current_match.length >= 2 * NB_BYTE);
            //if (current_match.valid && (   ((new_match[pos].length < current_match.length) && (is_large_match==0)) || ((new_match[pos].length < current_match.length) && (largest_large_counter == current_match.large_counter) && (is_large_match==1)) ) )
            if (current_match.valid && (largest_large_counter == current_match.large_counter) && (new_match[pos].length < current_match.length))
            {
                new_match[pos].valid = 1;
                new_match[pos].offset = current_match.offset + (NB_BYTE * cell);
                new_match[pos].length = current_match.length;
            }
            new_match[pos].large_counter = largest_large_counter;
        }
    }

    bool vcycle_inc = 0;
    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        if (new_match[pos].valid) {

            if ((vcycle > 47) && (vcycle < 50)) {
                //printf("VCYCLE:%d POS:%d D:%d L:%d XC:%d\n", vcycle, cycle * NB_BYTE + pos, new_match[pos].offset, new_match[pos].length, new_match[pos].large_counter);
            }
            vcycle_inc = 1;
        }
    }
    if (vcycle_inc == 1) vcycle++;



    // Append Matches to matchlist
    int start_pos;
    int resolved_length;

    for (int pos = 0; pos < NB_BYTE; pos++)
    {

        //printf("NEW MATCH: pos:%d offset:%d length:%d lcnt:%d\n", cycle * NB_BYTE + pos, new_match[pos].offset, new_match[pos].length, new_match[pos].large_counter);
        if (new_match[pos].length >= 2 * NB_BYTE) // Large cnt
        {
            //printf("NEW LARGE MATCH: pos:%d epos:%d offset:%d length:%d lcnt:%d\n", pos, new_match[pos].pos, new_match[pos].offset, new_match[pos].length, new_match[pos].large_counter);
            matchList[cycle * NB_BYTE + pos] = new_match[pos];
            resolved_length = (new_match[pos].length - (2 * NB_BYTE) + pos + 1 + new_match[pos].large_counter * NB_BYTE);
            //printf("NEW ULARGE MATCH: pos:%d epos:%d offset:%d length:%d lcnt:%d\n", pos, new_match[pos].pos, new_match[pos].offset, matchList[cycle * NB_BYTE + pos].length, new_match[pos].large_counter);
        }
        else // small cnt
        {
            matchList[cycle * NB_BYTE + pos] = new_match[pos];
            resolved_length = new_match[pos].length;
        }

        if (new_match[pos].valid)
        {
            start_pos = cycle * NB_BYTE + pos + 1 - resolved_length;

            if (start_pos > 0) {
                if (matchList_startpos[start_pos].valid) // Need to check is length is larger
                {
                    if (matchList_startpos[start_pos].length < resolved_length)
                    {
                        matchList_startpos[start_pos] = matchList[cycle * NB_BYTE + pos];
                        matchList_startpos[start_pos].length = resolved_length;
                        matchList_startpos[start_pos].large_counter = 0;
                    }
                }
                else // Nothing here can be appended
                {
                    matchList_startpos[start_pos] = matchList[cycle * NB_BYTE + pos];
                    matchList_startpos[start_pos].length = resolved_length;
                    matchList_startpos[start_pos].large_counter = 0;
                }
            }

        }

    }



    cycle++;
    if (cycle > 0 && cycle < 0) {
        printf("CYCLE: %d\n", cycle);
        verbose = 1;
        match_cell[8].verbose = 1;
    }
    else {
        verbose = 0;
        match_cell[8].verbose = 0;
    }

}

Matchstruct* match_detection_model::getMatchList()
{
    return matchList;
}

Matchstruct* match_detection_model::getMatchListStartPos()
{
    return matchList_startpos;
}


void match_processing_chain_model::init(uint64_t blockSize_in)
{
    for (int ii = 0; ii < CHUNKSIZE; ii++)
    {
        matchList[ii].valid = 0;
        matchList[ii].length = 0;
        matchList[ii].offset = 0;
        matchList[ii].large_counter = 0;

        cc_marked[ii] = 0;
        ii_is_better_than_iip1[ii] = 0;
    }

    blockSize = blockSize_in;
}

void match_processing_chain_model::loadMatchList(Matchstruct* matchList_ptr)
{
    for (int ii = 0; ii < CHUNKSIZE; ii++)
    {
        matchList[ii] = matchList_ptr[ii]; // Copy list
    }
}

void match_processing_chain_model::smalllz4ComplianceFiltering()
{

    Matchstruct matchList_tmp[CHUNKSIZE];

    for (int pos = 0; pos < CHUNKSIZE; pos++) {

        matchList_tmp[pos].valid = 0;
        matchList_tmp[pos].length = 0;
        matchList_tmp[pos].offset = 0;

        if (matchList[pos].valid) {
            //printf("-----  MATCH: pos:%d offset:%d length:%d\n", matchList[pos].pos, matchList[pos].offset, matchList[pos].length);
            if ((pos + matchList[pos].length) < int(blockSize)) {

                if ((pos + matchList[pos].length) <= int(blockSize - 6)) {
                    matchList_tmp[pos].offset = matchList[pos].offset;
                    matchList_tmp[pos].length = matchList[pos].length;
                    matchList_tmp[pos].valid = 1;
                }
                else { // This will be removed eventualy
                    matchList_tmp[pos].offset = matchList[pos].offset;
                    matchList_tmp[pos].length = (blockSize - 5 - pos);
                    matchList_tmp[pos].valid = 1;
                    if (matchList_tmp[pos].length == 0) 
                    {
                        matchList_tmp[pos].offset = 0;
                        matchList_tmp[pos].valid = 0;
                    }
                }

            }
        }
    }

    for (int pos = 0; pos < CHUNKSIZE; pos++)
    {
            matchList[pos] = matchList_tmp[pos]; // Update list
    }

}

void match_processing_chain_model::extend()
{
    // 1ST STEP
    // Extend the matches till a bigger one is found
    for (unsigned int ii = 0; ii < CHUNKSIZE - 1; ii++) {
        if (matchList[ii].length > 1)
        {
            if (matchList[ii].length - 1 >= matchList[ii + 1].length)
            {
                matchList[ii + 1].length = matchList[ii].length - 1;
                matchList[ii + 1].offset = matchList[ii].offset;
                matchList[ii + 1].valid = 1;
            }
        }
    }
}

void match_processing_chain_model::ccBtn()
{
    // 2ND STEP
    // Look for 4 consecutive non-zero distances
    for (unsigned int ii = 0; ii < CHUNKSIZE - 3; ii++) {
        cc_marked[ii] = ((matchList[ii].offset == matchList[ii + 1].offset) && (matchList[ii].offset == matchList[ii + 2].offset) && (matchList[ii].offset == matchList[ii + 3].offset) && (matchList[ii].offset != 0));
    }
    // Precompute (matches[ii].length + 2 >= matches[ii + 1].length)
    for (unsigned int ii = 0; ii < CHUNKSIZE - 1; ii++) {
        ii_is_better_than_iip1[ii] = (matchList[ii].length + 2 >= matchList[ii + 1].length);  // + 1 is also corrrect (need to run the complete Silicia corpus test to see if it is higher than 1.83)
    }
}

void match_processing_chain_model::extendBest()
{
    // 3RD STEP
    // WHen a match starts with a CC mark, it go till the end overwritting the other match (even those starting with CC mark)
    // Otherwise extend only the best matches (if ii+1 is better, then nothing to be done because it is already extended)
    for (unsigned int ii = 0; ii < CHUNKSIZE - 1; ii++) {
        if ((cc_marked[ii]) && (matchList[ii].length > 1))
        {
            matchList[ii + 1].length = matchList[ii].length - 1;
            matchList[ii + 1].offset = matchList[ii].offset;
            cc_marked[ii + 1] = 1;
        }
        else
        {
            if ((matchList[ii].length > 1) && (ii_is_better_than_iip1[ii]))
            {
                matchList[ii + 1].length = matchList[ii].length - 1;
                matchList[ii + 1].offset = matchList[ii].offset;
            }
        }
    }
}

void match_processing_chain_model::recomputeMatchLength()
{
    // FINAL STEP
    // Recompute the length
    int length_cnt = 0;
    for (unsigned int ii = CHUNKSIZE - 1; ii > 1; ii--) {
        if (matchList[ii].offset == matchList[ii - 1].offset)
        {
            length_cnt++;
            if (matchList[ii - 1].offset > 0)
            {
                matchList[ii - 1].length = length_cnt;
                matchList[ii].length = 0;
                matchList[ii].offset = 0;
            }
        }
        else
        {
            if (matchList[ii - 1].offset == 0)
            {
                matchList[ii - 1].length = 0;
                length_cnt = 1;
            }
            else
            {
                matchList[ii - 1].length = 1;
                length_cnt = 1;
            }
        }
    }
}

Matchstruct* match_processing_chain_model::getMatchList()
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

   // Match Processing Chain Model
   match_processing_chain_model match_processing_chain;
   match_processing_chain.init(blockSize);
   match_processing_chain.loadMatchList(match_detection.getMatchListStartPos());
   match_processing_chain.smalllz4ComplianceFiltering();
   match_processing_chain.extend();
   match_processing_chain.ccBtn();
   match_processing_chain.extendBest();
   match_processing_chain.recomputeMatchLength();

   Matchstruct* matchList;
   matchList = match_processing_chain.getMatchList();

   // CONVERT HW matches to SMALL_LZ4 matches
   for (int pos = 0; pos < CHUNKSIZE; pos++) {
       if (matchList[pos].valid) {
           //printf("-----  MATCH: pos:%d offset:%d length:%d\n", pos, matchList[pos].offset, matchList[pos].length);
           matches[pos].distance = matchList[pos].offset + 1;
           matches[pos].length = matchList[pos].length;
       }
   }

   for (unsigned int ii = 0; ii < matches.size(); ii++) {
       //printf("MATCH_NEW:\t%d\tD:\t%d\tL:\t%d\n", ii, matches[ii].distance, matches[ii].length);
   }

}
