/*******************************************************************************

   File        : backupfiles.s

   Date        : Wednesday 19th December 2018

   Description : Assignment 2 Task 3, backup and restore files..

   History     : 18/12/2018 - v1.00 - Initial Implementation.

   Author      : Alex H. Newark

*******************************************************************************/

/* Required for nftw */
#define _XOPEN_SOURCE 1
#define _XOPEN_SOURCE_EXTENDED 1

#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <ftw.h>
#include <string.h>
#include <fcntl.h>

/* The path of a file is an array of characters with a max size of 4096.
   The path always includes the backup path, 
   which doesn't need to be printed.
   The easiest way to make sure it isn't printed, is to just print everything
   after it. 
   
   in this case:
   path + lengthOfBackupPath = relativePath 
   
   If for any reason this isn't set, the default of 0 will cause the full
   path to be printed, which isn't the end of the world :) */
static short lengthOfBackupPath = 0;
static time_t modifiedAfterTimestamp = 0;

/* These are optional due to the order of functions,
   but I've added them in case I move things around, or call functions more. 
   Alternatively I could use a header. */
static int printFile(const char* path, const struct stat *fileStat, 
   int flag, struct FTW* fileTreeWalker);
void getModeString(mode_t mode, char modeStr[]);
void printHelp();

void printHelp() {
   printf("\nbackupfiles\n\n"
            "Lists files modified after the given datetime.\n"
            "usage: backupfiles <options> <list directory>\n"
            "options: \n"
            "   -t <datetime>\n"
            "      the datetime to list files after.\n"
            "      datetime can be provided as a string of format"
            "      \"YYYY-MM-DD hh:mm:ss\", or as a file path, from which\n"
            "      the modified date will be read.\n"
            "      Defaults to 1970-01-01 00:00:00.\n"
            "   -h\n"
            "      Displays utility help (this messsge).\n\n");
   exit(1);
}

int main(int argc, char *argv[])
{
   if(argc < 2) {
      printf("Insufficient Arguments. Use -h for help.");
      return 1;
   }

   char path[4096];

   for(int i = 1; i < argc; i++) {

      if(strcmp(argv[i], "-h") == 0) {
         printHelp();
      } 
      
      else if(strcmp(argv[i], "-t") == 0) {
         //If -t is provided with no datetime...
         if(argc <= i + 1) {
            printf("Invalid Arguments: No datetime provided.\n");
            return 1;
         }

         struct tm timestamp;
         if(strptime(argv[i + 1], "%Y-%m-%d %H:%M:%S", &timestamp) == NULL) {
            int fileDescriptor = open(argv[i + 1], O_RDWR);
            if(fileDescriptor == -1) {
               printf("Fatal Error: Unable to read provided timestamp:\n"
                     "\"%s\"\n", argv[i + 1]);
               return 1;
            }
            struct stat fileStatus;
            if(fstat(fileDescriptor, &fileStatus) != 0) {
               printf("Fatal Error: Unable to read provided\n"
                     "file modified date.\n");
               return 1;
            }
            modifiedAfterTimestamp = fileStatus.st_mtime;
         } else {
            modifiedAfterTimestamp = timegm(&timestamp);
         }
         i++;
      }
      
      else {
         strcpy(path, argv[i]);
      }
   }

   if(strlen(path) < 1) {
      printf("Invalid Arguments: No path provided.\n");
      return 1;
   }

   lengthOfBackupPath = (short)strlen(path) + 1;

   struct tm *localTime = localtime(&modifiedAfterTimestamp);
   char timestampString[20];
   strftime(timestampString, 20, "%Y-%m-%d %H:%M:%S", localTime);

   printf("\nSearching for files in:\n");
   printf("%s", path);
   printf("\nModified after:\n");
   printf("%s", timestampString);
   printf("\n\n");

   int nfds;
   nfds = getdtablesize();
	if (nftw(path, printFile, nfds, FTW_F | FTW_D) != 0) {
      printf("Fatal Error: Could not find files.\n"
               "Please check the provided path: \"%s\".\n", path);
      return 1;
   }

   printf("\n");

   return EXIT_SUCCESS;
}

void getModeString(mode_t mode, char modeStr[]) {

   /* Allocate a character array (a string) for the permissions string.
      Strings are null-terminated. A null character has a value of 0.
      '\0' is a character of value 0.
      If strings aren't terminated correcvtly, problems can occur.*/
   modeStr = strcpy(modeStr, "----------\0");

   /* Currently, no directory modes use this function,
      but I'm going to leave this condition in in case of reuse/expansion.
      the performance benefit without it is greatly insignificant
      when considering the time it may take another developer to understamd
      why this isn't working as they expect in potential future reuse. */
   if (mode & S_IFDIR) modeStr[0] = 'd';
   if (mode & S_IRUSR) modeStr[1] = 'r';
   if (mode & S_IWUSR) modeStr[2] = 'w';
   if (mode & S_IXUSR) modeStr[3] = 'x';
   if (mode & S_IRGRP) modeStr[4] = 'r';
   if (mode & S_IWGRP) modeStr[5] = 'w';
   if (mode & S_IXGRP) modeStr[6] = 'x';
   if (mode & S_IROTH) modeStr[7] = 'r';
   if (mode & S_IWOTH) modeStr[8] = 'w';
   if (mode & S_IXOTH) modeStr[9] = 'x';
   /* Interesting note:
      During development, I tested this on Mac OS for convenience. 
      Mac OS has extended attributes, 
      which it displays with an extra mode character, '@'.
      eg "-rw-r--r--@ 1 alex231  staff  275 16 Dec 17:21 .gitignore" */
}

static int printFile(const char* path, const struct stat *fileStat, 
   int flag, struct FTW* fileTreeWalker) 
{
   /* If the file is not a regular file or directory, 
      continue looping (skip it). */
   if (!S_ISREG(fileStat->st_mode)) return 0;

   /* If the file modified or changed timestamp is lower than (before) the 
      supplied modified after timestamp, return, don't print it. */
   if(fileStat->st_mtime < modifiedAfterTimestamp 
      && fileStat->st_ctime < modifiedAfterTimestamp) 
   {
      return 0;
   }

   /* As a personal preference, when declaring array pointers, I place the
      asterisk before the space, and otherwise after. 
      If contributing to a shared project, I follow the existing  standard, 
      but here the placement is mixed but consistent. */
   char modeStr[11];
   getModeString(fileStat->st_mode, modeStr);

   struct group *fileGroup;
   fileGroup = getgrgid(fileStat->st_gid);

   struct passwd *fileOwner;
   fileOwner = getpwuid(fileStat->st_uid);

   char dateString[13];
   strftime(dateString, 13, "%d %b %R\0", gmtime(&(fileStat->st_mtime)));

   /* I'm using fixed column sizes here for convenience.
      As a future improvement, these column lengths could by dynamic,
      by looping through files before printing to calculate how many characters
      the longest field in each column contains. */
   printf("%s %d %s %6s %7lld %s %s\n", 
      modeStr, 
      fileStat->st_nlink, 
      fileOwner->pw_name, fileGroup->gr_name, 
      fileStat->st_size, 
      dateString, 
      &path[lengthOfBackupPath]);
   
   return 0;
}
