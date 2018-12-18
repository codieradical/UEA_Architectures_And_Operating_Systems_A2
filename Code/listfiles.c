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

/* The path of a file is an array of characters with a max size of 4096.
   The path always includes the working directory, 
   which doesn't need to be printed.
   The easiest way to make sure it isn't printed, is to just print everything
   after it. 
   
   in this case:
   path + lengthOfWorkingDirectory = relativePath 
   
   If for any reason this isn't set, the default of 0 will cause the full
   path to be printed, which isn't the end of the world :) */

static short lengthOfWorkingDirectory = 0;

static int printFile(const char* path, const struct stat *fileStat, 
   int flag, struct FTW* fileTreeWalker);
void getModeString(mode_t mode, char modeStr[]);

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

int main(int argc, char *argv[])
{
   //Linux has a maximum directory length of 4096 for most filesystems.
   //So allocate a 4096 long character buffer.
   char currentDirectory[4096];
   //Get Current Working Directory
   getcwd(currentDirectory, 4096);

   lengthOfWorkingDirectory = (short)strlen(currentDirectory) + 1;

   printf("\nSearching for files in:\n");
   printf("%s", currentDirectory);
   printf("\n\n");

   int nfds;
   nfds = getdtablesize();
	if (nftw(currentDirectory, printFile, nfds, FTW_F | FTW_D) != 0) {
      return 1;
   }

   printf("\n");

   return EXIT_SUCCESS;
}

static int printFile(const char* path, const struct stat *fileStat, 
   int flag, struct FTW* fileTreeWalker) 
{
   /* If the file is not a regular file or directory, 
      continue looping (skip it). */
   if (!S_ISREG(fileStat->st_mode)) return 0;

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
   strftime(dateString, 13, "%d %b %R\0", localtime(&(fileStat->st_mtime)));

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
      &path[lengthOfWorkingDirectory]);
   
   return 0;
}
