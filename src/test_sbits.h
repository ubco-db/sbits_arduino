/******************************************************************************/
/**
@file		test_sbits.c
@author		Ramon Lawrence
@brief		This file does performance/correctness testing of sequential bitmap 
            indexing for time series (SBITS).
@copyright	Copyright 2021
			The University of British Columbia,
            Ramon Lawrence		
@par Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

@par 1.Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.

@par 2.Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

@par 3.Neither the name of the copyright holder nor the names of its contributors
	may be used to endorse or promote products derived from this software without
	specific prior written permission.

@par THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
/******************************************************************************/
#include <time.h>
#include <string.h>

#include "sbits.h"


/* A bitmap with 8 buckets (bits). Range 0 to 100. */
void updateBitmapInt8Bucket(void *data, void *bm)
{
    // Note: Assuming int key is right at the start of the data record
    int32_t val = *((int16_t*) data);
    uint8_t* bmval = (uint8_t*) bm;

    if (val < 10)
        *bmval = *bmval | 128;        
    else if (val < 20)
        *bmval = *bmval | 64;        
    else if (val < 30)
        *bmval = *bmval | 32;        
    else if (val < 40)
        *bmval = *bmval | 16;        
    else if (val < 50)
        *bmval = *bmval | 8;        
    else if (val < 60)
        *bmval = *bmval | 4;            
    else if (val < 100)
        *bmval = *bmval | 2;        
    else 
        *bmval = *bmval | 1;        
}	


/* A bitmap with 8 buckets (bits). Range 0 to 100. Build bitmap based on min and max value. */
void buildBitmapInt8BucketWithRange(void *min, void *max, void *bm)
{
    /* Note: Assuming int key is right at the start of the data record */    
    uint8_t* bmval = (uint8_t*) bm;

    if (min == NULL && max == NULL)
    {
        *bmval = 255;  /* Everything */
    }
    else
    {        
        int8_t i = 0;
        uint8_t val = 128;
        if (min != NULL)
        {
            /* Set bits based on min value */
            updateBitmapInt8Bucket(min, bm);

            /* Assume here that bits are set in increasing order based on smallest value */                        
            /* Find first set bit */
            while ( (val & *bmval) == 0 && i < 8)
            {
                i++;
                val = val / 2;
            }
            val = val / 2;
            i++;
        }
        if (max != NULL)
        {
            /* Set bits based on min value */
            updateBitmapInt8Bucket(max, bm);

            while ( (val & *bmval) == 0 && i < 8)
            {
                i++;
                *bmval = *bmval + val;
                val = val / 2;                 
            }
        }
        else
        {
            while (i < 8)
            {
                i++;
                *bmval = *bmval + val;
                val = val / 2;
            }
        }        
    }        
}	

int8_t inBitmapInt8Bucket(void *data, void *bm)
{    
    uint8_t* bmval = (uint8_t*) bm;

    uint8_t tmpbm = 0;
    updateBitmapInt8Bucket(data, &tmpbm);

    // Return a number great than 1 if there is an overlap
    return tmpbm & *bmval;
}

/* A 16-bit bitmap on a 32-bit int value */
void updateBitmapInt16(void *data, void *bm)
{
    int32_t val = *((int32_t*) data);
    uint16_t* bmval = (uint16_t*) bm;
 
    /* Using a demo range of 0 to 100 */
    int16_t stepSize = 100 / 15;
    int32_t current = stepSize;
    uint16_t num = 32768;
    while (val > current)
    {
        current += stepSize;
        num = num / 2;
    }
    if (num == 0)
        num = 1;        /* Always set last bit if value bigger than largest cutoff */
    *bmval = *bmval | num;            
}	

int8_t inBitmapInt16(void *data, void *bm)
{    
    uint16_t* bmval = (uint16_t*) bm;

    uint16_t tmpbm = 0;
    updateBitmapInt16(data, &tmpbm);

    // Return a number great than 1 if there is an overlap
    return tmpbm & *bmval;
}

/* A 64-bit bitmap on a 32-bit int value */
void updateBitmapInt64(void *data, void *bm)
{
    int16_t val = (int16_t) *((int32_t*) data);    
      
    const int8_t stepSize = 10;    // Temperature data in F. Scaled by 10. */    
    int16_t current = 320;
    const int8_t bmsize = 63;
    uint8_t count = 0;
      
    while (val > current && count < bmsize)
    {
        current += stepSize;
        count++;
    }
    /*
    if (val > current)
    {
        count = (val-current) / stepSize;
        if (count > bmsize)
            count = bmsize;
    }
    */
    uint8_t b = 128;    
    int8_t offset = count >> 3;  // / 8
    b = b >> (count & 7);
    
    *( (char*) ((char*) bm + offset)) =  *( (char*) ((char*)bm + offset)) | b;                 
}	

int8_t inBitmapInt64(void *data, void *bm)
{    
    uint64_t* bmval = (uint64_t*) bm;

    uint64_t tmpbm = 0;
    updateBitmapInt64(data, &tmpbm);

    // Return a number great than 1 if there is an overlap
    return tmpbm & *bmval;
}

int8_t int32Comparator(
        void			*a,
        void			*b
) {
	int32_t result = *((int32_t*)a) - *((int32_t*)b);
	if(result < 0) return -1;
	if(result > 0) return 1;
    return 0;
}


void testIterator(sbitsState *state)
{
    /* Iterator with filter on keys */
    uint32_t i, start, end;
    sbitsIterator it;
    uint32_t mv = 1;     
    it.minKey = &mv;
    uint32_t v = 1299;    
    it.maxKey = NULL;
    uint32_t md = 90;
    it.minData = &md;
    it.maxData = NULL;    

    resetStats(state);

    printf("\nInitializing iterator.\n");

    sbitsInitIterator(state, &it);
    i = 0;
    int8_t success = 1;    
    int32_t *itKey, *itData;

    while (sbitsNext(state, &it, (void**) &itKey, (void**) &itData))
    {                      
        // printf("Key: %d  Data: %d\n", *itKey, *itData);
        i++;        
    }
    printf("Read records: %d\n", i);

    printStats(state);
    
    /* Iterator with filter on data */       
    it.minKey = NULL;    
    it.maxKey = NULL;
    mv = 90;
    v = 100;
    it.minData = &mv;
    it.maxData = &v;    

    start = millis();
    resetStats(state);
    printf("\nInitializing iterator.\n");

    sbitsInitIterator(state, &it);
    i = 0;        

    while (sbitsNext(state, &it, (void**) &itKey, (void**) &itData))
    {                      
        // printf("Key: %d  Data: %d\n", *itKey, *itData);
        if ( *((int32_t*)itData) < *( (int32_t*) it.minData) || *((int32_t*)itData) > *( (int32_t*) it.maxData))
        {   success = 0;
            printf("Key: %d Data: %d Error\n", *itKey, *itData);
        }
        i++;        
    }
    printf("Read records: %d\n", i);   
    printf("Success: %d\n", success);   
    
    printStats(state);
    end = millis();
    printf("Elapsed Time: %lu ms\n", (end - start));   
    printStats(state); 
    
}

/**
 * Runs all tests and collects benchmarks
 */ 
void runalltests_sbits()
{
    printf("\nSTARTING SBITS TESTS.\n");

    int8_t      M = 4;    
    int32_t     numRecords = 10000;
    uint32_t    numSteps = 10, stepSize = numRecords / numSteps;
    count_t     r, numRuns = 3, l;
    uint32_t    times[numSteps][numRuns];
    uint32_t    reads[numSteps][numRuns];
    uint32_t    writes[numSteps][numRuns];
    uint32_t    overwrites[numSteps][numRuns];
    uint32_t    hits[numSteps][numRuns];    
    uint32_t    rtimes[numSteps][numRuns];
    uint32_t    rreads[numSteps][numRuns];
    uint32_t    rhits[numSteps][numRuns];
    int8_t      seqdata = 0;
    SD_FILE     *infile;
    uint32_t    minRange, maxRange;
    char        infileBuffer[512];
    uint32_t    reads2[numSteps][numRuns];
    uint32_t    writes2[numSteps][numRuns];    
    uint32_t    hits2[numSteps][numRuns];        
    uint32_t    rreads2[numSteps][numRuns];
    uint32_t    rhits2[numSteps][numRuns];

    if (seqdata != 1)
    {   /* Open file to read input records */
        /*
        infile = fopen("data/sea100K.bin", "r+b");
        minRange = 1314604380;
        maxRange = 1609487580;        
        numRecords = 100000 / 10;
        */
        
        infile = fopen("data/uwa500K.bin", "r+b");
        minRange = 946713600;
        maxRange = 977144040;
        numRecords = 500000 / 5;
        
        stepSize = numRecords / numSteps;
    }

    for (r=0; r < numRuns; r++)
    {
        /* Configure SBITS state */
        sbitsState* state = (sbitsState*) malloc(sizeof(sbitsState));
        if (state == NULL)
        {
            printf("Unable to allocate state. Exiting.\n");
            return;
        }   

        state->recordSize = 16;
        state->keySize = 4;
        state->dataSize = 12;
        state->pageSize = 512;
        state->bufferSizeInBlocks = M;
        state->buffer  = malloc((size_t) state->bufferSizeInBlocks * state->pageSize); 
        if (state->buffer == NULL)
        {
            printf("Unable to allocate buffer. Exiting.\n");
            return;
        }   
        int8_t* recordBuffer = (int8_t*) malloc(state->recordSize);   
        if (recordBuffer == NULL)
        {
            printf("Unable to allocate record buffer. Exiting.\n");
            return;
        }   

        /* Address level parameters */
        state->startAddress = 0;
        state->endAddress = state->pageSize * numRecords / 10;  /* Modify this value lower to test wrap around */	
        state->eraseSizeInPages = 4;
        state->parameters = SBITS_USE_BMAP | SBITS_USE_INDEX;
        // state->parameters =  0;
        if (SBITS_USING_INDEX(state->parameters) == 1)
            state->endAddress += state->pageSize * (state->eraseSizeInPages *2);    
        if (SBITS_USING_BMAP(state->parameters))
            state->bitmapSize = 8;

        /* Setup for data and bitmap comparison functions */
        /* 16-bit bitmap */
        /*
        state->inBitmap = inBitmapInt16;
        state->updateBitmap = updateBitmapInt16;
        */
        /* 64-bit bitmap */
        state->inBitmap = inBitmapInt64;
        state->updateBitmap = updateBitmapInt64;
        state->compareKey = int32Comparator;
        state->compareData = int32Comparator;
        
        /* Initialize SBITS structure with parameters */
        if (sbitsInit(state) != 0)
        {   
            printf("Initialization error.\n");
            return;
        }        
 
        /* Data record is empty. Only need to reset to 0 once as reusing struct. */    
        int32_t i;
        for (i = 0; i < state->recordSize-4; i++) // 4 is the size of the key
        {
            recordBuffer[i + sizeof(int32_t)] = 0;
        }

        /* Insert records into structure */  
        printf("\n\nINSERT TEST:\n");  
        uint32_t start = millis();

        if (seqdata == 1)
        {
            for (i = 0; i < numRecords; i++)
            {        
                *((int32_t*) recordBuffer) = i;        
                *((int32_t*) (recordBuffer+4)) = (i%100);    
                sbitsPut(state, recordBuffer, (void*) (recordBuffer + 4));  

                if (i % stepSize == 0)
                {           
                    printf("Num: %lu KEY: %lu\n", i, i);                
                    l = i / stepSize -1;
                    if (l < numSteps && l >= 0)
                    {
                        times[l][r] = millis()-start;
                        reads[l][r] = state->numReads;
                        writes[l][r] = state->numWrites;
                        overwrites[l][r] = 0;
                        hits[l][r] = state->bufferHits;                     
                        reads2[l][r] = state->numIdxReads;
                        writes2[l][r] =state->numIdxWrites;                    
                        hits2[l][r] = 0; 
                    }
                }                     
            }    
        }
        else
        {   /* Read data from a file */
            int8_t headerSize = 16;
            i = 0;
            fseek(infile, 0, SEEK_SET);
            while (1)
            {
                /* Read page */
                if (0 == fread(infileBuffer, state->pageSize, 1, infile))
                    break;
                        
                /* Process all records on page */
                int16_t count = *((int16_t*) (infileBuffer+4));                  
                for (int j=0; j < count; j++)
                {	
                    void *buf = (infileBuffer + headerSize + j*state->recordSize);				
                              
                    sbitsPut(state, buf, (void*) ((char*) buf + 4));  
                    // if ( i < 100)
                    //    printf("%lu %d %d %d\n", *((uint32_t*) buf), *((int32_t*) (buf+4)), *((int32_t*) (buf+8)), *((int32_t*) (buf+12)));   

                    if (i % stepSize == 0)
                    {           
                        printf("Num: %lu KEY: %lu\n", i, *((int32_t*) buf));                
                        l = i / stepSize -1;                        
                        if (l < numSteps && l >= 0)
                        {
                            times[l][r] = millis()-start;
                            reads[l][r] = state->numReads;
                            writes[l][r] = state->numWrites;
                            overwrites[l][r] = 0;
                            hits[l][r] = state->bufferHits; 
                            reads2[l][r] = state->numIdxReads;
                            writes2[l][r] =state->numIdxWrites;                    
                            hits2[l][r] = 0;                    
                        }
                    }  
                    i++;      
                    /* Allows stopping at set number of records instead of reading entire file */
                    if (i == numRecords)
                    {   maxRange = *((uint32_t*) buf);
                        goto doneread;              
                    }
                }                 
            }  
            numRecords = i;    
        }

doneread:
        sbitsFlush(state);
        fflush(state->file);
        uint32_t end = millis();

        l = numSteps-1;
        times[l][r] = end-start;
        reads[l][r] = state->numReads;
        writes[l][r] = state->numWrites;
        overwrites[l][r] = 0;
        hits[l][r] = state->bufferHits;
        reads2[l][r] = state->numIdxReads;
        writes2[l][r] =state->numIdxWrites;                    
        hits2[l][r] = 0;    
      
        printf("Elapsed Time: %lu ms\n", (end - start));
        printf("Records inserted: %lu\n", numRecords);

        printStats(state);
        resetStats(state);

        /* Verify that all values can be found and test query performance */    
        printf("\n\nQUERY TEST:\n");
        start = millis();

        if (seqdata == 1)
        {  
            for (i = 0; i < numRecords; i++)          
            { 
                int32_t key = i;        
                int8_t result = sbitsGet(state, &key, recordBuffer);
                
                if (result != 0) 
                    printf("ERROR: Failed to find: %lu\n", key);    
                if (*((int32_t*) recordBuffer) != key%100)
                {   printf("ERROR: Wrong data for: %lu\n", key);
                    printf("Key: %lu Data: %lu\n", key, *((int32_t*) recordBuffer));
                    return;
                }    

                if (i % stepSize == 0)
                {                                                         
                    l = i / stepSize - 1;
                    if (l < numSteps && l >= 0)
                    {
                        rtimes[l][r] = millis()-start;
                        rreads[l][r] = state->numReads;                    
                        rhits[l][r] = state->bufferHits;                                              
                        rreads2[l][r] = state->numIdxReads;                    
                        rhits2[l][r] = 0;                      
                    }
                }     
            }
        }
        else
        {   /* Data from file */            
            int8_t headerSize = 16;
            i = 0;
            int8_t queryType = 3;

            if (queryType == 1)
            {   /* Query each record from original data set. */
                fseek(infile, 0, SEEK_SET);

                while (1)
                {
                    /* Read page */
                    if (0 == fread(infileBuffer, state->pageSize, 1, infile))
                        break;
                            
                    /* Process all records on page */
                    int16_t count = *((int16_t*) (infileBuffer+4));                  
                    for (int j=0; j < count; j++)
                    {	
                        void *buf = (infileBuffer + headerSize + j*state->recordSize);				
                        int32_t* key = (int32_t*)  buf;
                     
                        int8_t result = sbitsGet(state, key, recordBuffer);  
                        if (result != 0) 
                            printf("ERROR: Failed to find: %lu\n", *key);    
                        if (*((int32_t*) recordBuffer) != *((int32_t*) ((char*) buf+4)))
                        {   
                            printf("ERROR: Wrong data for: Key: %lu Data: %lu ", *key, *((int32_t*) recordBuffer));
                            printf("%lu %d %d %d\n", *((uint32_t*) buf), *((int32_t*) ((char*) buf+4)), *((int32_t*) ((char*) buf+8)), *((int32_t*) ((char*) buf+12))); 
                            result = sbitsGet(state, key, recordBuffer);  
                            return;
                        } 

                        if (i % stepSize == 0)
                        {           
                            printf("Num: %lu KEY: %lu\n", i, *key);                
                            l = i / stepSize -1;
                            if (l < numSteps && l >= 0)
                            {
                                rtimes[l][r] = millis()-start;
                                rreads[l][r] = state->numReads;                    
                                rhits[l][r] = state->bufferHits; 
                                rreads2[l][r] = state->numIdxReads;                    
                                rhits2[l][r] = 0;                      
                            }
                        }  
                        i++;  
                        if (i == numRecords)    /* Allows ending test after set number of records rather than processing entire file */
                            goto donetest;
                    }                    
                }  
                donetest:
                    numRecords = i;
            }
            else if (queryType == 2)
            {   /* Query random values in range. May not exist in data set. */
                i = 0;
                int32_t num = maxRange - minRange;                
                int64_t numRec = 10000;
                int32_t stepS = 1000;
                
                while (i < numRec)
                {                    
                    double scaled = ((double)rand()*(double)rand())/RAND_MAX/RAND_MAX;				
                    int32_t key = (num+1)*scaled + minRange;  
                                   
                    // printf("Key :%d\n", key);           
                    sbitsGet(state, &key, recordBuffer);                          

                    if (i % stepS == 0)
                    {                                                         
                        l = i / stepS - 1;
                        printf("Num: %lu KEY: %lu\n", i, key);     
                        if (l < numSteps && l >= 0)
                        {
                            rtimes[l][r] = millis() - start;
                            rreads[l][r] = state->numReads;                    
                            rhits[l][r] = state->bufferHits;
                            rreads2[l][r] = state->numIdxReads;                    
                            rhits2[l][r] = 0;                            
                        }
                    }     
                    i++;  
                }                
            } 
            else 
            {   /* Data value query for given value range */                    
                uint32_t *itKey, *itData;    
                sbitsIterator it;         
                it.minKey = NULL;    
                it.maxKey = NULL;
                int32_t mv = 800;
                int32_t v = 1000;
                it.minData = &mv;
                it.maxData = &v;    
                int32_t rec, reads, idxreads;

                start = millis();
                mv = 290;
            
                i = 0;
                maxRange = 630;
                minRange = 300;
                int32_t num = maxRange - minRange;                
                int64_t numRec = 100;
                int32_t stepS = 3;
               
                if (queryType == 3)
                    numRec = 32;

                while (i < numRec)
                {
                    if (queryType == 3)
                    {
                        mv += 10;
                        v = mv; 
                    }    
                    else
                    {   // Generate random number in range
                        double scaled = ((double)rand()*(double)rand())/RAND_MAX/RAND_MAX;				
                        mv = (num+1)*scaled + minRange;                          
                    }          
                    
                    // resetStats(state);                                                             
                    sbitsInitIterator(state, &it);
                    rec = 0;        
                    reads = state->numReads;
                    idxreads = state->numIdxReads;
                    // printf("Min: %lu Max: %lu idx reads: %lu\n", mv, v, idxreads);
                    while (sbitsNext(state, &it, (void**) &itKey, (void**) &itData))
                    {                      
                        // printf("Key: %lu  Data: %lu\n", *itKey, *itData);
                        if ( *((uint32_t*)itData) < *( (uint32_t*) it.minData) || *((uint32_t*)itData) > *( (uint32_t*) it.maxData))
                        {   
                            printf("Key: %lu Data: %lu Error\n", *itKey, *itData);
                            if (rec >= 5)
                                return;
                        }
                        rec++;        
                    }
                   //  printf("Read records: %d\n", rec);                                           
                    
                    printf("Num: %lu KEY: %lu Perc: %lu Records: %lu Reads: %lu Idx reads: %lu\n", i, mv, ((state->numReads-reads)*1000/(state->nextPageWriteId-1)), rec, (state->numReads-reads), (state->numIdxReads-idxreads));     
                    
                    if (i % stepS == 0)
                    {                                                         
                        l = i / stepS - 1;
                        printf("Num: %lu Idx: %d KEY: %lu Records: %lu Reads: %lu\n", i, l, mv, rec, (state->numReads-reads));     
                        if (l < numSteps && l >= 0)
                        {                            
                            rtimes[l][r] = millis() - start;
                            rreads[l][r] = state->numReads;                    
                            rhits[l][r] = state->bufferHits;
                            rreads2[l][r] = state->numIdxReads;                    
                            rhits2[l][r] = 0;                            
                        }
                    }    
                    i++;  
                }
            } 
        }

        end = millis();
        printf("Elapsed Time: %lu ms\n", (end - start));
        printf("Records queried: %lu\n", i);
        l = numSteps-1;       
        rtimes[l][r] = millis()-start;
        rreads[l][r] = state->numReads;                    
        rhits[l][r] = state->bufferHits;  
        rreads2[l][r] = state->numIdxReads;                    
        rhits2[l][r] = 0;       

        printStats(state); 
            
        // Optional: Test iterator
        // testIterator(state);
        // printStats(state); 
 
        fclose(state->file);
        if (state->indexFile != NULL)
            fclose(state->indexFile);
        free(recordBuffer);        
        free(state->buffer);
        free(state);       
    }
   
    printf("\nComplete\n");

    // Prints results
    uint32_t sum;
    for (count_t i=1; i <= numSteps; i++)
    {
        printf("Stats for %lu:\n", i*stepSize);
    
        printf("Reads:   ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += reads[i-1][r];
            printf("\t%lu", reads[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Index Reads:   ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += reads2[i-1][r];
            printf("\t%lu", reads2[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Writes: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += writes[i-1][r];
            printf("\t%lu", writes[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        
        printf("Index Writes: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += writes2[i-1][r];
            printf("\t%lu", writes2[i-1][r]);
        }
        printf("\t%lu\n", sum/r);      

        printf("Overwrites: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += overwrites[i-1][r];
            printf("\t%lu", overwrites[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        
        printf("Totwrites: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += overwrites[i-1][r] + writes[i-1][r]+writes2[i-1][r];
            printf("\t%lu", overwrites[i-1][r] + writes[i-1][r]+writes2[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Buffer hits: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += hits[i-1][r];
            printf("\t%lu", hits[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        
        printf("Idx buf hits: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += hits2[i-1][r];
            printf("\t%lu", hits2[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Write Time: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += times[i-1][r];
            printf("\t%lu", times[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
        
        printf("R Time: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rtimes[i-1][r];
            printf("\t%lu", rtimes[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("R Reads: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rreads[i-1][r];
            printf("\t%lu", rreads[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Idx R Reads: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rreads2[i-1][r];
            printf("\t%lu", rreads2[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("R Buf hits: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rhits[i-1][r];
            printf("\t%lu", rhits[i-1][r]);
        }
        printf("\t%lu\n", sum/r);

        printf("Idx RBuf hits: ");
        sum = 0;
        for (r=0 ; r < numRuns; r++)
        {
            sum += rhits2[i-1][r];
            printf("\t%lu", rhits2[i-1][r]);
        }
        printf("\t%lu\n", sum/r);
    }
}
