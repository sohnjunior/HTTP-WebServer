#ifndef __LINKEDLIST__
#define __LINKEDLIST__

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

typedef char* ElementType;

typedef struct NODE
{
    struct NODE* pNext; // next node pointer
    ElementType data;   // data
} node;

node* createnode(ElementType newdata);

typedef struct LINKEDLIST
{
    int size;       // list size
    node* pHead;    // list head pointer
    node* pTail;    // list tail pointer

} linkedlist;

int  IsEmpty(linkedlist* ll);
void init(linkedlist* ll);
void insert(linkedlist* ll, ElementType newdata);
node* getNodeat(linkedlist* ll, int idx);

int compare(ElementType a, ElementType b);
void Filename_Sort(linkedlist* ll, int reverse);

long getFileSize(char* filename);
void Filesize_Sort(linkedlist* ll, int reverse);

#endif
