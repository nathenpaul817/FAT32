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

#define MAX_NUM_ARGUMENTS 3

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

int main()
{
  int i;
  int check = 1;
  char *cmd_str = (char *)malloc(MAX_COMMAND_SIZE);
  int16_t BPB_BytsPerSec;
  int8_t BPB_SecPerClus;
  int16_t BPB_RvsdSecCnt;
  int16_t BPB_NumFATs;
  int16_t BPB_FATSz32;

  while (check)
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
    /*int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ ) 
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );  
    }

    free( working_root );*/
    FILE *fp;
    if (token[0] != NULL && token[1] != NULL && strcmp(token[0], "open") == 0)
    {
      fp = fopen(token[1], "r");

      //We use SEEK_SET for this one
      fseek(fp, 11, SEEK_SET);
      fread(&BPB_BytsPerSec, 2, 1, fp);

      fseek(fp, 13, SEEK_SET);
      fread(&BPB_SecPerClus, 1, 1, fp);

      fseek(fp, 14, SEEK_SET);
      fread(&BPB_RvsdSecCnt, 2, 1, fp);

      fseek(fp, 16, SEEK_SET);
      fread(&BPB_NumFATs, 1, 1, fp);

      fseek(fp, 36, SEEK_SET);
      fread(&BPB_FATSz32, 4, 1, fp);
    }
    else if (strcmp(token[0], "bpb") == 0)
    {
      printf("BPB_BytsPerSec: %d\n", BPB_BytsPerSec); //512
      printf("BPB_SecPerClus: %d\n", BPB_SecPerClus); //1
      printf("BPB_RsvdSecCnt: %d\n", BPB_RvsdSecCnt); //32
      printf("BPB_NumFATs: %d\n", BPB_NumFATs);
      printf("BPB_FATSz32: %d\n", BPB_FATSz32);
    }
    else if (strcmp(token[0], "ls") == 0)
    {
      /*
       * ***********************************************************
       * ****************************************
       * 
      */
      fseek(fp, 0x100400, SEEK_SET);
      fread(dir, 16, sizeof(struct DirectoryEntry), fp);
      for (i = 0; i < 16; i++)
      {
        if ((dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20))
        {
          printf("Filename: %s %d %d\n", dir[i].DIR_Name, dir[i].DIR_FileSize, dir[i].DIR_FirstClusterLow);
        }
      }
    }
    else if (strcmp(token[0], "close") == 0)
    {
      fclose(fp);
    }
    else if (strcmp(token[0], "quit") == 0)
    {
      check = 0;
    }

    free(working_root);
  }

  return 0;
}
