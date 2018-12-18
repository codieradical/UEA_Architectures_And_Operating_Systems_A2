#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/_types/_s_ifmt.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>

// char* getPermissionsString(mode_t permissions) {

//    /* Allocate a character array (a string) for the permissions string.
//       Strings are null-terminated. A null character has a value of 0.
//       '\0' is a character of value 0.
//       If strings aren't terminated correcvtly, problems can occur.*/
//    char permissionsString[11] = "----------\0";

//    //printf(permissionsString);
//    if (permissions & S_IFDIR) permissionsString[0] = 'd';
//    if (permissions & S_IRUSR) permissionsString[1] = 'r';
//    if (permissions & S_IWUSR) permissionsString[2] = 'w';
//    if (permissions & S_IXUSR) permissionsString[3] = 'x';
//    if (permissions & S_IRGRP) permissionsString[4] = 'r';
//    if (permissions & S_IWGRP) permissionsString[5] = 'w';
//    if (permissions & S_IXGRP) permissionsString[6] = 'x';
//    if (permissions & S_IROTH) permissionsString[7] = 'r';
//    if (permissions & S_IWOTH) permissionsString[8] = 'w';
//    if (permissions & S_IXOTH) permissionsString[9] = 'x';
//    return permissionsString;
// }


//TODO
//FILE TREE WALK
//nftw
void listFiles(char *directory) {
   DIR *dp = opendir(directory);
   struct dirent *entry;
   while ((entry = readdir(dp)) != NULL)
   {
      if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
      struct stat fileStat;
      lstat(entry->d_name, &fileStat);

      char modeStr[11] = "----------\0";

      //If the file is not a regular file or directory, continue looping (skip it).
      if (!S_ISREG(fileStat.st_mode) && !S_ISDIR(fileStat.st_mode)) continue;

      printf("%s\n", entry->d_name);

      if (S_ISDIR(fileStat.st_mode)) {
         char currentDirectory[4096];
         sprintf(currentDirectory, "%s/%s", directory, entry->d_name);
         listFiles(currentDirectory);
         continue;
      }

      if (fileStat.st_mode & S_IFDIR) modeStr[0] = 'd';
      if (fileStat.st_mode & S_IRUSR) modeStr[1] = 'r';
      if (fileStat.st_mode & S_IWUSR) modeStr[2] = 'w';
      if (fileStat.st_mode & S_IXUSR) modeStr[3] = 'x';
      if (fileStat.st_mode & S_IRGRP) modeStr[4] = 'r';
      if (fileStat.st_mode & S_IWGRP) modeStr[5] = 'w';
      if (fileStat.st_mode & S_IXGRP) modeStr[6] = 'x';
      if (fileStat.st_mode & S_IROTH) modeStr[7] = 'r';
      if (fileStat.st_mode & S_IWOTH) modeStr[8] = 'w';
      if (fileStat.st_mode & S_IXOTH) modeStr[9] = 'x';
      //Interesting note:
      //During development, I tested this on Mac OS for convenience. 
      //Mac OS has extended attributes, which it displays with an extra mode character, '@'.
      //eg "-rw-r--r--@ 1 alex231  staff  275 16 Dec 17:21 assignment2.code-workspace"

      struct group *fileGroup;
      fileGroup = getgrgid(fileStat.st_gid);

      struct passwd *fileOwner;
      fileOwner = getpwuid(fileStat.st_uid);

      char dateString[13];
      strftime(dateString, 13, "%d %b %R\0", localtime(&(fileStat.st_mtime)));

     
      printf("%s %d %s %6s %5lld %s %s\n", modeStr, fileStat.st_nlink, fileOwner->pw_name, fileGroup->gr_name, fileStat.st_size, dateString, entry->d_name);
   }

   closedir(dp);
}

int main(int argc, char *argv[])
{
   //Linux has a maximum directory length of 4096 for most filesystems.
   //So allocate a 4096 long character buffer.
   char currentDirectory[4096];
   //Get Current Working Directory
   getcwd(currentDirectory);

   printf("\nSearching for files in:\n");
   printf("%s", currentDirectory);
   printf("\n\n");

   listFiles(currentDirectory);

   printf("\n");

   return EXIT_SUCCESS;
}

