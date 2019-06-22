#include "linkedlist.h"


///////////////////////////////////////////////
// createnode                                //
// ========================================= //
// Input: char* -> new data                  //
// Output: node* -> new node                 //
// Purpose: create new node                  //
///////////////////////////////////////////////

node* createnode(ElementType newdata)
{
    node* NewNode = (node*)malloc(sizeof(node));    // memory allocate the node

    NewNode->data = (char *)malloc(sizeof(char)*strlen(newdata) + 1);
    strcpy(NewNode->data, newdata);

    NewNode->pNext = NULL;  // set the data

    return NewNode;
}

///////////////////////////////////////////////
// IsEmpty                                   //
// ========================================= //
// Input: linkedlist* -> list for check      //
// Output: int -> 1 : empty 0 : not empty    //
// Purpose: check list whether is empty      //
///////////////////////////////////////////////

int IsEmpty(linkedlist* ll)
{
    return (ll->pHead == NULL) ? 1 : 0;
}

///////////////////////////////////////////////
// init                                      //
// ========================================= //
// Input: linkedlist* -> list to initialize  //
// Output: void                              //
// Purpose: initialize the linked list       //
///////////////////////////////////////////////

void init(linkedlist* ll)
{
    // initialize the linkedlist
    ll->size = 0;
    ll->pHead = NULL;
    ll->pTail = NULL;
}

///////////////////////////////////////////////
// insert                                    //
// ========================================= //
// Input: linkedlist* -> list to insert      //
//        char*       -> data to insert      //
// Output: void                              //
// Purpose: insert the node to list          //
///////////////////////////////////////////////

void insert(linkedlist* ll, ElementType newdata)
{
    node* NewNode = createnode(newdata);    // create new node

    
    if(ll->pHead == NULL)   // If list was empty
    {
        ll->pHead = NewNode;
        ll->pTail = NewNode;    // set the head & tail pointer
    }
    else    // If list already has elements
    {
        ll->pTail->pNext = NewNode;
        ll->pTail = NewNode;    // add after the tail and set the tail
    }

    ll->size += 1;  // size increase
}

///////////////////////////////////////////////
// getNodeat                                 //
// ========================================= //
// Input: linkedlist* -> list to find        //
//        int         -> index to find       //
// Output: node* -> found node               //
// Purpose: find the node locates at index   //
///////////////////////////////////////////////

node* getNodeat(linkedlist* ll, int idx)
{
    node* pCur = ll->pHead; // point the list's head

    while(pCur != NULL && (idx--) > 0)
        pCur = pCur->pNext; // move until the match index

    return pCur;
}

//////////////////////////////////////////////////////////
// compare                                              //
// ==================================================== //
// Input: char* -> string to compare                    //
//        char* -> string to compare                    //
// Output: int -> comparing result                      //
// Purpose: compare two string(lexicographical order)   //
//////////////////////////////////////////////////////////

int compare(ElementType a, ElementType b)
{
    char aTemp[256] = {0, };    // string for compare
    char bTemp[256] = {0, };    // string for compare

    strcpy(aTemp, a);   // copy the string
    strcpy(bTemp, b);   // copy the string

    //=======   hidden file   =======//
    if(aTemp[0] == '.')     // if file was hidden file
        memcpy(aTemp, &a[1], strlen(a));    // ignore the first '.' character

    if(bTemp[0] == '.')     // if file was hidden file
        memcpy(bTemp, &b[1], strlen(b));    // ignore the first '.' character
    //===============================//

    return strcasecmp(aTemp, bTemp);
}

//////////////////////////////////////////////////////////////////
// Filename_Sort                                                // 
// ============================================================ // 
// Input:   linkedlist* -> linkedlist                           //
//          int         -> -r option                            //
// Output:  void                                                // 
// Purpose: Sorting the linkedlist by Bubble sort algorithm     //
//////////////////////////////////////////////////////////////////

void Filename_Sort(linkedlist* ll, int reverse)
{
    int size = ll->size;
    int i, j;
    for(i=0; i<size-1; i++)
    {
        for(j=0; j<size-1-i; j++)   // Bubble up the value through comparison
        {
            node* target1 = getNodeat(ll, j);   // get the node at j
            node* target2 = getNodeat(ll, j+1); // get the node at j+1
             
            int result = compare(target1->data, target2->data);  // compare the value

            if(reverse)     // if -r option exist
            {
                if(result < 0)  // If 'result' is negative, first param is located front
                {
                    // change the value
                    char* temp = target1->data;
                    target1->data = target2->data;
                    target2->data = temp;
                }

            }
            else            // otherwise
            {
                if(result > 0)  // If 'result' is positive, first param is located behind
                {
                    // change the value
                    char* temp = target1->data;
                    target1->data = target2->data;
                    target2->data = temp;
                }
            }
        }
    }
}


//////////////////////////////////////////////////////////////////
// getFileSize                                                  // 
// ============================================================ // 
// Input:   char* -> filename to get size information           //
// Output:  void                                                // 
// Purpose: get the file size of 'filename'                     //
//////////////////////////////////////////////////////////////////

long getFileSize(char* filename)
{
    struct stat buf;
    char cwd[255] = {0, };      // current working directory
    char fpath[255] = {0, };    // filepath

    getcwd(cwd, 255);           // get current working directory
    strncpy(fpath, cwd, 255);   // copy the path
    strcat(fpath, "/");         // append '/' 
    strcat(fpath, filename);    // append filename at path

    lstat(fpath, &buf);         // get stat info

    return (long)buf.st_size;   // return file size from stat info
}


//////////////////////////////////////////////////////////////////
// Filesize_Sort                                                // 
// ============================================================ // 
// Input:   linkedlist* -> linkedlist                           //
//          int         -> -r option                            //
// Output:  void                                                // 
// Purpose: Sorting the linkedlist by Bubble sort algorithm     //
//////////////////////////////////////////////////////////////////

void Filesize_Sort(linkedlist* ll, int reverse)
{
    int size = ll->size;
    int i, j;
    for(i=0; i<size-1; i++)
    {
        for(j=0; j<size-1-i; j++)   // Bubble up the value through comparison
        {
            node* target1 = getNodeat(ll, j);   // get the node at j
            node* target2 = getNodeat(ll, j+1); // get the node at j+1
            
            long filesize1 = getFileSize(target1->data);
            long filesize2 = getFileSize(target2->data);

            if(reverse)     // if -r option exist
            {
                if(filesize1 > filesize2)
                {
                    // change the value
                    char* temp = target1->data;
                    target1->data = target2->data;
                    target2->data = temp;
                }
            }
            else            // otherwise
            {
                if(filesize1 < filesize2)
                {
                    // change the value
                    char* temp = target1->data;
                    target1->data = target2->data;
                    target2->data = temp;
                }
            }
        }
    }
}
