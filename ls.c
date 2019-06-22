
//////////////////////////////////////////////////////////////////////////////////////////////
// File name        : ls.c                                                                  // 
// Date             : 2019/05/08                                                            // 
// OS               : Ubuntu 16.04 LTS 64bits                                               // 
// Author           : Sohn Jeong Hyeon                                                      // 
// Student ID       : 2014722068                                                            // 
// -----------------------------------------------------------------------------------------//
// Title        :   System Programming Assignment #3-2 (web server)                         //
// Description  :   This program activate just like linux 'ls' command                      //
//                  with several options(a,h,S,l,r) and also wildcard(*, ?, [seq])          //
//                  It creates a html file to show the result.                              //
//////////////////////////////////////////////////////////////////////////////////////////////

#include "ls.h"



//////////////////////////////////////////////////////////////////
// ls                                                           // 
// ============================================================ // 
// Input:   int     -> argument count                           //
//          char*   -> arguments array                          //
// Output:  int     -> whether ls is successful                 // 
// Purpose: make the html file with the result of ls            //
//////////////////////////////////////////////////////////////////

int ls(int argc, char* argv[])
{
    int  has_wildcard = 0;  // whether wildcard path exist
    DIR* dir_p = NULL;
    int  option = 0;        // check argument options

    //=============== open the file stream ===============//
    char  cwd[256] = {0, };
    int   index;
    FILE* file = fopen("html_ls.html", "w+");

    getcwd(cwd, 256);
    fprintf(file, "<html>\n<head><meta charset=\"utf-8\">\n<title>%s</title>\n</head>\n", cwd);
    fprintf(file, "<body>\n");
    //====================================================//

    optind = 1;
    CheckOption(&option, argc, argv);   // check the options

    //=============== If non-option arguments doesn't exist ===============//
    if(argc == optind)   
    {
        fprintf(file, "<h1>Welcome to System Programming Http</h1><br><br>\n");
        fprintf(file, "<p><table border=\"1\">\n");
        dir_p = opendir(".");
        if(dir_p == NULL)   // open() fail
            fprintf(file, "ERROR: %s\n", strerror(errno));
        else                // open() success
            list_up(file, ".", dir_p, option, 0);
        fprintf(file, "</table><br>\n</p>\n");
    }
    //=====================================================================//
    
    //============= Otherwise, check the non-option arguments =============//
    else 
    {
        fprintf(file, "<h1>System Programming Http</h1><br><br>\n");
        int idx = 0;    // linkedlist index
        int lsize = 0;  // linkedlist size
        linkedlist* nExlist = (linkedlist*)malloc(sizeof(linkedlist));  // non-exist file list
        linkedlist* Exlist = (linkedlist*)malloc(sizeof(linkedlist));   // exist file list

        init(nExlist);
        init(Exlist);   // initialize the linked list

        //----- Check all of the arguments whether the file exists or not -----//
        for(; optind < argc; optind++)
        { 
            int exist = access(argv[optind], F_OK);
            if(exist == -1) // If file access failed
            {
                if(IsWildCard(argv[optind]))                // if filepath was wildcard
                {
                    has_wildcard = 1;   // set wildcard-found-flag to 1
                    if(print_WildCard(file, argv[optind], option) == -1) 
                        return -1;  // if wildcard is not exist path, return -1
                }
                else
                    insert(nExlist, argv[optind]);          // otherwise, it is non-exist path
            }
            else            // If file access success
                insert(Exlist, argv[optind]);
        }
        //---------------------------------------------------------------------//

        //--------------------- print non-existing files ----------------------//
        lsize = nExlist->size;
        for(idx=0; idx<lsize; idx++)
        {
            node* n = getNodeat(nExlist, idx);
            return -1;
        }
        //---------------------------------------------------------------------//

        //----------------------  print existing files  ----------------------//
        lsize = Exlist->size;
        for(idx=0; idx<lsize; idx++)
        {
            node* n = getNodeat(Exlist, idx);
            fprintf(file, "<p><table border=\"1\">\n");
            //---------- directory ----------//
            if(IsDIR(n->data))
            {
                dir_p = opendir(n->data);
                if(dir_p == NULL)   // open() fail
                    fprintf(file, "ERROR: %s\n", strerror(errno));
                else                // open() success
                {
                    if(has_wildcard && !(option & lFLAG)) // if wildcard exist and -l option is not exist
                        fprintf(file, "Directory path: %s<br>\n", n->data);

                    list_up(file, n->data, dir_p, option, 0);   // print directory entries
                    closedir(dir_p);                            // close directory
                }
            }
            //-------- regular file --------//
            else
            {
                if(option & lFLAG)  // with -l option
                    fprintf(file, "<tr><th>Name</th><th>Permission</th><th>Link</th><th>Owner</th><th>Group</th><th>Size</th><th>Last modified</th></tr>\n");
                else                // without -l option
                    fprintf(file, "<tr><th>Name</th></tr>\n");
                list_up(file, n->data, NULL, option, 1);        // print directory entries
            }
            fprintf(file, "</table><br>\n</p>\n");
        }
        //--------------------------------------------------------------------//
    }
    //===============================================================================//

    fprintf(file, "</body>\n</html>");
    fclose(file);   // file stream close

    return 0;       // successful exit
}


//////////////////////////////////////////////////////////////////
// CheckOption                                                  // 
// ============================================================ // 
// Input:   int*    -> option result                            //
//          int     -> argument count                           //
//          char*   -> arguments array                          //
// Output:  void                                                // 
// Purpose: check the options from input string                 //
//////////////////////////////////////////////////////////////////

void CheckOption(int* option, int argc, char* argv[])
{
    int c = 0;
    while((c = getopt(argc, argv, "alhSr")) != -1)
    {
        switch(c)
        {
            case 'a':   // with -a option
                *option |= aFLAG;
                break;
            case 'l':   // with -l option
                *option |= lFLAG;
                break;
            case 'h':   // with -h option
                *option |= hFLAG;
                break;
            case 'S':   // with -s option
                *option |= SFLAG;
                break;
            case 'r':   // with -r option
                *option |= rFLAG;
                break;
            case '?':   // unknown option
                printf("unknown option character\n");
                break;
        }
    }
}


//////////////////////////////////////////////////////////////////
// changeDIR                                                    // 
// ============================================================ // 
// Input:   char*   -> origin directory                         //
//          char*   -> destination directory                    //
// Output:  void                                                // 
// Purpose: change the directory                                //
//////////////////////////////////////////////////////////////////

void changeDIR(char* from, char* to)
{
    getcwd(from, 255);   // get current working directory
    chdir(to);           // change the working directory to newpath
}

//////////////////////////////////////////////////////////////////
// IsDIR                                                        // 
// ============================================================ // 
// Input:   char*   -> filename                                 //
// Output:  int     -> 1 directory                              //
//                     0 non-directory                          //
// Purpose: check whether file is directory                     //
//////////////////////////////////////////////////////////////////

int IsDIR(char* filename)
{
    struct stat buf;

    // Get the file stat information
    lstat(filename, &buf);

    // Check whether the file is directory
    if(S_ISDIR(buf.st_mode))  
        return 1;   // If it is directory, return 1(true)
    else
        return 0;   // Otherwise, return 0(false)

}


//////////////////////////////////////////////////////////////////
// list_up                                                      // 
// ============================================================ // 
// Input:   FILE*   -> file stream pointer                      //
//          char*   -> filename                                 //
//          DIR*    -> directory pointer                        //
//          int     -> option flag                              //
//          int     -> file type                                //
// Output:  void                                                //
// Purpose: print the information of file or directory          //
//////////////////////////////////////////////////////////////////

void list_up(FILE* file, char* filename, DIR* dir_p, int option, int type)
{
    //=============================  DIRECTORY   ==============================//
    if(type == 0) 
    {
        struct dirent*  pEntry = NULL;  // directory entry 
        int             idx       = 0;  // sorted_list index
        linkedlist*     dir_entry = (linkedlist*)malloc(sizeof(linkedlist));  // file entry list

        //------------ change working directory -----------//
        char oldpath[256] = {0, };
        char newpath[256] = {0, };

        strncpy(newpath, filename, 256);
        changeDIR(oldpath, newpath);
        //-------------------------------------------------//

        //----- read the directory entry until the end -----//
        init(dir_entry);    // initialize the list
        while((pEntry = readdir(dir_p))!= NULL)
        {
            // Without -a option, ignore the hidden file
            if(((option & aFLAG) == 0) && !strncmp(pEntry->d_name, ".", 1))
                continue;
            // if file name was 'html_ls.html'
            else if(!strncmp(pEntry->d_name, "html_ls.html", 12))
                continue;
            // with -a option, include the hidden file
            else 
            {
                insert(dir_entry, pEntry->d_name);      // insert in the list
                idx++;                                  // index increase
            }
        }
        //--------------------------------------------------//

        //-----------   sort the list   ----------//
        if(option & SFLAG)   // with -S option
        {
            if(option & rFLAG)
            {
                Filename_Sort(dir_entry, 1);    // sort in reverse
                Filesize_Sort(dir_entry, 1);    // sort in reverse
            }
            else
                Filesize_Sort(dir_entry, 0);
        }
        else                 // without -S option
        {
            if(option & rFLAG)
                Filename_Sort(dir_entry, 1);    // sort in reverse
            else
                Filename_Sort(dir_entry, 0);
        }
        //----------------------------------------//

        if(option & lFLAG)           // with -l option, print path and total blocks
            get_path_total(file, dir_entry);
        print_entry(file, dir_entry, option);     // print the entry information
        changeDIR(newpath, oldpath);              // change to origin
    }
    //==========================================================================//

    //============================= REGULAR FILE ===============================//
    else if(type == 1)
    {
        // Without -a option, ignore the hidden file that starts with '.'
        if(((option & aFLAG) == 0) && !strncmp(filename, ".", 1))
            return;
        // if file name was 'html_ls.html'
        else if(strstr(filename, "html_ls.html") != NULL)
            return;
        // With -a option, include the hidden file
        else 
            print_detail(file, filename, 1, option);
    }
    //==========================================================================//
}


//////////////////////////////////////////////////////////////////
// get_path_total                                               // 
// ============================================================ // 
// Input:   FILE*           -> file stream pointer              //
//          linkedlist*     -> directory entry list             //
// Output:  void                                                //
// Purpose: print the directory path and total blocks           //
//////////////////////////////////////////////////////////////////

void get_path_total(FILE* file, linkedlist* dir_entry)
{
    struct  stat buf;               // stat buffer
    int     idx = 0;                // index for iterating
    int     total = 0;              // directory total block count
    char    cwd[256] = {0, };       // current working directory path
    char    fpath[256] = {0, };     // directory entry path
    struct  dirent* dir = NULL;     // directory entry pointer

    //===== get directory path =====//
    getcwd(cwd, 256);
    //==============================//

    //===== get total block size =====//
    for(; idx < dir_entry->size; idx++)
    {
        strncpy(fpath, cwd, 256);                         // copy the current working directory path
        strcat(fpath, "/");                               // add '/'
        strcat(fpath, getNodeat(dir_entry, idx)->data);   // add filename

        //------ get file stat information -----//
        if(lstat(fpath, &buf) < 0)
        {
            fprintf(file, "ERROR: %s<br>", strerror(errno));
            return;
        }
        //--------------------------------------//

        total += buf.st_blocks; // add block count
    }
 
    total /= 2; // convert to 1K blocks
    //================================//


    fprintf(file, "<b>Directory path: %s</b><br>", cwd);
    fprintf(file, "<b>total : %d</b><br>", total);
}


//////////////////////////////////////////////////////////////////
// get_TypeandPerm                                              // 
// ============================================================ // 
// Input:   char*           -> formatted string                 //
//          struct stat*    -> stat buffer                      //
// Output:  void                                                //
// Purpose: get the file type and permission information        //
//////////////////////////////////////////////////////////////////

void get_TypeandPerm(char* str, struct stat* buf)
{

    //=======   file type   =======//
    if(S_ISREG(buf->st_mode))        // regular file
        str[0] = '-';
    else if(S_ISDIR(buf->st_mode))   // directory
        str[0] = 'd';
    else if(S_ISCHR(buf->st_mode))   // character special
        str[0] = 'c';
    else if(S_ISBLK(buf->st_mode))   // block special
        str[0] = 'b';
    else if(S_ISFIFO(buf->st_mode))  // FIFO
        str[0] = 'p';
    else if(S_ISLNK(buf->st_mode))   // symbolic link
        str[0] = 'l';
    else if(S_ISSOCK(buf->st_mode))  // socket
        str[0] = 's';
    //=============================//

    //=======   permission  =======//
    //----- user -----//
    if(buf->st_mode & S_IRUSR)   // user read
        str[1] = 'r';
    if(buf->st_mode & S_IWUSR)   // user write
        str[2] = 'w';
    if(buf->st_mode & S_IXUSR)   // user execute
        str[3] = 'x';
    //----------------//

    //---- group ----//
    if(buf->st_mode & S_IRGRP)   // group read
        str[4] = 'r';
    if(buf->st_mode & S_IWGRP)   // group write
        str[5] = 'w';
    if(buf->st_mode & S_IXGRP)   // group execute
        str[6] = 'x';
    //---------------//

    //---- other ----//
    if(buf->st_mode & S_IROTH)   // other read
        str[7] = 'r';
    if(buf->st_mode & S_IWOTH)   // other write
        str[8] = 'w';
    if(buf->st_mode & S_IXOTH)   // other execute
        str[9] = 'x';
    //---------------//

    //-- special bit --//
    if(buf->st_mode & S_ISUID)   // set user id
        str[3] = 's';
    if(buf->st_mode & S_ISGID)   // set group idx
        str[6] = 's';
    if(buf->st_mode & S_ISVTX)   // sticky bit
        str[9] = 't';
    //-----------------//
    //=============================//
}



//////////////////////////////////////////////////////////////////
// get_hrFSize                                                  // 
// ============================================================ // 
// Input:   char*  -> formatted string                          //
//          long   -> file size                                 //
// Output:  void                                                //
// Purpose: get the file size in human readable string          //
//////////////////////////////////////////////////////////////////

void get_hrFSize(char* str, long filesize)
{
    int kilo = 1024;
    int mega = 1024 * kilo;
    int giga = 1024 * mega;

    double result = 0;
    
    
    if((filesize/giga) >= 1)        // divide by 1G
    {
        int remainder = 0;   // check whether remainder occur

        result = (double)filesize/giga;
        if(filesize % giga)
            remainder = 1;

        if(result < 10.0)    // if result is 1 digit
                sprintf(str, "%.1fG", result);
        else                 // more than 2 digit
        {
            if(remainder)    // if has remainder increase 1
                result += 1;    
            sprintf(str, "%dG", (int)result);
        }
    }
    else if((filesize/mega) >= 1)   // divide by 1M
    {
        int remainder = 0;   // check wherher remainder occur
        
        result = (double)filesize/mega;
        if(filesize % mega)
            remainder = 1;

        if(result < 10.0)    // if result is 1 digit 
                sprintf(str, "%.1fM", result);
        else                 // more than 2 digit
        {
            if(remainder)    // if has remainder increase 1
                result += 1;
            sprintf(str, "%dM", (int)result);
        }
    }
    else if((filesize/kilo) >= 1)   // divide by 1K
    {
        int remainder = 0;   // check whether remainder occur

        result = (double)filesize/kilo;
        if(filesize % kilo)
            remainder = 1;

        if(result < 10.0)    // if result is 1 digit
            sprintf(str, "%.1fK", result);
        else                 // more than 2 digit
        {
            if(remainder)    // if has remainder increase 1
                result += 1;
            sprintf(str, "%dK", (int)result);
        }
    }
    else    // less than 1K
    {
        result = (double)filesize;
        sprintf(str, "%.0f", result);
    }
}



//////////////////////////////////////////////////////////////////
// print_detail                                                 // 
// ============================================================ // 
// Input:   FILE*   -> file stream pointer                      //
//          char*   -> filename                                 //
//          int     -> non-directory path                       //
//          int     -> option flag                              //
// Output:  void                                                //
// Purpose: print the detail information                        //
//////////////////////////////////////////////////////////////////

void print_detail(FILE* file, char* filename, int nondir, int option)
{
    struct stat buf;            // stat buffer
    struct tm*  tmp;            // time structure pointer 
    char type_perm[11] = {0, }; // file type and permission info
    char timestr[25] = {0, };   // time info buffer
    char cwd[256] = {0, };      // current working directory path
    char fpath[256] = {0, };    // current directory entry path


    if(nondir)  // if non-directory file
    {
        char* abs = strchr(filename, '/');

        if(abs == NULL) // if filename was relative path
        {
            getcwd(cwd, 256);
            strncpy(fpath, cwd, 255);    // copy the path
            strcat(fpath, "/");          // add '/'
            strcat(fpath, filename);     // get current directory entry path
        }
        else            // if filename was absolute path
            strncpy(fpath, filename, 255);      // copy the absolute path to e_path
    }
    else        // if directory file
    {
        getcwd(cwd, 255);            // get current working directory
        strncpy(fpath, cwd, 255);    // copy the path
        strcat(fpath, "/");          // add '/'
        strcat(fpath, filename);     // get current directory entry path
    }

    //===== get file stat information =====//
    if(lstat(fpath, &buf) < 0)
    {
        fprintf(file, "ERROR: %s<br>", strerror(errno));
        return;
    }
    //=====================================//

    //===== select the color of string =====//
    if(S_ISDIR(buf.st_mode))
        fprintf(file, "<tr style=\"color:blue\">");
    else if(S_ISLNK(buf.st_mode))
        fprintf(file, "<tr style=\"color:green\">");
    else
        fprintf(file, "<tr style=\"color:red\">");
    //======================================//

    //======== print the file name ========//
    if(S_ISLNK(buf.st_mode))    // if file was symbolic link
    {
        char src[256] = {0, };
        readlink(fpath, src, 256); // read the link
        fprintf(file, "<td><a href=\"%s\">%s -> %s</a></td>", fpath, filename, src);  
    } 
    else
        fprintf(file, "<td><a href=\"%s\">%s</a></td>", fpath, filename);

    if((option & lFLAG) == 0) // without -l option, return immediately
    {
        fprintf(file, "</tr>\n");
        return;
    }
    //=====================================//

    //==========    get type perm   ==========//
    memset(type_perm, '-', sizeof(char)*10); // init with '-' except null character
    get_TypeandPerm(type_perm, &buf);
    //========================================//

    //==========    get time info   ==========//
    tmp = localtime(&buf.st_mtime);
    strftime(timestr, 25, "%b %d %R", tmp);
    //========================================//


    fprintf(file, "<td>%s</td>", type_perm);                          // file type and permission info
    fprintf(file, "<td>%ld</td>", (long)buf.st_nlink);                // hard link count
    fprintf(file, "<td>%s</td>", getpwuid(buf.st_uid)->pw_name);      // user name
    fprintf(file, "<td>%s</td>", getgrgid(buf.st_gid)->gr_name);      // group name
    
    if(option & hFLAG)   // if -h option is exist
    {
        char fsize[10] = {0, };
        get_hrFSize(fsize, (long)buf.st_size);
        fprintf(file, "<td>%s</td>", fsize);               // human readable size
    }
    else
        fprintf(file, "<td>%ld</td>", (long)buf.st_size);  // file size

    fprintf(file, "<td>%s</td>", timestr);                 // modified time
    fprintf(file, "</tr>\n");
}


//////////////////////////////////////////////////////////////////
// print_entry                                                  // 
// =============================================================// 
// Input:   FILE*       -> file stream pointer                  //
//          linkedlist* -> entry list                           // 
//          int         -> option flag                          // 
// Output:  void                                                //
// Purpose: Print the list that passed by argument              //
//////////////////////////////////////////////////////////////////

void print_entry(FILE* file, linkedlist* ll, int option)
{
    int i;
    int size = ll->size;

    if((option & lFLAG) == 0)  // without -l option
    {
        if(size != 0)
            fprintf(file, "<tr><th>Name</th></tr>\n");
        for(i=0; i<size; i++)   
           print_detail(file, getNodeat(ll, i)->data, 0, option);   // print only filename
    }
    else            // with -l option
    {
        if(size != 0)
            fprintf(file, "<tr><th>Name</th><th>Permission</th><th>Link</th><th>Owner</th><th>Group</th><th>Size</th><th>Last modified</th></tr>\n");
        for(i=0; i<size; i++)
            print_detail(file, getNodeat(ll, i)->data, 0, option);   // print detail information
    }
}


//////////////////////////////////////////////////////////////////
// IsWildCard                                                   // 
// ============================================================ // 
// Input:   char*   -> pathname                                 //
// Output:  int     -> 1 wildcard pathname                      //
//                     0 non-wildcard pathname                  //
// Purpose: check whether pathname has wildcard                 //
//////////////////////////////////////////////////////////////////

int IsWildCard(char* pathname)
{
    char* asterisk = NULL;      // '*' char
    char* question = NULL;      // '?' char
    char* left_braket  = NULL;  // '[' char
    char* right_braket = NULL;  // ']' char

    //=====     check the character     =====//

    asterisk = strchr(pathname, '*');
    if(asterisk != NULL)    // found '*'
        return 1;

    question = strchr(pathname, '?');
    if(question != NULL)    // found '?'
        return 1;

    left_braket = strchr(pathname, '[');
    right_braket = strchr(pathname, ']');
    if((left_braket != NULL) && (right_braket != NULL)) // found '[' and ']'
        return 1;
        
    //=======================================//

    return 0;   // otherwise, return 0
}


//////////////////////////////////////////////////////////////////
// extract_WildCard                                             // 
// ============================================================ // 
// Input:   char*   -> pathname                                 //
//          char*   -> new working dircetory path               //
//          char*   -> wildcard pattern                         //
// Output:  void                                                //
// Purpose: parsing the string(passed by parameter)             //
//////////////////////////////////////////////////////////////////

void extract_WildCard(char* pathname, char* newpath, char* pattern)
{
    char* temp = NULL;      // last '/' position
    char* walker = NULL;    // pointer for iterating

    walker = strchr(pathname, '/');
    while(walker != NULL)
    {
        temp = walker;
        walker = strchr(walker + 1, '/');   // search next '/'
    }

    memcpy(pattern, temp+1, strlen(temp)-1);                    // get the wildcard pattern
    memcpy(newpath, pathname, strlen(pathname) - strlen(pattern)); // get the new working directory path

    // convert absolute path '~' to $HOME
    if(!strncmp(newpath, "~", 1))
    {
        struct passwd* pw = getpwuid(getuid()); // get user ID
        char *homedir = pw->pw_dir;             // get home directory path

        strncpy(newpath, homedir, 256);         // copy to newpath
        strcat(newpath, "/");                   // add '/'
    }
}


//////////////////////////////////////////////////////////////////
// print_WildCard                                               // 
// ============================================================ // 
// Input:   FILE*   -> file stream pointer                      //
//          char*   -> pathname                                 //
//          int     -> option                                   //
// Output:  int     -> success or not                           //
// Purpose: print the list of directory that passed by wildcard //
//////////////////////////////////////////////////////////////////

int print_WildCard(FILE* file, char* pathname, int option)
{ 
    DIR*            dir_p   = NULL;
    struct dirent*  pEntry  = NULL;
    char* abs = strchr(pathname, '/');  // find '/' in pathname
    char newpath[256] = {0, };
    char oldpath[256] = {0, };

    linkedlist* File        = (linkedlist*)malloc(sizeof(linkedlist));  // File name list
    linkedlist* Directory   = (linkedlist*)malloc(sizeof(linkedlist));  // Directory name list

    init(File);
    init(Directory);    // initialize the list

    //================       classify the entry       ================//

    //-------   if there is no '/', it is relative path   -------//
    if(abs == NULL)
    {
        dir_p = opendir(".");
        while((pEntry = readdir(dir_p)) != NULL)
        {
            char* filename = pEntry->d_name;
            
            if(!strncmp(pEntry->d_name, "html_ls.html", 12))
                continue;   // if filename was 'html_ls.html', skip that file

            if(!fnmatch(pathname, filename, FNM_PERIOD))
            {
                // pattern match
                if(IsDIR(filename))
                    insert(Directory, filename);    // append at File list
                else
                    insert(File, filename);         // append at Directory list
            }
        }
        closedir(dir_p);
    }

    //------    otherwise, it is absolute pathname     -----//
    else 
    {
        char pattern[256] = {0, };  // wildcard pattern

        extract_WildCard(pathname, newpath, pattern); // parse the wildcard
      
        dir_p = opendir(newpath);  // open the directory 
        if(dir_p == NULL)   // open() fail
        {
            //fprintf(file, "cannot access '%s' : No such directory<br>", pathname);
            return -1;
        }
 
        changeDIR(oldpath, newpath);    // change the working directory
        
        while((pEntry = readdir(dir_p)) != NULL)
        {
            char* filename = pEntry->d_name;

            if(!strncmp(pEntry->d_name, "html_ls.html", 12))
                continue;   // if filename was 'html_ls.html', skip that file

            if(!fnmatch(pattern, filename, FNM_PERIOD))
            {
                // pattern match
                char filepath[256] = {0, };         // buffer for file's pathname
                strncpy(filepath, newpath, 256);    // copy the new pathname
                strcat(filepath, filename);         // add filename

                if(IsDIR(filepath))
                    insert(Directory, filepath);    // append at File list
                else
                    insert(File, filepath);         // append at Directory list
            }
        }

        changeDIR(newpath, oldpath);    // change the working directory to origin
        closedir(dir_p);                // close the directory
    }
    //------------------------------------------------------//

    //===============================================================//
    
    //=====================    sort the list    =====================//
    if(option & rFLAG)  // with -r option
    {
        Filename_Sort(File, 1);         // sort in reverse
        if(option & SFLAG)
            Filesize_Sort(File, 1);
        Filename_Sort(Directory, 1);    // sort in reverse
    }
    else                // otherwise
    {
        if(option & SFLAG)
            Filesize_Sort(File, 0);
        else
            Filename_Sort(File, 0);
        Filename_Sort(Directory, 0);
    }
    //===============================================================//

    //=================       print the list       ==================//

    //------    non-directory file list    ------//
    if(!IsEmpty(File))
    {
        int idx;
        int lsize = File->size;
         
        fprintf(file, "<p>\n<table border=\"1\">\n");
        if(option & lFLAG)  // with -l option
            fprintf(file, "<tr><th>Name</th><th>Permission</th><th>Link</th><th>Owner</th><th>Group</th><th>Size</th><th>Last modified</th></tr>\n");
        else                // without -l option
            fprintf(file, "<tr><th>Name</th></tr>\n");
       
        for(idx=0; idx<lsize; idx++)
        {
            node* n = getNodeat(File, idx);
            list_up(file, n->data, NULL, option, 1);        // print directory entries
        }
        fprintf(file, "</table><br>\n</p>\n");
    }
    //-------------------------------------------//

    //-------     directory file list     -------//
    if(!IsEmpty(Directory))
    {
        int idx;
        int lsize = Directory->size;
        for(idx=0; idx<lsize; idx++)
        {
            node* n = getNodeat(Directory, idx);

            if(option & lFLAG)  // with -l option
            {
                fprintf(file, "<p>\n<table border=\"1\">\n");
                dir_p = opendir(n->data);
                list_up(file, n->data, dir_p, option, 0);   // print directory entries
                closedir(dir_p);                      // close directory
            }
            else    // without -l option
            {
                fprintf(file, "<b>Directory path: %s</b><br>", n->data);  // print directory path
                fprintf(file, "<p>\n<table border=\"1\">\n");
                dir_p = opendir(n->data);
                list_up(file, n->data, dir_p, option, 0);     // print directory entries
                closedir(dir_p);
            }
            fprintf(file, "</table><br>\n</p>\n");
        }
    }
    //-------------------------------------------//

    //--------     if nothing found     --------//
    if(IsEmpty(File) && IsEmpty(Directory))
        return -1;
    //------------------------------------------//
   
    //================================================================//

    return 0;
}


