#ifndef __LS__
#define __LS__

#include "linkedlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <fnmatch.h>
#include <memory.h>
#include <errno.h>


//===========   used options    ===========//
#define aFLAG   1
#define lFLAG   1<<1
#define hFLAG   1<<2
#define SFLAG   1<<3
#define rFLAG   1<<4
//=========================================//

int ls(int argc, char* argv[]);    // ls command


void CheckOption(int* option, int argc, char* argv[]);  // check the option from input string


void changeDIR(char* from, char* to);   // change directory
int  IsDIR(char* filename);             // check whether file is directory


void list_up(FILE* file, char* filename, DIR* dir_p, int option, int type);   // print the directory entry by options
void print_entry(FILE* file, linkedlist* ll, int option);                // print directory entry
void print_detail(FILE* file, char* filename, int nondir, int option);   // print detail information
void get_path_total(FILE* file, linkedlist* dir_entry);         // print directory path and total block
void get_hrFSize(char* str, long filesize);         // get file size in human readable format
void get_TypeandPerm(char* str, struct stat* buf);  // get file type and permission information


int  IsWildCard(char* pathname);         // check whether pathname is wildcard
void extract_WildCard(char* pathname, char* newpath, char* pattern);  // extract the pathname
int print_WildCard(FILE* file, char* pathname, int option);    // print the file match with pathname by wildcard

#endif
