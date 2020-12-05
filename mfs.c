// The MIT License (MIT)
//
// Copyright (c) 2020 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20

#define MAX_NUM_ARGUMENTS 4
#define NUM_ENTRIES 16

#define WHITESPACE " \t\n" // We want to split our command line up into tokens \
                           // so we need to define what delimits our tokens.   \
                           // In this case  white space                        \
                           // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255 // The maximum command-line size

struct __attribute__((__packed__)) DirectoryEntry
{
  char DIR_Name[11];
  uint8_t DIR_Attr;
  uint8_t Unused1[8];
  uint16_t DIR_FirstClusterHigh;
  uint8_t Unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};

struct DirectoryEntry dir[16];

int16_t BPB_BytsPerSec;
int8_t BPB_SecPerClus;
int16_t BPB_RsvdSecCnt;
int8_t BPB_NumFATs;
int32_t BPB_FATSz32;

FILE *fp;
int file_open = 0;

int32_t LBAToOffset(int32_t sector)
{
  return ((sector - 2) * BPB_BytsPerSec) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec);
}

int16_t NextLB(int32_t sector)
{
  uint32_t FATAddress = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
  int16_t val;
  fseek(fp, FATAddress, SEEK_SET);
  fread(&val, 2, 1, fp);
  return val;
}

int compare(char *userString, char *directoryString)
{
  char *dotdot = "..";

  if (strncmp(dotdot, userString, 2) == 0)
  {
    if (strncmp(userString, directoryString, 2) == 0)
    {
      return 1;
    }
    return 0;
  }

  char IMG_Name[12];

  strncpy(IMG_Name, directoryString, 11);
  IMG_Name[11] = '\0';

  char input[11];
  memset(input, 0, 11);
  strncpy(input, userString, strlen(userString));

  char expanded_name[12];
  memset(expanded_name, ' ', 12);

  char *token = strtok(input, ".");

  strncpy(expanded_name, token, strlen(token));

  token = strtok(NULL, ".");

  if (token)
  {
    strncpy((char *)(expanded_name + 8), token, strlen(token));
  }

  expanded_name[11] = '\0';

  int i;
  for (i = 0; i < 11; i++)
  {
    expanded_name[i] = toupper(expanded_name[i]);
  }

  if (strncmp(expanded_name, IMG_Name, 11) == 0)
  {
    return 1;
  }
  return 0;
}

int info()
{
  printf("BPB_BytsPerSec: %d\nBPB_BytsPerSec: %.4x\n", BPB_BytsPerSec, BPB_BytsPerSec); //512
  printf("BPB_SecPerClus: %d\nBPB_SecPerClus: %.4x\n", BPB_SecPerClus, BPB_FATSz32);    //1
  printf("BPB_RsvdSecCnt: %d\nBPB_RsvdSecCnt: %.4x\n", BPB_RsvdSecCnt, BPB_FATSz32);    //32
  printf("BPB_NumFATs: %d\nBPB_NumFATs: %.4x\n", BPB_NumFATs, BPB_FATSz32);
  printf("BPB_FATSz32: %d\nBPB_FATSz32: %.4x\n", BPB_FATSz32, BPB_FATSz32);
}

int ls()
{
  int i;
  for (i = 0; i < NUM_ENTRIES; i++)
  {
    char filename[12];
    strncpy(filename, dir[i].DIR_Name, 11);
    filename[11] = '\0';

    if ((dir[i].DIR_Attr == ATTR_READ_ONLY || dir[i].DIR_Attr == ATTR_DIRECTORY || dir[i].DIR_Attr == ATTR_ARCHIVE) && filename[0] != 0xffffffe5)
    {
      printf("%s\n", filename);
    }
  }
  return 0;
}

int cd( char * directoryName)
{
  int i;
  int found = 0;
  for(i = 0; i < NUM_ENTRIES; i++)
  {
    if(compare(directoryName, dir[i].DIR_Name))
    {
      int cluster = dir[i].DIR_FirstClusterLow;

      if(cluster == 0)
      {
        cluster = 2;
      }
      int offset = LBAToOffset(cluster);
      fseek(fp, offset, SEEK_SET);

      fread(dir, sizeof(struct DirectoryEntry), NUM_ENTRIES, fp);
      
      found = 1;
      break;
    }
  }
  if(!found)
  {
    printf("Error Directory Wasn't found\n");
    return -1;
  }
  return 0;
}

int main()
{

  char *cmd_str = (char *)malloc(MAX_COMMAND_SIZE);

  while (1)
  {
    // Print out the mfs prompt
    printf("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while (!fgets(cmd_str, MAX_COMMAND_SIZE, stdin))
      ;

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;

    char *working_str = strdup(cmd_str);

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while (((arg_ptr = strsep(&working_str, WHITESPACE)) != NULL) &&
           (token_count < MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup(arg_ptr, MAX_COMMAND_SIZE);
      if (strlen(token[token_count]) == 0)
      {
        token[token_count] = NULL;
      }
      token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your FAT32 functionality
    if (strcmp(token[0], "open") == 0)
    {
      fp = fopen(token[1], "r");
      if (fp == NULL)
      {
        perror("File Could Not Be Open.\n");
      }
      else
      {
        file_open = 1;
      }

      //We use SEEK_SET for this one
      fseek(fp, 11, SEEK_SET);
      fread(&BPB_BytsPerSec, 1, 2, fp);

      fseek(fp, 13, SEEK_SET);
      fread(&BPB_SecPerClus, 1, 1, fp);

      fseek(fp, 14, SEEK_SET);
      fread(&BPB_RsvdSecCnt, 1, 2, fp);

      fseek(fp, 16, SEEK_SET);
      fread(&BPB_NumFATs, 1, 1, fp);

      fseek(fp, 36, SEEK_SET);
      fread(&BPB_FATSz32, 1, 4, fp);

      int rootAddress = (BPB_RsvdSecCnt * BPB_BytsPerSec) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec);

      fseek(fp, rootAddress, SEEK_SET);
      fread(dir, sizeof(struct DirectoryEntry), 16, fp);
    }
    else if (strcmp(token[0], "close") == 0)
    {
      if (file_open)
      {
        fclose(fp);
        fp = NULL;
        file_open = 0;
      }
      else
      {
        printf("Error: File Not Open.\n");
      }
    }
    else if (strcmp(token[0], "bpb") == 0)
    {
      if (file_open)
      {
        info();
      }
      else
      {
        printf("Error: Image is not open\n");
      }
    }
    else if (strcmp(token[0], "ls") == 0)
    {
      if (file_open)
      {
        ls();
      }
      else
      {
        printf("ERROR: File Not Open\n");
      }
    }
    else if (strcmp(token[0], "cd") == 0)
    {
      if (file_open)
      {
        cd(token[1]);
      }
      else
      {
        printf("ERROR: File Image Not Open\n");
      }
    }

    free(working_root);
  }
  return 0;
}
