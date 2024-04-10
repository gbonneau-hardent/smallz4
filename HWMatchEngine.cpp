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

    int merged_length;

    if (verbose) printf("\nSMALL COUNTERS\n");
    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        match_list[pos].valid = 0;
        match_list[pos].length = 0;
        match_list[pos].offset = 0;

        new_match_list[pos].valid = 0;
        new_match_list[pos].length = 0;
        new_match_list[pos].offset = 0;

        for (int offset = 0; offset < NB_BYTE; offset++)
        {
        if (comparator[offset][pos])
        {
            if ((large_count_status[offset] != 1 || small_counter[offset] < NB_BYTE) && (small_counter[offset] != 2 * NB_BYTE - 1))
                small_counter[offset] ++;
        }
        else
        {
            if (small_counter[offset] > 0) {

                if ((large_count_status[offset] && (small_counter[offset] < NB_BYTE))) // When Large status was 1 but the cnt has been reset
                {
                    if ((small_counter[offset] >= 4) && (new_match_list[pos].length < small_counter[offset]))
                    {
                        new_match_list[pos].valid = 1;
                        new_match_list[pos].length = small_counter[offset];
                        new_match_list[pos].offset = offset;
                        new_match_list[pos].large_counter = large_counter;
                    }
                }
                else
                {
                    merged_length = (large_count_status[offset] * 2 * NB_BYTE) + small_counter[offset];
                    if ((small_counter[offset] >= 4) && (new_match_list[pos].length < merged_length))
                    {
                        new_match_list[pos].valid = 1;
                        new_match_list[pos].length = merged_length;
                        new_match_list[pos].offset = offset;
                        new_match_list[pos].large_counter = large_counter;
                    }
                }
            }

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

Matchstruct match_cell_model::getNewMatch(int pos)
{
    return new_match_list[pos];
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

        matchList_startpos[ii].valid = 0;
        matchList_startpos[ii].length = 0;
        matchList_startpos[ii].offset = 0;
        matchList_startpos[ii].pos = 0;
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
        new_match[pos].valid = 0;
        new_match[pos].length = 0;
        new_match[pos].large_counter = 0;
    }
    large_match.valid = 0;
    large_match.length = 0;

    for (int cell = 0; cell < NB_CELL; cell++)
    {
        match_cell[cell].processClock();

        for (int pos = 0; pos < NB_BYTE; pos++)
        {
            match = match_cell[cell].getNewMatch(pos);
            if (match.valid && ( (new_match[pos].length < match.length) && (new_match[pos].large_counter == match.large_counter) || (new_match[pos].large_counter < match.large_counter)))
            {
                new_match[pos].valid = 1;
                new_match[pos].offset = match.offset + (NB_BYTE * cell);
                new_match[pos].length = match.length;
                new_match[pos].pos = pos + (NB_BYTE * cycle);
                new_match[pos].large_counter = match.large_counter;
            }
        }


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

    int start_pos;
    
    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        if (new_match[pos].valid)
        {
            //printf("NEW MATCH: pos:%d offset:%d length:%d lcnt:%d\n", new_match[pos].pos, new_match[pos].offset, new_match[pos].length, new_match[pos].large_counter);
            if (new_match[pos].length >= 2 * NB_BYTE) // Large cnt
            {
                //printf("NEW LARGE MATCH: pos:%d epos:%d offset:%d length:%d lcnt:%d\n", pos, new_match[pos].pos, new_match[pos].offset, new_match[pos].length, new_match[pos].large_counter);
                matchList[cycle * NB_BYTE + pos] = new_match[pos];
                matchList[cycle * NB_BYTE + pos].length = new_match[pos].length - (2*NB_BYTE) + pos + new_match[pos].large_counter*NB_BYTE;
                //printf("NEW ULARGE MATCH: pos:%d epos:%d offset:%d length:%d lcnt:%d\n", pos, new_match[pos].pos, new_match[pos].offset, matchList[cycle * NB_BYTE + pos].length, new_match[pos].large_counter);
            }
            else // small cnt
            {
                matchList[cycle * NB_BYTE + pos] = new_match[pos];
            }
            
            start_pos = cycle * NB_BYTE + pos - matchList[cycle * NB_BYTE + pos].length;

            if (matchList_startpos[start_pos].valid) // Need to check is length is larger
            {
                if (matchList_startpos[start_pos].length < matchList[cycle * NB_BYTE + pos].length)
                {
                    matchList_startpos[start_pos] = matchList[cycle * NB_BYTE + pos];
                } 
            }
            else // Nothing here can be appended
            {
                matchList_startpos[start_pos] = matchList[cycle * NB_BYTE + pos];
            }

        }
    }
    
    
    for (int pos = 0; pos < NB_BYTE; pos++)
    {
        if (standard_match[pos].valid)
        {
        //printf("OLD SMALL MATCH: pos:%d offset:%d length:%d\n", standard_match[pos].pos, standard_match[pos].offset, standard_match[pos].length);
       // matchList[cycle * NB_BYTE + pos] = standard_match[pos];
        }
    }
    if (large_match.valid)
    {
        //printf("LARGE  MATCH: pos:%d offset:%d length:%d\n", large_match.pos, large_match.offset, large_match.length);
       // matchList[large_match.pos] = large_match;
    }
    

    cycle++;
    if (cycle > 0 && cycle < 0) {
        printf("CYCLE: %d\n", cycle);
        verbose = 1;
        match_cell[102].verbose = 1;
    }
    else {
        verbose = 0;
        match_cell[102].verbose = 0;
    }

}

Matchstruct* match_detection_model::getStandardMatch()
{
    return standard_match;
}

Matchstruct* match_detection_model::getNewMatch()
{
    return new_match;
}

Matchstruct* match_detection_model::getLargeMatch()
{
    return &large_match;
}

Matchstruct* match_detection_model::getMatchList()
{
    return matchList;
}

Matchstruct* match_detection_model::getMatchListStartPos()
{
    return matchList_startpos;
}

void hw_model_compress(std::vector<Match>& matches, const uint64_t& blockSize, const unsigned char* dataBlock)
{
   //std::cout << "Using HW Cmodel to Compress" << std::endl;

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

   /*
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
   */

   
   matchList = match_detection.getMatchListStartPos();
   for (int pos = 0; pos < CHUNKSIZE; pos++) {

       if (matchList[pos].valid) {
           //printf("-----  MATCH: pos:%d offset:%d length:%d\n", matchList[pos].pos, matchList[pos].offset, matchList[pos].length);
           if ((pos + matchList[pos].length) < blockSize) {

               if ( (pos + matchList[pos].length) <= blockSize - 6) {
                   matches[pos].distance = matchList[pos].offset + 1;
                   matches[pos].length = matchList[pos].length;
               }
               else { // This will be removed eventualy
                   matches[pos].distance = matchList[pos].offset + 1;
                   matches[pos].length = (blockSize - 5 - pos);
                   if (matches[pos].length == 0)
                       matches[pos].distance = 0;
               }

           }
       }
   }
   
   for (unsigned int ii = 0; ii < matches.size(); ii++) {
       //printf("MATCH_NEW:\t%d\tD:\t%d\tL:\t%d\n", ii, matches[ii].distance, matches[ii].length);
   }

   int distance[CHUNKSIZE];
   bool mprotected[CHUNKSIZE];
   bool mremovable[CHUNKSIZE];
   int end_of_match;

   // TENTATIVE FIND THE distance between the end of match and next start
   for (unsigned int ii = 0; ii < matches.size(); ii++) { 
       distance[ii] = 4;
       mprotected[ii] = 0;
       mremovable[ii] = 1;
       if (matches[ii].length >0) // Start of the match
       {
           end_of_match = ii + matches[ii].length - 1;
           if (matches[end_of_match + 1].length != 0)
           {
               distance[ii] = 0;
           }
           else if (matches[end_of_match + 2].length != 0)
           {
               distance[ii] = 1;
           }
           else if (matches[end_of_match + 3].length != 0)
           {
               distance[ii] = 2;
           }
           else if (matches[end_of_match + 4].length != 0)
           {
               distance[ii] = 3;
           }
       }
   }

   for (unsigned int ii = 0; ii < matches.size(); ii++) {
       //printf("MATCH_NEW:\t%d\tD:\t%d\tL:\t%d\tN:\t%d\n", ii, matches[ii].distance, matches[ii].length, distance[ii]);
   }

   // RULES: The first match to come after literals is choosen de-facto. (This maybe unefficient in saome case, but will bite the bullet) - ARG!!!! maybe not true
   //        ALways look for the 3 next positions (Still again this can lead to mis-oportunities but this is ok)
   //               - Not efficient to fill a hole of 3 or less if length is smaller than the next match.
   //               - If no holes, Choose the match that will go the farthest of the 3 positions
   // 
   //  IMPORTANT: If the the match can extent beyond 3 position of the current one then it can be chosen
   // 
   // PRE PASS: Remove complet overlap using length and start pos
   // PRE PASS: Find holes of 3 or less
   // PRE PASS: Remove overallping match over two subsecant matches using end pos of 1st match, start pos of 2nd match and length of the overlapping match











   // 1ST STEP
   // Extend the matches till a bigger one is found
   for (unsigned int ii = 0; ii < matches.size()-1; ii++) {
       if (matches[ii].length > 1)
       {
           if (matches[ii].length - 1 >= matches[ii + 1].length)
           {
               matches[ii + 1].length = matches[ii].length - 1;
               matches[ii + 1].distance = matches[ii].distance;
           }
       }
   }

    

   // 2ND STEP
   // Look for 4 consecutive non-zero distances
   bool cc_marked[CHUNKSIZE];
   for (unsigned int ii = 0; ii < matches.size()-3; ii++) {
       cc_marked[ii] = ((matches[ii].distance == matches[ii + 1].distance) && (matches[ii].distance == matches[ii + 2].distance) && (matches[ii].distance == matches[ii + 3].distance) && (matches[ii].distance != 0));
   }
   // Precompute (matches[ii].length + 2 >= matches[ii + 1].length)
   bool ii_is_better_than_iip1[CHUNKSIZE];
   for (unsigned int ii = 0; ii < matches.size(); ii++) {
       ii_is_better_than_iip1[ii] = (matches[ii].length + 2 >= matches[ii + 1].length);  // + 1 is also corrrect (need to run the complete Silicia corpus test to see if it is higher than 1.83)
   }



   // 3RD STEP
   // WHen a match starts with a CC mark, it go till the end overwritting the other match (even those starting with CC mark)
   // Otherwise extend only the best matches (if ii+1 is better, then nothing to be done because it is already extended)
   for (unsigned int ii = 0; ii < matches.size()-1; ii++) {
       if ((cc_marked[ii]) && (matches[ii].length > 1))
       {
            matches[ii + 1].length = matches[ii].length - 1;
            matches[ii + 1].distance = matches[ii].distance;
            cc_marked[ii + 1] = 1;
       }
       else
       {
           if ((matches[ii].length > 1) && (ii_is_better_than_iip1[ii]) )
           {
               matches[ii + 1].length = matches[ii].length - 1;
               matches[ii + 1].distance = matches[ii].distance;
           }
       }
   }










   /*
   int keep_cnt [CHUNKSIZE];

   for (int ii = 0; ii < CHUNKSIZE; ii++)
       keep_cnt[ii] = 1;

   for (unsigned int ii = 0; ii < matches.size(); ii++) {
       if ((matches[ii].length + 1 > matches[ii + 1].length) || (matches[ii].distance == matches[ii + 1].distance))
       {
           keep_cnt[ii + 1] = keep_cnt[ii] + 1;
       } 
       else
       {
           keep_cnt[ii + 1] = 1;
       }

       if (keep_cnt[ii] >= 4)
       {
           if (matches[ii].length > 1)
           {
               matches[ii + 1].length = matches[ii].length - 1;
               matches[ii + 1].distance = matches[ii].distance;
           }
       }
   }
   */

   /*
   bool next0_is_better;
   bool next1_is_better;
   bool next2_is_better;
   bool next3_is_better;

   for (unsigned int ii = 0; ii < matches.size(); ii++) {

       if (matches[ii].length > 0) // There is a match here 
       {
           next0_is_better = 0;
           next1_is_better = 0;
           next2_is_better = 0;
           next3_is_better = 0;

           // Look at the 4 next positions for better match
           if (matches[ii + 1].length > matches[ii].length + 1) // Next position
           {
               next0_is_better = 1;
           }

           if (matches[ii + 2].length > matches[ii + 1].length) // NextNext position
           {
               next1_is_better = 1;
           }

           if (matches[ii + 3].length > matches[ii + 1].length + 1) // NextNextNext position
           {
               next2_is_better = 1;
           }

           if (matches[ii + 4].length > matches[ii + 1].length + 2) // NextNextNext position
           {
               next3_is_better = 1;
           }

           if ((matches[ii + 2].length > 0) && (matches[ii].length < 3) && (matches[ii + 1].length < matches[ii + 2].length + 1)) // NextNextNext position
           {
               next1_is_better = 1;
           }

           if ((matches[ii + 3].length > 0) && (matches[ii].length < 4) && (matches[ii + 1].length < matches[ii + 3].length + 2)) // NextNextNext position
           {
               next2_is_better = 1;
           }

           if ((matches[ii + 4].length > 0) && (matches[ii].length < 5) && (matches[ii + 1].length < matches[ii + 4].length + 3)) // NextNextNext position
           {
               next3_is_better = 1;
           }

           // If only next is better than start a new match else extend the current one
           if (next0_is_better && !next1_is_better && !next2_is_better && !next3_is_better)
           {
               // Do nothing (Keep the match)
           }
           else
           if (matches[ii].length > 1) // Extend current one
           {

               //Look to extend the next too
               if ((matches[ii + 1].length > 4) && (matches[ii + 1].length > matches[ii + 2].length))
               {
                   matches[ii + 2].length = matches[ii + 1].length - 1;
                   matches[ii + 2].distance = matches[ii + 1].distance;
               }

               matches[ii + 1].length = matches[ii].length - 1;
               matches[ii + 1].distance = matches[ii].distance;


           }
       }
   }


   */


   for (unsigned int ii = 0; ii < matches.size(); ii++) {
       //printf("MATCH_NEW:\t%d\tD:\t%d\tL:\t%d\tCC:\t%d\n", ii, matches[ii].distance, matches[ii].length, cc_marked[ii]);
   }

   //printf("\n");


   /*

   bool previous_is_literal = 0;
   int consecutive_match_cnt[CHUNKSIZE];
   for (int ii = 0; ii < CHUNKSIZE; ii++)
       consecutive_match_cnt[ii] = 1;

   for (unsigned int ii = 0; ii < matches.size(); ii++) {
   
       if (matches[ii].length > 0) // There is match here
       {
           if (previous_is_literal) // Take this match de-facto
           {
               if (matches[ii].length > 1) // Unecessary???
               {
                   matches[ii + 1].length = matches[ii].length - 1;
                   matches[ii + 1].distance = matches[ii].distance;
                   consecutive_match_cnt[ii + 1] = consecutive_match_cnt[ii] + 1;
               }
           }
           else // There is a chosen match just before this one
           {
               // Can the current match be terminated ? Need a counter for that
               if (consecutive_match_cnt[ii] > 3) // That means this is at least the 4th copy of the match
               {
                   // Is there another match in the next 3 positions that can be chosen

                   // IF pos+2 or pos+3 are better than next is the best (Can create a literal but that ok)
                   
                   if (The_next_one_is_best) // The next one is the best
                   {

                   }
                   else // The next one is not the best
                   {

                   }

               }
               else
               {
                   if (matches[ii].length > 1)  // Unecessary???
                   {
                       matches[ii + 1].length = matches[ii].length - 1;
                       matches[ii + 1].distance = matches[ii].distance;
                       consecutive_match_cnt[ii + 1] = consecutive_match_cnt[ii] + 1;
                   } 
               }

           }
       }
       else
       {
           previous_is_literal = 1;
       }
   
   }

   */

   /*
   int start_loss;
   int end_loss;

   int consecutive_match_cnt [CHUNKSIZE];

   for (int ii = 0; ii< CHUNKSIZE; ii++)
    consecutive_match_cnt[ii] = 0;


   bool choose_next_valid = 0;

   for (unsigned int ii = 0; ii < matches.size(); ii++) { // FIRST NEW PASS
       
       if (matches[ii].length > 0) // There is a match and it may be extended on the next position
       {
           if ( (consecutive_match_cnt[ii]==0) && (!choose_next_valid) )// This is the real start of a match and can be completly removed (This the first and only one occasion to do so)
           {
               if (matches[ii + 1].length > 0) // There is another match @ pos+1 need to check if it is better 
               {
                   start_loss = 1;
                   end_loss = matches[ii + 1].length + 1 - matches[ii].length;

                   if (distance[ii] < end_loss)
                   {
                       end_loss = distance[ii];
                   }

                   if (start_loss < end_loss) // The new one is better (remove completly the current one)
                   {
                       matches[ii].length = 0;
                       matches[ii].distance = 0;
                       consecutive_match_cnt[ii] = 0;
                       choose_next_valid = 1;
                   }
                   else // The current one is better (extend it)
                   {
                       matches[ii + 1].length = matches[ii].length - 1;
                       matches[ii + 1].distance = matches[ii].distance;
                       distance[ii + 1] = distance[ii];
                       consecutive_match_cnt[ii+1] = 1;
                   }
               }
               else if (matches[ii + 2].length > 0) // There is another match @ pos+2 need to check if it is better 
               {
                   start_loss = 2;
                   end_loss = matches[ii + 2].length + 1 - matches[ii].length;

                   if (distance[ii] < end_loss)
                   {
                       end_loss = distance[ii];
                   }

                   if (start_loss < end_loss) // The new one is better (remove completly the current one)
                   {
                       matches[ii].length = 0;
                       matches[ii].distance = 0;
                       consecutive_match_cnt[ii] = 0;
                       choose_next_valid = 1;
                   }
                   else // The current one is better (extend it)
                   {
                       matches[ii + 1].length = matches[ii].length - 1;
                       matches[ii + 1].distance = matches[ii].distance;
                       distance[ii + 1] = distance[ii];
                       consecutive_match_cnt[ii+1] = 1;
                   }
               }
               else if (matches[ii + 3].length > 0) // There is another match @ pos+3 need to check if it is better 
               {
                   start_loss = 3;
                   end_loss = matches[ii + 3].length + 1 - matches[ii].length;

                   if (distance[ii] < end_loss)
                   {
                       end_loss = distance[ii];
                   }

                   if (start_loss < end_loss) // The new one is better (remove completly the current one)
                   {
                       matches[ii].length = 0;
                       matches[ii].distance = 0;
                       consecutive_match_cnt[ii] = 0;
                       choose_next_valid = 1;
                   }
                   else // The current one is better (extend it)
                   {
                       matches[ii + 1].length = matches[ii].length - 1;
                       matches[ii + 1].distance = matches[ii].distance;
                       distance[ii + 1] = distance[ii];
                       consecutive_match_cnt[ii+1] = 1;
                   }
               }
               else // 
               {
                   matches[ii + 1].length = matches[ii].length - 1;
                   matches[ii + 1].distance = matches[ii].distance;
                   distance[ii + 1] = distance[ii];
                   consecutive_match_cnt[ii+1] = 1;
               }

           }
           else
           {
               choose_next_valid = 0;
               if ((consecutive_match_cnt[ii] < 3) && (matches[ii].length > 1))
               {
                   //Try to move down the other match
                   if ((matches[ii + 1].length > 4) && (matches[ii + 1].length > matches[ii + 2].length))
                   {
                       matches[ii + 2].length = matches[ii + 1].length - 1;
                       matches[ii + 2].distance = matches[ii + 1].distance;
                       distance[ii + 2] = distance[ii + 1];
                   }
                    matches[ii + 1].length = matches[ii].length - 1;
                    matches[ii + 1].distance = matches[ii].distance;
                    distance[ii + 1] = distance[ii];
                    consecutive_match_cnt[ii+1] = consecutive_match_cnt[ii] + 1;
               }
               else // Occasion to change match
               {
                   if (matches[ii + 1].length > 3) // There is another match @ pos+1 need to check if it is better 
                   {
                       if (matches[ii + 1].length > matches[ii].length + 3)
                       {
                           consecutive_match_cnt[ii + 1] = 1;
                       }
                       else if (matches[ii].length > 1)
                       {                           
                           //Try to move down the other match
                           if ((matches[ii + 1].length > 4) && (matches[ii + 1].length > matches[ii + 2].length))
                           {
                               matches[ii + 2].length = matches[ii + 1].length - 1;
                               matches[ii + 2].distance = matches[ii + 1].distance;
                               distance[ii + 2] = distance[ii + 1];
                           }
                           // Replace it
                           matches[ii + 1].length = matches[ii].length - 1;
                           matches[ii + 1].distance = matches[ii].distance;
                           distance[ii + 1] = distance[ii];
                           consecutive_match_cnt[ii + 1] = consecutive_match_cnt[ii] + 1;
                       }
                       
                   }
                   else if (matches[ii].length > 1)
                   {
                       matches[ii + 1].length = matches[ii].length - 1;
                       matches[ii + 1].distance = matches[ii].distance;
                       distance[ii + 1] = distance[ii];
                       consecutive_match_cnt[ii + 1] = consecutive_match_cnt[ii] + 1;
                   }

               }
              
              
           }
       }

   }

   for (unsigned int ii = 0; ii < matches.size(); ii++) {
       //printf("MATCH_NEW:\t%d\tD:\t%d\tL:\t%d\tDSE:\t%d\tCC:\t%d\n", ii, matches[ii].distance, matches[ii].length, distance[ii], consecutive_match_cnt[ii]);
   }
   


   //printf("\n");

   */

   /*
   for (unsigned int ii = 1; ii < matches.size(); ii++) { // FIRST PASS
       if (matches[ii - 1].length > 1) // There is a match and it can be extended on the next posisition
       {
           if ((matches[ii - 1].distance != matches[ii].distance) && (matches[ii].length > 0)) // There is another match on the next position (Collision)
           {

           }
           else // (No Collision)
           {
               matches[ii].length = matches[ii - 1].length - 1;
               matches[ii].distance = matches[ii - 1].distance;
           }
       }
   }
   
   
   int consecutive_match_cnt = 0;

   for (unsigned int ii = 1; ii < matches.size(); ii++) { // SECOND PASS

       //if (ii < 1024) printf("BEFORE MATCH:\t%d\tD:\t%d\tL:\t%d\n", ii, matches[ii].distance, matches[ii].length);
       

       if (matches[ii - 1].length > 0) // There is a match and it may be extended on the next posisition
       {
           if ( (matches[ii - 1].distance != matches[ii].distance) && (matches[ii].length > 0) ) // There is another match on the next position (Collision)
           {
               if ((matches[ii - 1].length > matches[ii].length) && (matches[ii - 1].length > 1)) // The matches overlap completly (The next match can be removed)
               {
                   matches[ii].length   = matches[ii - 1].length - 1;
                   matches[ii].distance = matches[ii - 1].distance;
                   consecutive_match_cnt++;
               }
               if (((matches[ii - 1].length + 3) > matches[ii].length) && (matches[ii - 1].length > 1)) // The next matches can be removed (Only 3 bytes remain after merging) 
               {
                   matches[ii].length = matches[ii - 1].length - 1;
                   matches[ii].distance = matches[ii - 1].distance;
                   consecutive_match_cnt++;
               }
               else if ((consecutive_match_cnt < 3) && (matches[ii - 1].length > 1)) // The current match cannot be terminated without penality
               {             
                   if (matches[ii].length > matches[ii+1].length) // Move back the next match if possible
                   {
                       matches[ii+1].length   = matches[ii].length - 1;
                       matches[ii+1].distance = matches[ii].distance;
                   }
                   matches[ii].length   = matches[ii - 1].length - 1;
                   matches[ii].distance = matches[ii - 1].distance;
                   consecutive_match_cnt++;
               }
               else
               {
                   if ((matches[ii - 1].distance != matches[ii].distance) && (matches[ii].length < 4)) // This is a new match but not long enough to consider
                   {
                       matches[ii].length = 0;
                       matches[ii].distance = 0;
                   }
                   else if ((matches[ii].distance != matches[ii + 1].distance) && (matches[ii].length < matches[ii + 1].length)) // The next match will be better
                   {
                       matches[ii].length = 0;
                       matches[ii].distance = 0;
                   }
                   else if ((matches[ii].distance != matches[ii + 2].distance) && (matches[ii].length < matches[ii + 2].length)) // The second next match will be better
                   {
                       matches[ii].length = 0;
                       matches[ii].distance = 0;
                   }
                   else if ((matches[ii].distance != matches[ii + 3].distance) && (matches[ii].length < matches[ii + 3].length)) // The third next match will be better
                   {
                       matches[ii].length = 0;
                       matches[ii].distance = 0;
                   }
                   consecutive_match_cnt = 0;
               }
           }
           else // (No Collision)
           {
               if (matches[ii - 1].length > 1) // Extend the match
               {
                   matches[ii].length = matches[ii - 1].length - 1;
                   matches[ii].distance = matches[ii - 1].distance;
                   consecutive_match_cnt++;
               }
               else // No more match to extend
               {
                   consecutive_match_cnt = 0;
               }          
           }
       }
       else 
       {
           if ((matches[ii - 1].distance != matches[ii].distance) && (matches[ii].length < 4)) // This is a new match but not long enough to consider
           {
               matches[ii].length = 0;
               matches[ii].distance = 0;
           }
           else if ((matches[ii].distance != matches[ii+1].distance) && (matches[ii].length < matches[ii+1].length)) // The next match will be better
           {
               matches[ii].length = 0;
               matches[ii].distance = 0;
           }
           else if ((matches[ii].distance != matches[ii + 2].distance) && (matches[ii].length < matches[ii + 2].length)) // The second next match will be better
           {
               matches[ii].length = 0;
               matches[ii].distance = 0;
           }
           else if ((matches[ii].distance != matches[ii + 3].distance) && (matches[ii].length < matches[ii + 3].length)) // The third next match will be better
           {
               matches[ii].length = 0;
               matches[ii].distance = 0;
           }
           consecutive_match_cnt = 0;
       }

       //if (ii < 1024) printf("AFTER  MATCH:\t%d\tD:\t%d\tL:\t%d\n\n", ii, matches[ii].distance, matches[ii].length);
      
   }
   */
   
   

   int length_cnt;

   for (unsigned int ii = matches.size() - 1; ii > 1; ii--) {
       if (matches[ii].distance == matches[ii - 1].distance)
       {
           length_cnt++;
           if (matches[ii - 1].distance > 0)
           {
               matches[ii - 1].length = length_cnt;
               matches[ii].length = 0;
               matches[ii].distance = 0;
           }
       }
       else
       {
           if (matches[ii - 1].distance == 0)
           {
               matches[ii - 1].length = 0;
               length_cnt = 1;
           }
           else
           {
               matches[ii - 1].length = 1;
               length_cnt = 1;
           }

       }

   }
   
   
   for (unsigned int ii = 0; ii < matches.size(); ii++) {
       //printf("MATCH_NEW:\t%d\tD:\t%d\tL:\t%d\n", ii, matches[ii].distance, matches[ii].length);
   }
   

   //printf("\n");

}
