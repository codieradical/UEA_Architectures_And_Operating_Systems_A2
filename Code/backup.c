/*******************************************************************************

   File        : backupfiles.s

   Date        : Wednesday 19th December 2018

   Description : Assignment 2 Task 3, backup and restore files..

   History     : 18/12/2018 - v1.00 - Initial Implementation.

   Author      : Alex H. Newark

*******************************************************************************/

/*******************************************************************************
   Note:

   While I'm happy to have a working solution, 
   I can think of quite a few ways to improve it. To name a few:
   -  More utility functions for dealing with tar files, specifically around
      serialization and deserialization.
   -  Handling errors better by passing them up where suitable.
   -  Spliting functions down to smaller ones.
   -  Refactoring to make better use of variables and repeat operations less.
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
#include <utime.h>

/* The path of a file is an array of characters with a max size of 4096.
   The path always includes the backup path, 
   which doesn't need to be printed.
   The easiest way to make sure it isn't printed, is to just print everything
   after it. 
   
   in this case:
   path + lengthOfBackupPath = relativePath 
   
   If for any reason this isn't set, the default of 0 will cause the full
   path to be printed, which isn't the end of the world :) */
static short backupPathLength = 0;
static time_t modifiedAfterTimestamp = 0;
static FILE *archiveFile;
static char archivePath[4351];

/* Structure / Function Definitions
   Alternatively I could use a header, but the assignment brief only
   specifies one file name, so I'm sticking to that. 
   When structures are defined, their body much be fully defined before they
   are refenced. So I'm placing them at the top. */

/* Tar files are formed of 512 byte blocks. 
   Before each file in the archive, there's a header block, containg file 
   details. Following the header is the file's data, with padding to fill the 
   gap between the end of file, and the next 512 byte block. 
   
   There are different tar header formats, I am using the UStar format.
   Early formats only allowed file paths 100 characters long, maximum.
   However, UStar format provides an extra 155 characters at the end of the 
   header for longer files paths, which I am making use of. 
   The extra characters, if not null, are appended to the beginning of the 
   first filePath field. */
struct tar_header_block {
      /* Old, Pre-POSIX.1-1988 fields */
      /* Must be null terminated unless all 100 characters are used. */
      char filePath[100];
      /* Octal mode. */
      char fileMode[8];
      /* Octal number in ASCII, null (or space) terminated, zero padded. */
      char ownerId[8];
      /* Octal number in ASCII, null (or space) terminated, zero padded. */
      char groupId[8];
      char fileSize[12];
      /* Numeric, octal unix time format. */
      char modifiedTime[12];
      /* The checksum is calculated based on the header with an empty 
         checksum containing space characters. */
      char checksum[8];
      /* This field was originally the link type, but with the ustar format,
         it represents the file type. 
         eg, normal file/symbolic link/directory... */
      char type;
      /* Must be null terminated unless all 100 characters are used. */
      char linkName[100];
      /* UStar only fields from here.   */
      /* Must be null terminated */
      char ustarMagic[6];
      /* Always "00" */
      char version[2];
      /* Must be null terminated */
      char ownerName[32];
      /* Must be null terminated */
      char groupName[32];
      /* Magor and minor are for devices, which I'm not using, so for this
         purpose they can be left null. */
      char major[8];
      char minor[8];
      /* The file path prefix, must be null terminated unless all 155 characters
         are used. */
      char filePathPrefix[155];
      /* The header uses 500/512 bytes of it's block. Some custom formats
         can make use of the extra 12 bytes, but in generally it's null. 
         As an improvement, this tool could use some of this space to store
         a file's change timestamp (not modified date). */
      char unused[12];
};

static int backupFile(const char* path, const struct stat *fileStat, 
   int flag, struct FTW* fileTreeWalker);
void getModeString(mode_t mode, char modeStr[]);
void printHelp();
static void makeHeader(const char* relativePath, const struct stat *fileStatus, 
   struct tar_header_block *tarHeader);
static void backup(char* backupPath);
static void restore();
unsigned int convertOctalStringToUInt(char * octalString, 
   unsigned int stringSize);

/*******************************************************************************
   printHelp
      Prints a simple help message for the user.
*******************************************************************************/
void printHelp() {
   printf("\nbackupfiles\n\n"
         "Lists files modified after the given datetime.\n"
         "usage: backupfiles (options) -f <archive path> (backup directory)\n"
         "options: \n"
         "   -t <datetime>\n"
         "      the datetime to list files after.\n"
         "      datetime can be provided as a string of format"
         "      \"YYYY-MM-DD hh:mm:ss\", or as a file path, from which\n"
         "      the modified date will be read.\n"
         "      Defaults to 1970-01-01 00:00:00.\n"
         "   -h\n"
         "      Displays utility help (this messsge).\n"
         "   -f <filename>\n"
         "      (Required) The name/path of the archive file to"
         "      backup to / restore from.\n\n");
   exit(1);
}


/*******************************************************************************
   main
      Program entry function.
      - argc = argument count
      - argv = command line arguments
*******************************************************************************/
int main(int argc, char *argv[])
{
   if(argc < 2) {
      printf("Insufficient Arguments. Use -h for help.\n");
      return 1;
   }

   printf("%s", argv[0]);

   //Detect symbolic link.
   char restoring = 0;
   if(strcmp(argv[0], "restore")) {
      restoring = 1;
   }

   char backupPath[4096];

   /* Parse Arguments */
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
         continue;
      }

      else if(strcmp(argv[i], "-f") == 0) {
         //If -f is provided with no filename...
         if(argc <= i + 1) {
            printf("Invalid Arguments: No filename provided.\n");
            return 1;
         }

         strcpy(archivePath, argv[i + 1]);

         i++;
         continue;
      }
      
      else {
         strcpy(backupPath, argv[i]);
      }
   }

   backupPathLength = strlen(backupPath) + 1;
   int archivePathLength = strlen(archivePath);

   if(archivePathLength < 1) {
      printf("Invalid Arguments: An archive path is required.\n");
      return 1;
   }

   if(backupPathLength > 1 && !restoring) {
      archiveFile = fopen(archivePath, "wb+");
      backup(backupPath);
   } else {
      archiveFile = fopen(archivePath, "rb");
      restore();
   }

   fclose(archiveFile);
   printf("\n");

   return EXIT_SUCCESS;
}


/*******************************************************************************
   backup
      Creates a backup archive containing all files found within the backupPath
      directory.
*******************************************************************************/
static void backup(char* backupPath) {
   struct tm *localTime = localtime(&modifiedAfterTimestamp);
   char timestampString[20];
   strftime(timestampString, 20, "%Y-%m-%d %H:%M:%S", localTime);

   printf("\nSearching for files in:\n");
   printf("%s", backupPath);
   printf("\nModified after:\n");
   printf("%s", timestampString);
   printf("\n\n");

   int nfds;
   nfds = getdtablesize();
	if (nftw(backupPath, backupFile, nfds, FTW_F | FTW_D) != 0) {
      printf("Fatal Error: Could not find files.\n"
               "Please check the provided path: \"%s\".\n", backupPath);
      fclose(archiveFile);
      exit(1);
   }
   /* Write two empty blocks to the end of the file. */
   /* There has to be a better way to do this... */
   char padding[1024];
   memset(padding, 0, 1024);
   fwrite((void*)padding, 1, 1024, archiveFile);
}

/*******************************************************************************
   restore
      Restores filed from the backup archive.
*******************************************************************************/
static void restore() {
   char restorePath[4347];
   strncpy(restorePath, archivePath, strlen(archivePath) - 4);
   mkdir(restorePath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
   
   fseek(archiveFile, 0, SEEK_END);
   long int fileLen = ftell(archiveFile);
   rewind(archiveFile);

   if(fileLen % 512 != 0) {
      printf("Fatal Error: Corrupted backup file.\n"
         "Please check the provided file: \"%s\".\n", archivePath);
      exit(1);
   }

   /* Ignore the 1024 buffer at the end*/
   long int filePos = 0;
   while(filePos < fileLen - 1024) {
      struct tar_header_block *headerData = malloc(512);
      fread(headerData, 512, 1, archiveFile);
      //printf("\n%s%s\n", headerData->filePathPrefix, headerData->filePath);
      filePos += 512;

      int fileSize 
         = convertOctalStringToUInt(headerData->fileSize, 11);
      char *fileData = malloc(fileSize);
      fread(fileData, fileSize, 1, archiveFile);
      filePos += fileSize;

      char *restoreFilePath = malloc(4351);
      strcat(restoreFilePath, restorePath);
      strcat(restoreFilePath, "/");
      strcat(restoreFilePath, headerData->filePathPrefix);
      strcat(restoreFilePath, headerData->filePath);

      //Make necessary folders.
      char *currentFolderEnd = strchr(restoreFilePath, (int)'/');
      while(currentFolderEnd != NULL) {
         int currentFolderPathLength 
            = currentFolderEnd - restoreFilePath;
         char *currentFolder = malloc(4096);
         memcpy(currentFolder, restoreFilePath, currentFolderPathLength);
         currentFolder[currentFolderPathLength] = '\0';
         mkdir(currentFolder, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
         currentFolderEnd = 
            strchr(&restoreFilePath[currentFolderPathLength + 1], (int)'/');
      }

      FILE *restoreFile = fopen(restoreFilePath, "wb+");
      fwrite(fileData, 1, fileSize, restoreFile);
      fclose(restoreFile);
      chmod(restoreFilePath, convertOctalStringToUInt(headerData->fileMode, 8));
      struct utimbuf timeStamps;
      timeStamps.actime = time(NULL);
      timeStamps.modtime = convertOctalStringToUInt(headerData->modifiedTime, 11);
      utime(restoreFilePath, &timeStamps);
      int filePadding = 512 - (fileSize % 512);
      fseek(archiveFile, filePadding, SEEK_CUR);
      filePos += filePadding;
   }

   printf("\nSuccessfully restored from backup.\n");
}

/*******************************************************************************
   convertOctalStringToUInt
      Converts an octal strigt to a uint and returns it.
*******************************************************************************/
unsigned int convertOctalStringToUInt(char * octalString, 
   unsigned int stringSize)
{
    unsigned int converted = 0;
    int i = 0;
    while ((i < stringSize) && octalString[i]){
        converted = (converted << 3) | (unsigned int) (octalString[i++] - '0');
    }
    return converted;
}


/*******************************************************************************
   getModeString
      Returns the ls -l style string representation of a mode_t.
*******************************************************************************/
void getModeString(mode_t mode, char modeStr[]) {

   /* Allocate a character array (a string) for the permissions string.
      Strings are null-terminated. A null character has a value of 0.
      '\0' is a character of value 0.
      If strings aren't terminated correctly, problems can occur.*/
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

/*******************************************************************************
   backupFile
      Write an individual file to the backup archive.
*******************************************************************************/
static int backupFile(const char* path, const struct stat *fileStat, 
   int flag, struct FTW* fileTreeWalker) 
{
   /* If the file is not a regular file, 
      continue looping (skip it). 
      As a future improvement, directories could be included to preserve
      permission changes.
      This could be acomplished by backing-up folders made prior to the
      filter timestamp, if they contain fields modified after.*/
   if (!S_ISREG(fileStat->st_mode)) return 0;

   /* If the current path is the backup archive, ignore it */
   if(strcmp(path, archivePath) == 0 
      || strcmp(&path[backupPathLength], archivePath) == 0)
      return 0;

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

   FILE *file;
   char *fileData;
   long int filelen;

   /* Open the file... */
   file = fopen(path, "rb");
   /* If it couldn't be opened, move on... */
   if(file == NULL) return 1;
   /* Skip to the end of the file */
   fseek(file, 0, SEEK_END); 
   /* The length of the file is the cursor's position */
   filelen = ftell(file);
   /* Go back to the start of the file. */
   rewind(file);

   /* Allocate an array for file data */
   fileData = (char *)malloc((filelen)*sizeof(char));
   /* Read file data. */
   fread(fileData, filelen, 1, file); // Read in the entire file
   /* Close the file */
   fclose(file);

   /* Print file details. */
   printf("%s %d %s %6s %7lld %s %s\n", 
      modeStr, 
      fileStat->st_nlink, 
      fileOwner->pw_name, fileGroup->gr_name, 
      fileStat->st_size, 
      dateString, 
      &path[backupPathLength]);

   /* Allocate space for a new tar header. */
   struct tar_header_block *tarHeader = malloc(sizeof(struct tar_header_block));
   /* Make a tar header for the file */
   makeHeader(&path[backupPathLength], fileStat, tarHeader);
   /* Write the header to the archive */
   fwrite((void*)tarHeader, 1, 512, archiveFile);

   /* While the file has full 512 blocks of data left... */
   while(filelen > 512) {
      /* Write them to the file */
      fwrite(fileData, 1, 512, archiveFile);
      fileData = &fileData[512];
      filelen -= 512;
   }
   /* If the file has any remaining data... */
   if(filelen > 0) {
      /* Write it. */
      fwrite(fileData, 1, filelen, archiveFile);
      /* Write padding. */
      char *padding = malloc(512 - filelen);
      memset(padding, 0, 512 - filelen);
      fwrite((void*)padding, 1, 512 - filelen, archiveFile);
   }
   
   return 0;
}

/*******************************************************************************
   makeHeader
      Creates a tar header for a file.
*******************************************************************************/
static void makeHeader(const char* relativePath, const struct stat *fileStatus, 
   struct tar_header_block *tarHeader)
{
   memset(tarHeader, '\0', 512);
   /* setup ustar magic and checksum empty */
   strcpy(tarHeader->ustarMagic, "ustar");
   memcpy(tarHeader->major, "000000 ", 7);
   memcpy(tarHeader->minor, "000000 ", 7);
   memset(tarHeader->version, '0', 2);
   memset(tarHeader->checksum, ' ', sizeof(char) * 8);

   /* setup filePath and filePath prefix */
   unsigned int pathLength = strlen(relativePath);
   if(pathLength > 255) {
      printf("Unable to backup files, file path \n\"%s\"\n is too long.\n",
         relativePath);
      /* This could be handled differently, by returning the error, 
         but for this purpose exiting is sufficient. */
      exit(1);
   } else if (pathLength > 100) {
      /* This could be a char, as it's never higher than 155. */
      int prefixLength = pathLength - 100;
      strcpy(tarHeader->filePath, &relativePath[prefixLength]);
      strncpy(tarHeader->filePathPrefix, relativePath, prefixLength);
      if (prefixLength != 155) {
         /* null terminate path prefix */
         tarHeader->filePathPrefix[prefixLength] = 0;
      }
   } else {
      strcpy(tarHeader->filePath, relativePath);
      if(pathLength != 100) {
         /* null terminate path */
         tarHeader->filePathPrefix[pathLength] = 0;
      }
   }

   if (S_ISREG(fileStatus->st_mode)) {
      tarHeader->type = '0';
   } else if(S_ISDIR(fileStatus->st_mode)) {
      tarHeader->type = '5';
   } else {
      printf("Fatal Error: Unable to create archive header for file "
            "\n\"%s\"\n , unsupported file type\n", relativePath);
      /* This could be handled differently, by returning the error, 
         but for this purpose exiting is sufficient. */
      exit(1);
   }
   
   sprintf(tarHeader->fileMode, "%06o ", fileStatus->st_mode);
   sprintf(tarHeader->ownerId, "%06o ", fileStatus->st_uid);
   sprintf(tarHeader->groupId, "%06o ", fileStatus->st_gid);
   sprintf(tarHeader->fileSize, "%011o", (int)fileStatus->st_size);
   tarHeader->fileSize[11] = ' ';
   sprintf(tarHeader->modifiedTime, "%0lo", fileStatus->st_mtime);
   tarHeader->modifiedTime[11] = ' ';
   
   struct passwd *fileOwner = getpwuid(fileStatus->st_uid);
   strcpy(tarHeader->ownerName, fileOwner->pw_name);

   struct group *fileGroup = getgrgid(fileStatus->st_gid);
   strcpy(tarHeader->groupName, fileGroup->gr_name);

   /* The checksum is very important, if it's wrong, the tar won't be opened
      by many tools */
   unsigned int checksum = 0;
   unsigned char *tarHeaderBytes = (unsigned char*)tarHeader;

   for (int i = 0; i < 500; i++) {
      checksum += tarHeaderBytes[i];
   }

   sprintf(tarHeader->checksum, "%06o", checksum);
   tarHeader->checksum[6] = '\0';
   tarHeader->checksum[7] = ' ';
}