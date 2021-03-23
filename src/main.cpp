/******************************************************************************/
/**
@file		    main.cpp
@author		  Ramon Lawrence
@brief		  Main Arduino program for testing SBITS implementation.
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
#include <Arduino.h>
#include <SD.h>

#include "test_sbits.h"

Sd2Card card;
SdVolume volume;
SdFile root;
const int chipSelect = 4;


void setup() 
{
  Serial.begin(9600);

  printf("\nInitializing SD card...");

  /* Verify SD card is accessible */
  if (!card.init(SPI_HALF_SPEED, chipSelect)) 
  {
    printf("initialization failed. Things to check:\n");
    printf("* is a card inserted?\n");
    printf("* is your wiring correct?\n");
    printf("* did you change the chipSelect pin to match your shield or module?\n");
    while (1);
  } 
  else 
  {
    printf("Wiring is correct and a card is present.\n");
  }

  /* Print the type of card */ 
  printf("\nCard type:  ");
  switch (card.type()) 
  {
    case SD_CARD_TYPE_SD1:
      printf("SD1\n");
      break;
    case SD_CARD_TYPE_SD2:
      printf("SD2\n");
      break;
    case SD_CARD_TYPE_SDHC:
      printf("SDHC\n");
      break;
    default:
      printf("Unknown\n");
  }

  /* Open the 'volume'/'partition' - it should be FAT16 or FAT32 */
  if (!volume.init(card)) 
  {
    printf("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card\n");
    while (1);
  }

  printf("Clusters:         %lu\n", volume.clusterCount());  
  printf("Blocks x Cluster: %d\n", volume.blocksPerCluster());  
  printf("\nTotal Blocks:   %lu\n", volume.blocksPerCluster() * volume.clusterCount());

  /* Print the type and size of the first FAT-type volume */
  uint32_t volumesize;
  printf("Volume type is:    FAT %d\n", volume.fatType());
  
  volumesize = volume.blocksPerCluster();    /* Clusters are collections of blocks */
  volumesize *= volume.clusterCount();      
  volumesize /= 2;                           /* SD card blocks are always 512 bytes (2 blocks are 1KB) */
  printf("\nVolume size (Kb):  %lu\n", volumesize);    
  volumesize /= 1024;
  printf("\nVolume size (Mb):  %lu\n", volumesize);
  volumesize /= 1024;
  printf("\nVolume size (Gb):  %lu\n", volumesize);  
  
  /* List all files in the card with date and size */
  printf("\n\nFiles found on the card (name, date and size in bytes): ");
  root.openRoot(volume);

  root.ls(LS_R | LS_DATE | LS_SIZE);

  /* Delete any files not needed */
  // root.remove(root, "myfile.bin");
  
  SD.begin(4);
  
  runalltests_sbits(); 
}

void loop() 
{
 
}