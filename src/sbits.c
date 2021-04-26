/******************************************************************************/
/**
@file		sbits.c
@author		Ramon Lawrence
@brief		This file is for sequential bitmap indexing for time series (SBITS).
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "sbits.h"

/**
 * Use binary search instead of value-based search
 */
// #define USE_BINARY_SEARCH 	1

void printBitmap(char* bm)
{	
	for (int8_t i = 0; i <= 7; i++)
	{
		printf(" "BYTE_TO_BINARY_PATTERN"", BYTE_TO_BINARY(*(bm+i)));
	}
	printf("\n");	
}

int8_t bitmapOverlap(uint8_t* bm1, uint8_t* bm2, int8_t size)
{
	for (int8_t i=0; i < size; i++)
		if ( (*((uint8_t*) (bm1 + i)) & *((uint8_t*) (bm2 + i))) >= 1)
			return 1;

	return 0;
}

void initBufferPage(sbitsState *state, int pageNum)
{
	/* Initialize page */
	uint16_t i = 0;
	void *buf = state->buffer + pageNum * state->pageSize;

	for (i = 0; i < state->pageSize; i++)
    {
        ((int8_t*) buf)[i] = 0;
    }	

	/* Initialize header key min. Max and sum is already set to zero by the for-loop above */
	void *min = SBITS_GET_MIN_KEY(buf, state);
	/* Initialize min to all 1s */
	for (i = 0; i < state->keySize; i++)
    {
        ((int8_t*) min)[i] = 1;
    }		

	/* Initialize data min. */
	min = SBITS_GET_MIN_DATA(buf, state);
	/* Initialize min to all 1s */
	for (i = 0; i < state->dataSize; i++)
    {
        ((int8_t*) min)[i] = 1;
    }	
}

/**
@brief     	Return the smallest key in the node
@param     	state
                SBITS algorithm state structure
@param     	buffer
                In memory page buffer with node data
*/
void* sbitsGetMinKey(sbitsState *state, void *buffer)
{
	return (void*) (buffer+state->headerSize);
}

/**
@brief     	Return the smallest key in the node
@param     	state
                SBITS algorithm state structure
@param     	buffer
                In memory page buffer with node data
*/
void* sbitsGetMaxKey(sbitsState *state, void *buffer)
{
	int16_t count =  SBITS_GET_COUNT(buffer); 	
	return (void*) (buffer+state->headerSize+(count-1)*state->recordSize);
}


/**
@brief     	Initialize SBITS structure.
@param     	state
                SBITS algorithm state structure
@return		Return 0 if success. Non-zero value if error.
*/
int8_t sbitsInit(sbitsState *state)
{
	printf("Initializing SBITS.\n");
	printf("Buffer size: %d  Page size: %d\n", state->bufferSizeInBlocks, state->pageSize);	
	state->recordSize = state->keySize + state->dataSize;
	printf("Record size: %d\n", state->recordSize);
	printf("Use index: %d  Max/min: %d Sum: %d Bmap: %d\n", SBITS_USING_INDEX(state->parameters), SBITS_USING_MAX_MIN(state->parameters),
								SBITS_USING_SUM(state->parameters), SBITS_USING_BMAP(state->parameters));
	
	state->file = NULL;
 	state->indexFile = NULL;
	state->nextPageId = 0;
	state->nextPageWriteId = 0;
	state->wrappedMemory = 0;

	/* Calculate block header size */
	/* Header size fixed: 8 bytes: 4 byte id, 2 for record count, X for bitmap. */	
	state->headerSize = 6 + state->bitmapSize;
	if (SBITS_USING_MAX_MIN(state->parameters))
		state->headerSize += state->keySize*2 + state->dataSize*2;

	state->minKey = 0;
	state->bufferedPageId = -1;
	state->bufferedIndexPageId = -1;

	/* Calculate number of records per page */
	state->maxRecordsPerPage = (state->pageSize - state->headerSize) / state->recordSize;
	printf("Header size: %d  Records per page: %d\n", state->headerSize, state->maxRecordsPerPage);	

	/* Allocate first page of buffer as output page */
	initBufferPage(state, 0);
 
  	resetStats(state);

	id_t numPages = (state->endAddress - state->startAddress) / state->pageSize;

	if (numPages < (SBITS_USING_INDEX(state->parameters)*2+2) * state->eraseSizeInPages)
	{
		printf("ERROR: Number of pages allocated must be at least twice erase block size for SBITS and four times when using indexing. Memory pages: %d\n", numPages);		
		return -1;
	}

	state->startDataPage = 0;
	state->endDataPage = state->endAddress / state->pageSize;	
	state->firstDataPage = 0;
	state->firstDataPageId = 0;
	state->erasedEndPage = 0;	
	state->avgKeyDiff = 1;	

 	/* Setup data file. */    
    state->file = fopen("datafile.bin", "w+b");	
    if (state->file == NULL) 
	{
        printf("Error: Can't open file!\n");
        return -1;
    }   	

	if (SBITS_USING_INDEX(state->parameters))
	{	/* Allocate file and buffer for index */
		if (state->bufferSizeInBlocks < 4)
		{
			printf("ERROR: SBITS using index requires at least 4 page buffers. Defaulting to without index.\n");
			state->parameters -= SBITS_USE_INDEX;
		}
		else
		{
			/* Setup index file. */  
			state->indexFile = fopen("idxfile.bin", "w+b");
			if (state->indexFile == NULL) 
			{
				printf("Error: Can't open index file!\n");
				return -1;
			}

			state->maxIdxRecordsPerPage = (state->pageSize - 16) / sizeof(count_t);		/* 4 for id, 2 for count, 2 unused, 4 for minKey (pageId), 4 for maxKey (pageId) */

			/* Allocate third page of buffer as index output page */
			initBufferPage(state, SBITS_INDEX_WRITE_BUFFER);

			/* Add page id to minimum value spot in page */
			void *buf = state->buffer + state->pageSize*(SBITS_INDEX_WRITE_BUFFER);
			// id_t *ptr = (id_t*) SBITS_GET_MIN_KEY(buf, state);
			id_t* ptr = ((id_t*) (buf + 8));
			*ptr = state->nextPageId;

			state->nextIdxPageId = 0;
			state->nextIdxPageWriteId = 0;

			count_t numIdxPages = numPages / 100;			/* Index overhead is about 1% of data size */
			if (numIdxPages < state->eraseSizeInPages * 2)
				numIdxPages = state->eraseSizeInPages * 2;	/* Minimum index space is two erase blocks */
			else											/* Ensure index space is a multiple of erase block size */
				numIdxPages = ((numIdxPages/state->eraseSizeInPages)+1)*state->eraseSizeInPages;
			
			/* Index pages are at the end of the memory space */
			state->endIdxPage = state->endDataPage;
			state->endDataPage -= numIdxPages;
			state->startIdxPage = state->endDataPage+1;		
			// state->firstIdxPage = state->startIdxPage;
			state->firstIdxPage =0;		/* TODO: Decide how to handle if share memory space. For now, having logical pages start from 0 rather than the physical page id after the data block */
			state->erasedEndIdxPage = 0;
			state->wrappedIdxMemory = 0;
		}
	}
	return 0;
}

/**
@brief     	Puts a given key, data pair into structure.
@param     	state
                SBITS algorithm state structure
@param     	key
                Key for record
@param     	data
                Data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t sbitsPut(sbitsState *state, void* key, void *data)
{
	/* Copy record into block */
	count_t count =  SBITS_GET_COUNT(state->buffer); 

	/* Write current page if full */
	if (count >= state->maxRecordsPerPage)
	{
		id_t pageNum = writePage(state, state->buffer);
		/*
		printf("PN: %d PP: %d MK: %d MK: %d MD: %d MD: %d",  pageNum, state->nextPageWriteId-1, *((int32_t*) SBITS_GET_MIN_KEY(state->buffer, state)),
							*((int32_t*) SBITS_GET_MAX_KEY(state->buffer, state)),
							*((int32_t*) SBITS_GET_MIN_DATA(state->buffer, state)),
							*((int32_t*) SBITS_GET_MAX_DATA(state->buffer, state))
							);
		
		char *bm = SBITS_GET_BITMAP(state->buffer);
		printBitmap(bm);
		*/
		/* Save record in index file */
		if (state->indexFile != NULL)
		{
			void *buf = state->buffer + state->pageSize*(SBITS_INDEX_WRITE_BUFFER);
			count_t idxcount =  SBITS_GET_COUNT(buf); 
			if (idxcount >= state->maxIdxRecordsPerPage)
			{	/* Save index page */ 
				writeIndexPage(state, buf);
				
				idxcount = 0;
				initBufferPage(state, SBITS_INDEX_WRITE_BUFFER);

				/* Add page id to minimum value spot in page */
				id_t *ptr = (id_t*) SBITS_GET_MIN_KEY(buf, state);
				*ptr = pageNum;				
			}
						
			SBITS_INC_COUNT(buf);

			/* Copy record onto index page */
			count_t *bm = SBITS_GET_BITMAP(state->buffer);
			memcpy( (void*) (buf + SBITS_IDX_HEADER_SIZE + sizeof(count_t) * idxcount), bm, sizeof(count_t));			
		}

		/* Update estimate of average key difference. */
		int32_t numBlocks = state->nextPageWriteId-1;		
		if (state->nextPageWriteId < state->firstDataPage)
		{	/* Wrapped around in memory and first data page is after the next page that will write */
			numBlocks = state->endDataPage-state->firstDataPage+1+state->nextIdxPageWriteId;
		}
		if (numBlocks == 0)
			numBlocks = 1;

		// #ifndef USE_BINARY_SEARCH
		state->avgKeyDiff = ( *((int32_t*) sbitsGetMaxKey(state, state->buffer)) - state->minKey) / numBlocks / (state->maxRecordsPerPage-1); 
		// printf("Avg key diff: %lu\n", state->avgKeyDiff);
		// #endif

		count = 0;
		initBufferPage(state, 0);		
	}

	/* Copy record onto page */
	memcpy(state->buffer + state->recordSize * count + state->headerSize, key, state->keySize);
	memcpy(state->buffer + state->recordSize * count + state->headerSize + state->keySize, data, state->dataSize);

	/* Update count */
	SBITS_INC_COUNT(state->buffer);	

	if (SBITS_USING_MAX_MIN(state->parameters))
	{	/* Update MIN/MAX */
		void *ptr;
		if (count != 0)
		{		
			/* Since keys are inserted in ascending order, every insert will update max. Min will never change after first record. */	
			ptr = SBITS_GET_MAX_KEY(state->buffer, state);			
			memcpy(ptr, key, state->keySize);
			
			ptr = SBITS_GET_MIN_DATA(state->buffer, state);
			if (state->compareData(data,ptr) < 0)
				memcpy(ptr, data, state->dataSize);
			ptr = SBITS_GET_MAX_DATA(state->buffer, state);
			if (state->compareData(data,ptr) > 0)
				memcpy(ptr, data, state->dataSize);
		}
		else
		{	/* First record inserted */
			ptr = SBITS_GET_MIN_KEY(state->buffer, state);
			memcpy(ptr, key, state->keySize);
			ptr = SBITS_GET_MAX_KEY(state->buffer, state);
			memcpy(ptr, key, state->keySize);

			ptr = SBITS_GET_MIN_DATA(state->buffer, state);
			memcpy(ptr, data, state->dataSize);
			ptr = SBITS_GET_MAX_DATA(state->buffer, state);
			memcpy(ptr, data, state->dataSize);
		}		
	}

	if (SBITS_USING_BMAP(state->parameters))
	{	/* Update bitmap */		
		void *bm = SBITS_GET_BITMAP(state->buffer);
		state->updateBitmap(data, bm);
	}
	
	return 0;	
}

/**
@brief     	Given a key, searches the node for the key.
			If interior node, returns child record number containing next page id to follow.
			If leaf node, returns if of first record with that key or (<= key).
			Returns -1 if key is not found.			
@param     	state
                SBITS algorithm state structure
@param     	buffer
                Pointer to in-memory buffer holding node
@param     	key
                Key for record
@param		pageId
				Page if for page being searched
@param		range
				1 if range query so return pointer to first record <= key, 0 if exact query so much return first exact match record
*/
id_t sbitsSearchNode(sbitsState *state, void *buffer, void* key, id_t pageId, int8_t range)
{
	int16_t first, last, middle, count;
	int8_t compare;
	void *mkey;
	
	count = SBITS_GET_COUNT(buffer);  	

	first = 0;	
	last =  count - 1;
	middle = (first+last)/2;	

	while (first <= last) 
	{			
		mkey = buffer+state->headerSize+state->recordSize*middle;
		compare = state->compareKey(mkey, key);
		if (compare < 0)
			first = middle + 1;
		else if (compare == 0) 
			return middle;							
		else
			last = middle - 1;

		middle = (first + last)/2;
	}
	if (range)
		return middle;
	return -1;
}


/**
@brief     	Given a key, returns data associated with key.
			Note: Space for data must be already allocated.
			Data is copied from database into data buffer.
@param     	state
                SBITS algorithm state structure
@param     	key
                Key for record
@param     	data
                Pre-allocated memory to copy data for record
@return		Return 0 if success. Non-zero value if error.
*/
int8_t sbitsGet(sbitsState *state, void* key, void *data)
{
	int32_t pageId;
	int32_t first=0, last;
	void *buf;
	int16_t numReads = 0;
	

	/* Determine last page */
 	buf = state->buffer + state->pageSize;
	if (state->nextPageWriteId < state->firstDataPage)
	{	/* Wrapped around in memory and first data page is after the next page that will write */
		last = state->endDataPage-state->firstDataPage+1+state->nextIdxPageWriteId;
	}
	else
	{
		last = state->nextPageWriteId-1;
	}

	#ifndef USE_BINARY_SEARCH
	/* Perform a modified binary search that uses info on key location in sequence for first placement. */
	if (state->compareKey(key, (void*) &(state->minKey)) < 0)
		pageId = 0;
	else
	{
	 	pageId = (*((int32_t*)key) - state->minKey) / (state->maxRecordsPerPage * state->avgKeyDiff);

		if (pageId > state->endDataPage || (state->wrappedMemory == 0 && pageId >= state->nextPageWriteId))
			pageId = state->nextPageWriteId-1;		/* Logical page would be beyond maximum. Set to last page. */
	}		
	int32_t offset = 0;
	
	while (1)
	{
		/* Move logical page number to physical page id based on location of first data page */
		int32_t physPageId = pageId + state->firstDataPage;
		if (physPageId >= state->endDataPage)
			physPageId = physPageId - state->endDataPage;

		// printf("Page id: %d  Offset: %d\n", pageId, offset);
		/* Read page into buffer */
		if (readPage(state, physPageId) != 0)
			return -1;
		numReads++;

		if (first >= last)
			break;

		if (state->compareKey(key,sbitsGetMinKey(state,buf)) < 0)
		{	/* Key is less than smallest record in block. */
			last = pageId - 1;	
			offset = (*((int32_t*) key) - *((int32_t*) sbitsGetMinKey(state,buf))) / state->maxRecordsPerPage / ((int32_t) state->avgKeyDiff) - 1;					
			if (pageId + offset < first)
				offset = first-pageId;
			pageId += offset;
			
		}
		else if (state->compareKey(key,sbitsGetMaxKey(state,buf)) > 0)
		{	/* Key is larger than largest record in block. */
			first = pageId + 1;
			offset = (*((int32_t*) key) - *((int32_t*) sbitsGetMaxKey(state,buf))) / (state->maxRecordsPerPage * state->avgKeyDiff) + 1;	
			if (pageId + offset > last)
				offset = last-pageId;
			pageId += offset;
		}
		else
		{	/* Found correct block */
			break;
		}
	}
	#else
	/* Regular binary search */
	pageId = (first+last)/2;
	while (1)
	{
		/* Move logical page number to physical page id based on location of first data page */
		int32_t physPageId = pageId + state->firstDataPage;
		if (physPageId >= state->endDataPage)
			physPageId = physPageId - state->endDataPage;

		/* Read page into buffer */
		if (readPage(state, physPageId) != 0)
			return -1;
		numReads++;

		if (first >= last)
			break;

		if (state->compareKey(key,sbitsGetMinKey(state,buf)) < 0)
		{	/* Key is less than smallest record in block. */
			last = pageId - 1;	
			pageId = (first+last)/2;						
		}
		else if (state->compareKey(key,sbitsGetMaxKey(state,buf)) > 0)
		{	/* Key is larger than largest record in block. */
			first = pageId + 1;
			pageId = (first+last)/2;			
		}
		else
		{	/* Found correct block */
			break;
		}
	}
	#endif		
		 
	id_t nextId = sbitsSearchNode(state, buf, key, nextId, 0);
	if (nextId != -1)
	{	/* Key found */
		memcpy(data, (void*) (buf+state->headerSize+state->recordSize*nextId+state->keySize), state->dataSize);
		return 0;
	}
	return -1;
}


/**
@brief     	Initialize iterator on sbits structure.
@param     	state
                SBITS algorithm state structure
@param     	it
            	SBITS iterator state structure
*/
void sbitsInitIterator(sbitsState *state, sbitsIterator *it)
{
	/* Build query bitmap (if used) */
	it->queryBitmap = NULL;
	it->lastIdxIterRec = 20000;		/* Flag to indicate that not using index */	
	if (SBITS_USING_BMAP(state->parameters))
	{
		/* Verify that bitmap index is useful (must have set either min or max data value) */
		if (it->minData != NULL || it->maxData != NULL)		
		{	/*
			uint16_t *bm = malloc(sizeof(uint16_t));		
			*bm = 0;
			buildBitmapInt16FromRange(state, it->minData, it->maxData, bm);
			*/
			uint64_t *bm = malloc(sizeof(uint64_t));		
			*bm = 0;
			buildBitmapInt64FromRange(state, it->minData, it->maxData, bm);
			
			// printBitmap((char*) bm);			
			it->queryBitmap = bm;

			/* Setup for reading index file */
			if (state->indexFile != NULL)
			{
				it->lastIdxIterPage = state->firstIdxPage;
				it->lastIdxIterRec = 10000;	/* Force to read next index page */	
				it->wrappedIdxMemory = 0;				
			}			
		}
	}

	/* Read first page into memory */
	it->lastIterPage = state->firstDataPage-1;
	it->lastIterRec = 10000;	/* Force to read next page */	
	it->wrappedMemory = 0;
}

/**
@brief     	Flushes output buffer.
@param     	state
                SBITS algorithm state structure
*/
int8_t sbitsFlush(sbitsState *state)
{
	writePage(state, state->buffer);	

	if (state->indexFile != NULL)
	{
		void *buf = state->buffer + state->pageSize*(SBITS_INDEX_WRITE_BUFFER);	
		count_t idxcount = SBITS_GET_COUNT(buf);
		SBITS_INC_COUNT(buf);
			
		/* Copy record onto index page */
		count_t *bm = SBITS_GET_BITMAP(state->buffer);
		memcpy( (void*) (buf + SBITS_IDX_HEADER_SIZE + sizeof(count_t) * idxcount), bm, sizeof(count_t));			
		
		/* TODO: Handle case that index buffer is full */				
		writeIndexPage(state, buf);

		/* Reinitialize buffer */
		initBufferPage(state, SBITS_INDEX_WRITE_BUFFER);
	}

	/* Reinitialize buffer */
	initBufferPage(state, 0);
	return 0;
}

/**
@brief     	Return next key, data pair for iterator.
@param     	state
                SBITS algorithm state structure
@param     	it
            	SBITS iterator state structure
@param     	key
                Key for record
@param     	data
                Data for record
*/
int8_t sbitsNext(sbitsState *state, sbitsIterator *it, void **key, void **data)
{	
	void *buf = state->buffer+state->pageSize;
	/* Iterate until find a record that matches search criteria */
	while (1)
	{	
		if (it->lastIterRec >= SBITS_GET_COUNT(buf))
		{	/* Read next page */			
			it->lastIterRec = 0;

			while (1)
			{
				id_t readPageId = 0;

				if (it->lastIdxIterRec == 20000)
				{	/* No index. Scan next data page by iterator. */
					it->lastIterPage++;
					if (it->lastIterPage >= state->endDataPage)
					{	it->lastIterPage = 0;  /* Wrap around to start of memory */
						it->wrappedMemory = 1;
					}

					if (state->wrappedMemory == 0 || it->wrappedMemory == 1)
					{
						if (it->lastIterPage >= state->nextPageWriteId)
							return 0;		/* No more pages to read */	
					}
					readPageId = it->lastIterPage;														
				}
				else
				{	/* Using index file. */
					void* idxbuf = state->buffer+state->pageSize*SBITS_INDEX_READ_BUFFER;
					count_t cnt = SBITS_GET_COUNT(idxbuf);
					if (it->lastIdxIterRec == 10000 || it->lastIdxIterRec >= cnt)
					{	/* Read next index block. Special case for first block as will not be read into buffer (so count not accurate). */						
						if (it->lastIdxIterPage >= (state->endIdxPage - state->startIdxPage +1))
						{								
							it->wrappedIdxMemory = 1;
							it->lastIdxIterPage = 0;	/* Wrapped around */							
						}
						if (state->wrappedIdxMemory == 0 || it->wrappedIdxMemory == 1)
						{							
							if (it->lastIdxIterPage >= state->nextIdxPageWriteId)
								return 0;		/* No more pages to read */	
						}
						
						if (readIndexPage(state, it->lastIdxIterPage) != 0)
							return 0;	

						it->lastIdxIterPage++;	
						it->lastIdxIterRec = 0;
						cnt = SBITS_GET_COUNT(idxbuf);						
						id_t* id = ((id_t*) (idxbuf + 8));	/* Get min page # for this index page */

						/* Index page may have entries that are earlier than first active data page. Advance iterator beyond them. */
						it->lastIterPage = *id;	
						if (state->firstDataPageId > *id)				
							it->lastIdxIterRec += (state->firstDataPageId - *id);						
						if (it->lastIdxIterRec >= cnt)
						{	/* Jump ahead pages in the index */ /* TODO: Could improve this so do not read first page if know it will not be useful */
							it->lastIdxIterPage += it->lastIdxIterRec / state->maxIdxRecordsPerPage -1;  // -1 as already performed increment
							// printf("Jumping ahead pages to: %d\n", it->lastIdxIterPage);
						}
					}
				
					/* Check bitmaps in current index page until find a match */																			
					while (it->lastIdxIterRec < cnt)
					{			
						count_t *bm = idxbuf+SBITS_IDX_HEADER_SIZE+it->lastIdxIterRec*sizeof(count_t);	
							
						if ( ( *((count_t*) it->queryBitmap) & *bm) >= 1)
						{	readPageId = (it->lastIterPage+it->lastIdxIterRec) % (state->endDataPage - state->startDataPage);							
							it->lastIdxIterRec++;
							goto readPage;
						}
						it->lastIdxIterRec++;
					}		
					continue; /* Read next index block */			
				}
readPage:				
				// printf("Read page: %lu\n", readPageId);			
				if (readPage(state, readPageId) != 0)
					return 0;		

				/* Check bitmap overlap if present */
				if (it->queryBitmap == NULL || !SBITS_USING_BMAP(state->parameters))
					break;

				/* Check bitmap */
				count_t *bm = SBITS_GET_BITMAP(buf);							
				if ( ( *((count_t*) it->queryBitmap) & *bm) >= 1)
				{	/* Overlap in bitmap - will process this page */										
					break;
				}											
			}
		}
		
		/* Get record */	
		*key = buf+state->headerSize+it->lastIterRec*state->recordSize;
		*data = buf+state->headerSize+it->lastIterRec*state->recordSize+state->keySize;
		it->lastIterRec++;

		/* Check that record meets filter constraints */
		if (it->minKey != NULL && state->compareKey(*key, it->minKey) < 0)
			continue;
		if (it->maxKey != NULL && state->compareKey(*key, it->maxKey) > 0)
			return 0;
		if (it->minData != NULL && state->compareData(*data, it->minData) < 0)
			continue;
		if (it->maxData != NULL && state->compareData(*data, it->maxData) > 0)
			continue;
		return 1;
	}
}


/**
@brief     	Prints statistics.
@param     	state
                SBITS state structure
*/
void printStats(sbitsState *state)
{
	printf("Num reads: %d\n", state->numReads);
	printf("Buffer hits: %d\n", state->bufferHits);
	printf("Num writes: %d\n", state->numWrites);
	printf("Num index reads: %d\n", state->numIdxReads);	
	printf("Num index writes: %d\n", state->numIdxWrites);
}

/**
@brief     	Writes page in buffer to storage. Returns page number.
@param     	state
                SBITS algorithm state structure
@param		pageNum
				Page number to read
@return		Return page number if success, -1 if error.
*/
id_t writePage(sbitsState *state, void *buffer)
{    
	if (state->file == NULL)
		return -1;

	/* Always writes to next page number. Returned to user. */	
	id_t pageNum = state->nextPageId++;

	/* Setup page number in header */	
	memcpy(buffer, &(pageNum), sizeof(id_t));			

	if (state->nextPageWriteId >= state->erasedEndPage && state->nextPageWriteId+state->eraseSizeInPages < state->endDataPage)
	{		
		if (state->erasedEndPage != 0)
			state->erasedEndPage += state->eraseSizeInPages;
		else	
			state->erasedEndPage += state->eraseSizeInPages-1;	/* Special case for start of file and page 0 */

		if (state->wrappedMemory != 0) // pageNum > state->nextPageWriteId)
		{	/* Have went through memory at least once. Whatever is erased is actual data that is no longer available. */
			state->firstDataPage  = state->erasedEndPage+1;
			state->firstDataPageId += state->eraseSizeInPages;
			/* Estimate the smallest key now. Could determine exactly by reading this page */
			state->minKey += state->eraseSizeInPages * state->avgKeyDiff * state->maxRecordsPerPage;
		}
		// printf("Erasing pages. Start: %d  End: %d First data page: %d\n", state->erasedEndPage-state->eraseSizeInPages+1, state->erasedEndPage, state->firstDataPage);
	}

	if (state->nextPageWriteId >= state->endDataPage)
	{	// printf("Exhausted data pages: %d.\n", state->nextPageWriteId);

		/* Data storage is full. Reclaim space. */		
		/* Perform erase */
		// printf("Erasing pages. Start: %d  End: %d\n", state->startDataPage, state->startDataPage+state->eraseSizeInPages);

		/* Update the start of the data */
		state->firstDataPageId += state->eraseSizeInPages;	/* TODO: How to determine this without reading? */		
		state->erasedEndPage = state->startDataPage+state->eraseSizeInPages-1;
		state->firstDataPage = state->erasedEndPage+1;	/* First active physical data page is just after what was erased */
		state->wrappedMemory = 1;

		/* Wrap to start of memory space */
		state->nextPageWriteId = state->startDataPage;

		/* Estimate the smallest key now. Could determine exactly by reading this page */
		state->minKey += state->eraseSizeInPages * state->avgKeyDiff * state->maxRecordsPerPage;
		// printf("Estimated min key: %d\n", state->minKey);
		// printf("Erasing pages. Start: %d  End: %d First data page: %d\n", state->erasedEndPage-state->eraseSizeInPages+1, state->erasedEndPage, state->firstDataPage);
	}

	/* Seek to page location in file */
    fseek(state->file, state->nextPageWriteId*state->pageSize, SEEK_SET);	
	fwrite(buffer, state->pageSize, 1, state->file);
	
	state->nextPageWriteId++;
	state->numWrites++;

	return pageNum;
}

/**
@brief     	Writes index page in buffer to storage. Returns page number.
@param     	state
                SBITS algorithm state structure
@param		pageNum
				Page number to read
@return		Return page number if success, -1 if error.
*/
id_t writeIndexPage(sbitsState *state, void *buffer)
{    
	if (state->indexFile == NULL)
		return -1;

	/* Always writes to next page number. Returned to user. */	
	id_t pageNum = state->nextIdxPageId++;
	// printf("\nWrite page: %d Key: %d\n", pageNum, *((int32_t*) (buffer+6)));

	/* Setup page number in header */	
	memcpy(buffer, &(pageNum), sizeof(id_t));	
		
	if (state->nextIdxPageWriteId >= state->erasedEndIdxPage && state->nextIdxPageWriteId+state->eraseSizeInPages < state->endIdxPage-state->startIdxPage+1)
	{		
		if (state->erasedEndIdxPage != 0)
			state->erasedEndIdxPage += state->eraseSizeInPages;
		else	
			state->erasedEndIdxPage += state->eraseSizeInPages-1;	/* Special case for start of file and page 0 */

		if (state->wrappedIdxMemory != 0) // pageNum > state->nextPageWriteId)
		{	/* Have went through memory at least once. Whatever is erased is actual data that is no longer available. */
			state->firstIdxPage  = state->erasedEndIdxPage+1;		
		}
		// printf("Erasing pages. Start: %d  End: %d FirstIdx page: %d\n", state->erasedEndIdxPage-state->eraseSizeInPages+1, state->erasedEndIdxPage, state->firstIdxPage);
	}

	if (state->nextIdxPageWriteId >= state->endIdxPage-state->startIdxPage+1)
	{	printf("Exhausted index pages: %d.\n", state->nextIdxPageWriteId);

		/* Index storage is full. Reclaim space. */
		
		/* Perform erase */
		// printf("Erasing pages. Start: %d  End: %d\n", state->startDataPage, state->startDataPage+state->eraseSizeInPages);
		/* Update the start of the data */		
		state->erasedEndIdxPage = 0+state->eraseSizeInPages-1;
		state->firstIdxPage = state->erasedEndIdxPage+1;	/* First active physical data page is just after what was erased */
		state->wrappedIdxMemory = 1;

		/* Wrap to start of memory space */
		state->nextIdxPageWriteId = 0; // state->startIdxPage;
		
		// printf("Erasing pages. Start: %d  End: %d First index page: %d\n", state->erasedEndIdxPage-state->eraseSizeInPages+1, state->erasedEndIdxPage, state->firstIdxPage);
	}


	/* Seek to page location in file */
    fseek(state->indexFile, state->nextIdxPageWriteId*state->pageSize, SEEK_SET);	
	fwrite(buffer, state->pageSize, 1, state->indexFile);
	
	state->nextIdxPageWriteId++;
	state->numIdxWrites++;

	return pageNum;
}

/**
@brief     	Reads given page from storage.
@param     	state
                SBITS algorithm state structure
@param		pageNum
				Page number to read
@return		Return 0 if success, -1 if error.
*/
int8_t readPage(sbitsState *state, id_t pageNum)
{   
	/* Check if page is currently in buffer */ 
	if (pageNum == state->bufferedPageId)
	{
		state->bufferHits++;
		return 0;
	}

	/* Page is not in buffer. Read from storage. */
    SD_FILE* fp = state->file;
    void *buf = state->buffer + state->pageSize;

    /* Seek to page location in file */
    fseek(fp, pageNum*state->pageSize, SEEK_SET);

    /* Read page into start of buffer 1 */   
    if (0 ==  fread(buf, state->pageSize, 1, fp))
    	return 1;       
    

    state->numReads++;
	state->bufferedPageId = pageNum;    
	return 0;
}

/**
@brief     	Reads given index page from storage.
@param     	state
                SBITS algorithm state structure
@param		pageNum
				Page number to read
@return		Return 0 if success, -1 if error.
*/
int8_t readIndexPage(sbitsState *state, id_t pageNum)
{   
	/* Check if page is currently in buffer */ 	
	if (pageNum == state->bufferedIndexPageId)
	{
		state->bufferHits++;
		return 0;
	}
	
	/* Page is not in buffer. Read from storage. */
    SD_FILE* fp = state->indexFile;
    void *buf = state->buffer + state->pageSize*SBITS_INDEX_READ_BUFFER;

    /* Seek to page location in file */
    fseek(fp, pageNum*state->pageSize, SEEK_SET);

    /* Read page into start of buffer */   
    if (0 ==  fread(buf, state->pageSize, 1, fp))
    	return 1;       
    

    state->numIdxReads++;
	state->bufferedIndexPageId = pageNum;    
	return 0;
}

/**
@brief     	Resets statistics.
@param     	state
                SBITS state structure
*/
void resetStats(sbitsState *state)
{
	state->numReads = 0;
	state->numWrites = 0;
	state->bufferHits = 0;  
	state->numIdxReads = 0;
	state->numIdxWrites = 0;
}

/**
@brief     	Builds 16-bit bitmap from (min, max) range.
@param     	state
                SBITS state structure
@param		min
				minimum value (may be NULL)
@param		max
				maximum value (may be NULL)
@param		bm
				bitmap created
*/
void buildBitmapInt16FromRange(sbitsState *state, void *min, void *max, void *bm)
{
    uint16_t* bmval = (uint16_t*) bm;

    if (min == NULL && max == NULL)
    {
        *bmval = 65535;  /* Everything */
        return;
    }
        
    int8_t i = 0;
    uint16_t val = 32768;
    if (min != NULL)
    {
        /* Set bits based on min value */
        state->updateBitmap(min, bm);
		
        /* Assume here that bits are set in increasing order based on smallest value */                        
        /* Find first set bit */
        while ( (val & *bmval) == 0 && i < 16)
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
        state->updateBitmap(max, bm);

        while ( (val & *bmval) == 0 && i < 16)
        {
            i++;
            *bmval = *bmval + val;
            val = val / 2;                 
        }
    }
    else
    {
        while (i < 16)
        {
            i++;
            *bmval = *bmval + val;
            val = val / 2;
        }
    }            
}	


/**
@brief     	Builds 64-bit bitmap from (min, max) range.
@param     	state
                SBITS state structure
@param		min
				minimum value (may be NULL)
@param		max
				maximum value (may be NULL)
@param		bm
				bitmap created
*/
void buildBitmapInt64FromRange(sbitsState *state, void *min, void *max, void *bm)
{
    uint64_t* bmval = (uint64_t*) bm;

    if (min == NULL && max == NULL)
    {
        *bmval = UINT64_MAX;  /* Everything */
        return;
    }
        
    int8_t i = 0;
    uint64_t val = (uint64_t) (INT64_MAX)+1;
    if (min != NULL)
    {
        /* Set bits based on min value */
        state->updateBitmap(min, bm);
		
        /* Assume here that bits are set in increasing order based on smallest value */                        
        /* Find first set bit */
        while ( (val & *bmval) == 0 && i < 64)
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
		uint64_t prev = *bmval;
        state->updateBitmap(max, bm);
		if (*bmval == prev)
			return;	/* Min and max bit vector are the same */

        while ( (val & *bmval) == 0 && i < 64)
        {
            i++;
            *bmval = *bmval + val;
            val = val / 2;                 
        }
    }
    else
    {
        while (i < 64)
        {
            i++;
            *bmval = *bmval + val;
            val = val / 2;
        }
    }            
}	
