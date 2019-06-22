
//////////////////////////////////////////////////////////////////////////////////////////////
// File name        : srv.c                                                                 // 
// Date             : 2019/06/06                                                            // 
// OS               : Ubuntu 16.04 LTS 64bits                                               // 
// Author           : Sohn Jeong Hyeon                                                      // 
// Student ID       : 2014722068                                                            // 
// -----------------------------------------------------------------------------------------//
// Title        :   System Programming Assignment #4-3 (web server)                         //
// Description  :   This is server program for browser client.                              //
//                  It shows the directory entries of server environment                    //
//////////////////////////////////////////////////////////////////////////////////////////////


#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <wait.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>

#include "ls.h"


#define URL_LEN     256
#define BUFSIZE     1024
#define PORTNO      40309
#define MEMSIZE     1024    // shared memory size  


static int      sd_GLOBAL; // socket descriptor for signal handler
static pid_t*   pids; // keep child's pid
static int      CurChilds = 0;  // current child(use for parent process)
static int      InitEnd = 1;    // init or end state of server program(use for parent process)


void MainRoutines(int client_fd, int server_fd, char* requested_url); // main routines of server

void getDateTime(char* buf); // get date time format string
int  SetConfig(void);                                           // set config
int  CheckIPPerm(char* IP);                                     // IP permisson check
void AccessDenied(int client_fd, char* IP);                     // access denied
void NotFound(int client_fd, char* url);                        // send 404 Not Found
void TextandImage(int client_fd, char* url, char* MIMETYPE);    // send image&text related message

void sig_int(int signo);    // interrupt signal handler
void sig_alarm(int signo);  // alarm signal handler
void sig_child(int signo);  // child signal handler
void sig_usr1(int signo);   // user-defined signal handler 1
void sig_usr2(int signo);   // user-defined signal handler 2
void sig_term(int signo);   // termination signal handler

pid_t   child_make(int server_fd);  // make child
void    child_main(int server_fd);  // child main routines

static int MaxChilds;      // max child process
static int MaxIdleNum;     // max idle process
static int MinIdleNum;     // min idle process
static int StartServers;   // child process count for create in init
static int MaxHistory;     // max history


void* InitShared(void* arg);    // initialize the shared memory
void* InsertTable(void* arg);   // insert new process in working table
void* UpdateTable(void* arg);   // update the process idle status
void* CheckTable(void* arg);    // check idle process and return that pid
void* GetIdle(void* arg);       // get idle count in SHARED MEMORY
void* SetIdle(void* arg);       // set idle count in SHARED MEMORY
void* GetHistory(void* arg);    // get History record in SHARED MEMORY
void* SetHistory(void* arg);    // set History record in SHARED MEMORY
void* CommitLog(void* log);     // write the log at 'server_log.txt'

const char* logfile = "server_log"; // log file name

pthread_mutex_t TBL_MUTEX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t IDL_MUTEX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t HIS_MUTEX = PTHREAD_MUTEX_INITIALIZER;   // initialize the mutex

typedef struct ClientInfo {
    char    ip[16];
    int     pid;
    int     port;
    char    access_time[26];
} client;   // 50 byte

typedef struct HistoryInfo {
    char    history[60];
} history;  // display string

typedef struct ProcessInfo {
    pid_t   pid;    // process
    int     idle;   // 1 : idle 0 : on working
} Process;

typedef struct SharedMemory {
    int         ExistChilds;        // total child process count
    int         idle_count;         // idle process count
    Process     WorkingTable[10];   // Process list equal with MaxChilds
    int         History_count;      // current history count
    history     History[10];        // history list equal with MaxHistory
} shm;

struct IdleInfo {
    int type;       // request type
    int from_child; // is this request from child
};



//////////////////////////////////////////////////////////////////
// main                                                         // 
// ============================================================ // 
// Input:   void                                                //
// Output:  void                                                // 
// Purpose: send the response message to the client             //
//////////////////////////////////////////////////////////////////

void main()
{
    pthread_t   tid, tidA;
    struct sockaddr_in  server_addr;
    int server_fd;  // server socket descriptor
    int idx;        
    int fd;         // open file - 'server_log'
    int opt = 1;    // socket option

    char buf[BUFSIZE] = {0, };  // buffer
    char log[BUFSIZE] = {0, };  // log

    //==========    create semaphore    ==========//
    sem_t* mysem = sem_open("40309", O_CREAT|O_EXCL, 0700, 1);
    sem_close(mysem);
    //============================================//

    //==========    create log file    ==========//
    fd = open(logfile, O_CREAT | O_TRUNC | O_RDWR, 0666);
    close(fd);
    //===========================================//

    //===========   display start time   ==========//
    getDateTime(buf);
    sprintf(log, "[%s] Server is started.\n", buf);
    pthread_create(&tidA, NULL, &CommitLog, &log);
    pthread_join(tidA, NULL);
    printf("%s", log);
    //=============================================//

    //==============    create server socket    ==============//
    if((server_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printf("server : Can't open stream socket\n");
        return;
    }
    sd_GLOBAL = server_fd;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // set the option
    //========================================================//

    //==========    create server socket address    ==========//
    memset(&server_addr, 0, sizeof(server_addr));       // initialize buffer
    server_addr.sin_family = AF_INET;                   // set address family
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // convert to network byte order
    server_addr.sin_port = htons(PORTNO);               // convert to network byte order
    //========================================================//

    //==========    bind the address and socket     ==========//
    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("server : can't bind local address\n");
        return;
    }
    //========================================================//

    //==========    read httpd.conf    ==========//
    if(SetConfig() == 0)
        return;
    //===========================================//

    listen(server_fd, MaxChilds);   // wait for client request connect...
    alarm(10);                      // start timer(generate SIGALRM every 10 sec)
    signal(SIGINT, sig_int);        // set signal handler func (SIGINT)
    signal(SIGALRM, sig_alarm);     // set signal handler func (SIGALRM)
    signal(SIGCHLD, sig_child);     // set signal handler func (SIGCHLD)
    signal(SIGUSR1, sig_usr1);      // set signal handler func (SIGUSR1)
    signal(SIGUSR2, sig_usr2);      // set signal handler func (SIGUSR2)

    // create child processes(MAX : MaxChilds)
    // and initialize the shared memory
    pthread_create(&tid, NULL, &InitShared, NULL);
    pthread_join(tid, NULL);

    InitEnd = 1;
    pids = (pid_t*)malloc(MaxChilds*sizeof(pid_t));
    for(idx = 0; idx < StartServers; idx++)
       pids[idx] = child_make(server_fd);
   
    while(1)
        pause();    // main server pause
}


//////////////////////////////////////////////////////////////////
// SetConfig                                                    // 
// ============================================================ // 
// Input:   void                                                //
// Output:  int ->  1 : successful  0 : unsuccessful            // 
// Purpose: read config file and set the variable               //
//////////////////////////////////////////////////////////////////

int SetConfig(void)
{
    FILE*   fp = fopen("httpd.conf", "r");
    if(!fp){
        printf("server : config file open error\n");
        return 0;
    }

    // read a file by line until end of file
    while(!feof(fp))
    {
        char*   phrase = NULL;
        char*   cnt = NULL;
        char    buf[BUFSIZE] = {0, };
        
        fgets(buf, BUFSIZE, fp);       // read one line
        buf[strlen(buf) - 1] = '\0';    // remove \n character
        if(buf[0] == 0)
            break;

        phrase = strtok(buf, ": ");     // parse phrase
        cnt = strtok(NULL, ": ");       // parse code

        if(strcmp(phrase, "MaxChilds") == 0)
            MaxChilds = atoi(cnt);
        else if(strcmp(phrase, "MaxIdleNum") == 0)
            MaxIdleNum = atoi(cnt);
        else if(strcmp(phrase, "MinIdleNum") == 0)
            MinIdleNum = atoi(cnt);
        else if(strcmp(phrase, "StartServers") == 0)
            StartServers = atoi(cnt);
        else if(strcmp(phrase, "MaxHistory") == 0)
            MaxHistory = atoi(cnt);
        else {
            fclose(fp);
            return 0;   // unknown command!
        }
    }
    
    fclose(fp);
    return 1;   // successfully return
}


//////////////////////////////////////////////////////////////////
// CommitLog                                                    // 
// ============================================================ // 
// Input:   void*   ->  log string (used by this func)          //
// Output:  void*   ->  NULL                                    // 
// Purpose: write the log at text file                          //
//////////////////////////////////////////////////////////////////

void* CommitLog(void* log)
{
    int fd;
    sem_t*  mysem = sem_open("40309", O_RDWR);  // open the semaphore

    sem_wait(mysem);    // get the key
    //----------    critical section    ----------//
    fd = open(logfile, O_WRONLY | O_APPEND);
    write(fd, log, strlen(log));
    close(fd);
    //--------------------------------------------//
    sem_post(mysem);    // put the key

    sem_close(mysem);   // close the semaphore

    return NULL;
}

//////////////////////////////////////////////////////////////////
// InitShared                                                   // 
// ============================================================ // 
// Input:   void*   ->  argument (used by this func)            //
// Output:  void*   ->  NULL                                    // 
// Purpose: initialize the shared memory                        //
//////////////////////////////////////////////////////////////////

void* InitShared(void* arg)
{
    int     shm_id;
    shm*    shm_addr;

    if((shm_id = shmget((key_t)PORTNO, MEMSIZE, IPC_CREAT|0666)) == -1)
    {
        printf("shmget failed!\n");
        return NULL;
    }

    if((shm_addr = (shm*)shmat(shm_id, (void*)0, 0)) == (void*)-1)
    {
        printf("shmat failed!\n");
        return NULL;
    }

    pthread_mutex_lock(&TBL_MUTEX);
    //----------    critical section    ----------//
    (*shm_addr).ExistChilds = 0;   // initialze the existing child
    //--------------------------------------------//
    pthread_mutex_unlock(&TBL_MUTEX);

    return NULL;
}


//////////////////////////////////////////////////////////////////
// InsertTable                                                  // 
// ============================================================ // 
// Input:   void*   ->  argument (used by this func)            //
// Output:  void*   ->  NULL                                    // 
// Purpose: insert new process info at process table            //
//////////////////////////////////////////////////////////////////

void* InsertTable(void* arg)
{
    int     insert_pos;
    int     shm_id;
    shm*    shm_addr;
    pid_t   pid = (*((pid_t*)arg));
    Process newprocess = {pid, 1};  // create new idle process

    if((shm_id = shmget((key_t)PORTNO, MEMSIZE, IPC_CREAT|0666)) == -1)
    {
        printf("shmget failed!\n");
        return NULL;
    }

    if((shm_addr = (shm*)shmat(shm_id, (void*)0, 0)) == (void*)-1)
    {
        printf("shmat failed!\n");
        return NULL;
    }

    pthread_mutex_lock(&TBL_MUTEX);
    //----------    critical section    ----------//
    insert_pos = (*shm_addr).ExistChilds;
    (*shm_addr).WorkingTable[insert_pos] = newprocess;
    (*shm_addr).ExistChilds = insert_pos + 1;   // increase existing child
    //--------------------------------------------//
    pthread_mutex_unlock(&TBL_MUTEX);

    return NULL;
}


//////////////////////////////////////////////////////////////////
// UpdateTable                                                  // 
// ============================================================ // 
// Input:   void*   ->  argument (used by this func)            //
// Output:  void*   ->  NULL                                    // 
// Purpose: update the process idle status                      //
//////////////////////////////////////////////////////////////////

void* UpdateTable(void* arg)
{
    int     idx;
    int     shm_id;
    int     total_childs;
    shm*    shm_addr;
    Process* target = (Process*)arg;

    if((shm_id = shmget((key_t)PORTNO, MEMSIZE, IPC_CREAT|0666)) == -1)
    {
        printf("shmget failed!\n");
        return NULL;
    }

    if((shm_addr = (shm*)shmat(shm_id, (void*)0, 0)) == (void*)-1)
    {
        printf("shmat failed!\n");
        return NULL;
    }

    pthread_mutex_lock(&TBL_MUTEX);
    //----------    critical section    ----------//
    // find the target process and change idle status
    total_childs = (*shm_addr).ExistChilds;
    for(idx = 0; idx < total_childs; idx++)
        if((*shm_addr).WorkingTable[idx].pid == target->pid) 
            (*shm_addr).WorkingTable[idx].idle = target->idle;
    //--------------------------------------------//
    pthread_mutex_unlock(&TBL_MUTEX);

    return NULL;
}


//////////////////////////////////////////////////////////////////
// CheckTable                                                   // 
// ============================================================ // 
// Input:   void*   ->  argument (used by this func)            //
// Output:  void*   ->  NULL                                    // 
// Purpose: check the idle process                              //
//          and return that pid(/w remove from process table    //
//////////////////////////////////////////////////////////////////

void* CheckTable(void* arg)
{
    int     idx;
    int     shm_id;
    int     total_childs;
    shm*    shm_addr;
    pid_t*  idle_id = (pid_t*)arg;

    if((shm_id = shmget((key_t)PORTNO, MEMSIZE, IPC_CREAT|0666)) == -1)
    {
        printf("shmget failed!\n");
        return NULL;
    }

    if((shm_addr = (shm*)shmat(shm_id, (void*)0, 0)) == (void*)-1)
    {
        printf("shmat failed!\n");
        return NULL;
    }

    pthread_mutex_lock(&TBL_MUTEX);
    //----------    critical section    ----------//
    // find the idle process
    total_childs = (*shm_addr).ExistChilds;
    for(idx = 0; idx < total_childs; idx++) {
        if((*shm_addr).WorkingTable[idx].idle == 1) {
            *idle_id = (*shm_addr).WorkingTable[idx].pid;
            break;
        }
    }
    // remove that process from Working Table list
    for( ; idx < total_childs - 1; idx++)
        (*shm_addr).WorkingTable[idx] = (*shm_addr).WorkingTable[idx + 1];
    
    (*shm_addr).ExistChilds -= 1;   // decrease existing child
    //--------------------------------------------//
    pthread_mutex_unlock(&TBL_MUTEX);

    return NULL;
}


//////////////////////////////////////////////////////////////////
// GetIdle                                                      // 
// ============================================================ // 
// Input:   void*   ->  argument (used by this func)            //
// Output:  void*   ->  NULL                                    // 
// Purpose: get idle process count                              //
//////////////////////////////////////////////////////////////////

void* GetIdle(void* arg)
{
   int* idle = (int*)arg;
   int   shm_id;
   shm* shm_addr;   // typecast

   if((shm_id = shmget((key_t)PORTNO, MEMSIZE, IPC_CREAT|0666)) == -1)
   {
       printf("shmget failed!\n");
       return NULL;
   }

   if((shm_addr = (shm*)shmat(shm_id, (void*)0, 0)) == (void*)-1)
   {
       printf("shmat fail!\n");
       return NULL;
   }

   pthread_mutex_lock(&IDL_MUTEX);
   //----------    critical section    ----------//
   *idle = (*shm_addr).idle_count;  // get idle count from memory
   //--------------------------------------------//
   pthread_mutex_unlock(&IDL_MUTEX);

   return NULL;
}


//////////////////////////////////////////////////////////////////
// SetIdle                                                      // 
// ============================================================ // 
// Input:   void*   ->  argument (used by this func)            //
// Output:  void*   ->  NULL                                    // 
// Purpose: set idle process count                              //
//////////////////////////////////////////////////////////////////

void* SetIdle(void* arg)
{
   int  type = ((struct IdleInfo*)arg)->type;               // parse arg type
   int  from_child = ((struct IdleInfo*)arg)->from_child;   // parse arg from
   int  shm_id;     // shared memory id
   shm* shm_addr;   // typecast

   char buf[BUFSIZE] = {0, };
   char log[BUFSIZE] = {0, };
   int  idle;
   pthread_t    tid;


   if((shm_id = shmget((key_t)PORTNO, MEMSIZE, IPC_CREAT|0666)) == -1)
   {
       printf("shmget failed!\n");
       return NULL;
   }

   if((shm_addr = (shm*)shmat(shm_id, (void*)0, 0)) == (void*)-1)
   {
       printf("shmat fail!\n");
       return NULL;
   }

   pthread_mutex_lock(&IDL_MUTEX);
   //----------     critical section    ----------//
   if(type == 1)
       (*shm_addr).idle_count += 1; // increase idle
   else if(type == -1)
       (*shm_addr).idle_count -= 1; // decrease idle
   
   idle = (*shm_addr).idle_count;

   getDateTime(buf);
   sprintf(log, "[%s] idleProcessCount : %d\n", buf, (*shm_addr).idle_count);
   pthread_create(&tid, NULL, &CommitLog, &log);
   pthread_join(tid, NULL);
   printf("%s", log);
   //---------------------------------------------//
   pthread_mutex_unlock(&IDL_MUTEX);

   if(from_child)
   {
       if(idle < MinIdleNum)
           kill(getppid(), SIGUSR1);
       else if(MaxIdleNum < idle)
           kill(getppid(), SIGUSR2);
   }
   else
   {
       if(idle < MinIdleNum)
           raise(SIGUSR1);
       else if(MaxIdleNum < idle)
           raise(SIGUSR2);
   }

   return NULL;
}


//////////////////////////////////////////////////////////////////
// GetHistory                                                   // 
// ============================================================ // 
// Input:   void*   ->  argument (used by this func)            //
// Output:  void*   ->  NULL                                    // 
// Purpose: get connection history info                         //
//////////////////////////////////////////////////////////////////

void* GetHistory(void* arg)
{
    char    buf[BUFSIZE] = {0, };
    int     idx;
    int     NO = 1;
    int     shm_id;
    shm*    shm_addr;   // typecast

    if((shm_id = shmget((key_t)PORTNO, MEMSIZE, IPC_CREAT|0666)) == -1)
    {
        printf("shmget failed!\n");
        return NULL;
    }

    if((shm_addr = (shm*)shmat(shm_id, (void*)0, 0)) == (void*)-1)
    {
        printf("shmat fail!\n");
        return NULL;
    }

    pthread_mutex_lock(&HIS_MUTEX);
    //----------    critical section    ----------//
    for(idx = (*shm_addr).History_count-1; idx >= 0; idx--, NO++)
        printf("%d\t%s\n", NO, (*shm_addr).History[idx].history);
    //--------------------------------------------//
    pthread_mutex_unlock(&HIS_MUTEX);

    return NULL;
}


//////////////////////////////////////////////////////////////////
// SetHistory                                                   // 
// ============================================================ // 
// Input:   void*   ->  argument (used by this func)            //
// Output:  void*   ->  NULL                                    // 
// Purpose: set connection history info                         //
//////////////////////////////////////////////////////////////////

void* SetHistory(void* arg)
{
    client* cli = (client*)arg;
    char    buf[BUFSIZE] = {0, };
    int     shm_id;
    shm*    shm_addr;   // typecast

    if((shm_id = shmget((key_t)PORTNO, MEMSIZE, IPC_CREAT|0666)) == -1)
    {
        printf("shmget failed!\n");
        return NULL;
    }

    if((shm_addr = (shm*)shmat(shm_id, (void*)0, 0)) == (void*)-1)
    {
        printf("shmat fail!\n");
        return NULL;
    }

    // Create History string
    sprintf(buf, "%s\t%d\t%d\t%s", cli->ip, cli->pid, cli->port, cli->access_time);

    pthread_mutex_lock(&HIS_MUTEX);
    //----------    critical section    ----------//
    // if reach to MaxHistory
    if(MaxHistory == (*shm_addr).History_count)
    {
        // move the memory
        memmove(&(*shm_addr).History[0], &(*shm_addr).History[1], sizeof(history)*9);
        // set history count
        (*shm_addr).History_count -= 1;
    }

    // set string
    strcpy((*shm_addr).History[(*shm_addr).History_count].history, buf);
    // set history count
    (*shm_addr).History_count += 1;
    //--------------------------------------------//
    pthread_mutex_unlock(&HIS_MUTEX);

    return NULL;
}


//////////////////////////////////////////////////////////////////
// getDateTime                                                  // 
// ============================================================ // 
// Input:   char* -> result buffer                              //
// Output:  void                                                // 
// Purpose: store current time at buffer                        //
//////////////////////////////////////////////////////////////////

void getDateTime(char* buf)
{
    time_t  t;
    time(&t);   // get current time
    strcpy(buf, ctime(&t)); // store time string to buf 
    buf[strlen(buf)-1] = 0; // remove \n
}


//////////////////////////////////////////////////////////////////
// child_make                                                   // 
// ============================================================ // 
// Input:   int -> server file discriptor                       //
// Output:  void                                                // 
// Purpose: make child process                                  //
//////////////////////////////////////////////////////////////////

pid_t child_make(int server_fd)
{
    pid_t   pid;
   
    ++CurChilds; // increase current child count
    if(CurChilds == StartServers)
        InitEnd = 0;
    
    // parent process
    if((pid = fork()) > 0)
    {
        struct IdleInfo arg = {1, 0};   // argument from parent
        char buf[BUFSIZE] = {0, };
        char log[BUFSIZE] = {0, };  // log message
        pthread_t   tidA, tidB, tidC;

        getDateTime(buf);   // get current time
        sprintf(log, "[%s] %d process is forked.\n", buf, pid);
        pthread_create(&tidC, NULL, &CommitLog, &log);
        pthread_join(tidC, NULL);
        printf("%s", log);  // print at terminal and text file

        pthread_create(&tidB, NULL, &SetIdle, &arg);
        pthread_join(tidB, NULL);   // set idle count
        
        pthread_create(&tidA, NULL, &InsertTable, &pid);
        pthread_join(tidA, NULL);   // insert created process in table

        return pid; // return child pid
    }

    // child process
    child_main(server_fd);  // run child main routines
}


//////////////////////////////////////////////////////////////////
// child_main                                                   // 
// ============================================================ // 
// Input:   int -> server socket discriptor                     //
// Output:  void                                                // 
// Purpose: child process main routines                         //
//////////////////////////////////////////////////////////////////

void child_main(int server_fd)
{
    Process     ps;        // argument for thread
    pthread_t   tidA, tidB, tidC, tidD, tidE;       // thread ID

    ps.pid = getpid(); // set arg
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, sig_term);  // set signal handler


    //=========================  CHILD  SERVER ROUTINES  =========================//
    while(1)
    {
        int     client_fd;      // socket descriptor connected with client
        int     len;            // client address struct size
        char    IP[16] = {0, }; // dotted decimal IP address
        char    cnt[BUFSIZE] = {0, };   // connected time
        char    dcnt[BUFSIZE] = {0, };  // disconnected time
        char    requested_url[URL_LEN] = {0, }; // requested url from client
        struct  in_addr         inet_client_address;   // client IPv4 address
        struct  sockaddr_in     client_addr;    // client address
        struct  timeval         accept_time;    // sec + micro second - accepted time
        struct  IdleInfo        arg;            // argument for SetIdle                
        struct  timeval         discon_time;    // sec + micro second - disconnected time
        long    udistime;   // micro sec - disconnected time
        long    uactime;    // micro sec - accepted time
        char    log[BUFSIZE] = {0, };   // log message

        client cli;     // structure of client info

        //----------    accept the client's request    ----------//
        len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &len);
        if(client_fd < 0) {
            printf("server : accept failed\n");
            return;
        }

        inet_client_address.s_addr = client_addr.sin_addr.s_addr;   // get IPv4 address
        strcpy(IP, inet_ntoa(inet_client_address));                 // copy the dotted decimal IP

        if(!CheckIPPerm(IP))    // if IP has no permission
        {
            AccessDenied(client_fd, IP);
            continue;
        }
        
        ps.idle = 0;   // set argument
        pthread_create(&tidC, NULL, &UpdateTable, &ps);
        pthread_join(tidC, NULL);  // create thread (update the working table)
        getDateTime(cnt);   // get connected time for history
        //-------------------------------------------------------//
        

        //----------    create client info      ----------//
        memset(cli.ip, 0, sizeof(cli.ip));
        memset(cli.access_time, 0, sizeof(cli.access_time));
        strcpy(cli.ip, IP);
        cli.pid = getpid();
        cli.port = client_addr.sin_port;
        strcpy(cli.access_time, cnt);
        //------------------------------------------------//       
        //----------    access shared memory    ----------//
        pthread_create(&tidB, NULL, &SetHistory, &cli); // update history
        pthread_detach(tidB);
        //------------------------------------------------//


        //----------    read & send message with client    ----------//
        gettimeofday(&accept_time, NULL);
        uactime = accept_time.tv_sec*1000000 + accept_time.tv_usec;     // start time

        MainRoutines(client_fd, server_fd, requested_url);  // rcv, snd with client
        
        gettimeofday(&discon_time, NULL);
        udistime = discon_time.tv_sec*1000000 + discon_time.tv_usec;    // end time
        //-----------------------------------------------------------//

        sprintf(log,
                "\n============= New Client =============\n"
                "TIME : [%s]\n"
                "URL : %s\n"
                "IP : %s\n"
                "PORT : %d\n"
                "PID : %d\n"
                "======================================\n\n",
                cnt, requested_url, IP, client_addr.sin_port, getpid());

        pthread_create(&tidD, NULL, &CommitLog, &log);
        pthread_join(tidD, NULL);
        printf("%s", log);
       
        arg.type = -1;
        arg.from_child = 1;
        pthread_create(&tidA, NULL, &SetIdle, &arg);    // decrease idle count
        pthread_join(tidA, NULL);   // wait for idle chandge theard quit

        sleep(5);   // sleep 5 sec

        getDateTime(dcnt);
        memset(log, 0, sizeof(log));    // reset the log
      
        sprintf(log,
                "\n========= Disconnected Client =========\n"
                "TIME : [%s]\n"
                "URL : %s\n"
                "IP : %s\n"
                "PORT : %d\n"
                "PID : %d\n"
                "CONNECTING TIME: %ld(us)\n"
                "=======================================\n\n",
                dcnt, requested_url, IP, client_addr.sin_port, getpid(), udistime - uactime);

        pthread_create(&tidE, NULL, &CommitLog, &log);
        pthread_join(tidE, NULL);
        printf("%s", log);
         

        //----------    access shared memory    ----------//
        arg.type = 1;
        arg.from_child = 1; // set argument from child
        ps.idle = 1;
       
        pthread_create(&tidC, NULL, &UpdateTable, &ps);
        pthread_create(&tidA, NULL, &SetIdle, &arg);    // increase idle
        pthread_join(tidC, NULL);
        pthread_join(tidA, NULL);
        //------------------------------------------------//

        close(client_fd); // close the connect with client
    }
}


//////////////////////////////////////////////////////////////////
// sig_usr1                                                     // 
// ============================================================ // 
// Input:   int -> signal                                       //
// Output:  void                                                // 
// Purpose: SIGUSR1 signal handler                              //
//////////////////////////////////////////////////////////////////

void sig_usr1(int signo) 
{
    // [request from child] increase child pool until 5
    if(!InitEnd)
    {
        int increase;
        int idle;
        int idx;

        pthread_t   tid;
        pthread_create(&tid, NULL, &GetIdle, &idle);
        pthread_join(tid, NULL);

        increase = 5 - idle;
        for(idx=0; idx < increase; idx++)
            if(CurChilds < MaxChilds)   // DO NOT EXCEED MAX CHILDS!
                pids[CurChilds] = child_make(sd_GLOBAL);
    }
}


//////////////////////////////////////////////////////////////////
// sig_usr2                                                     // 
// ============================================================ // 
// Input:   int -> signal                                       //
// Output:  void                                                // 
// Purpose: SIGUSR2 signal handler                              //
//////////////////////////////////////////////////////////////////

void sig_usr2(int signo)
{
    // [request from child] terminate the idle child process
    if(!InitEnd)
    {
        int decrease;
        int idle;
        int idx;

        pthread_t   tid;
        pthread_create(&tid, NULL, &GetIdle, &idle);
        pthread_join(tid, NULL);

        decrease = idle - 5;
        for(idx = 0; idx < decrease; idx++)
        {
            // find the idle process
            pid_t   target_child;
            pthread_create(&tid, NULL, &CheckTable, &target_child);
            pthread_join(tid, NULL);
            // kill idle process
            kill(target_child, SIGTERM);
            pause();
        }
    }
}

//////////////////////////////////////////////////////////////////
// sig_term                                                     // 
// ============================================================ // 
// Input:   int -> signal                                       //
// Output:  void                                                // 
// Purpose: SIGTERM signal handler                              //
//////////////////////////////////////////////////////////////////

void sig_term(int signo)
{   
    usleep(100*1000);   // 100ms delay
    exit(0);            // exit child process
}


//////////////////////////////////////////////////////////////////
// sig_int                                                      // 
// ============================================================ // 
// Input:   int -> signal                                       //
// Output:  void                                                // 
// Purpose: SIGINT signal handler                               //
//////////////////////////////////////////////////////////////////

void sig_int(int signo)
{
    int     idx;
    int     idle;
    int     shm_id;
    int     wait_count = 0;
    char    buf[BUFSIZE] = {0, };
    char    log[BUFSIZE] = {0, };
    shm*    shm_addr;
    pthread_t   tid;

    alarm(0);   // stop timer
    InitEnd = 1;    // end state
 
    if((shm_id = shmget((key_t)PORTNO, MEMSIZE, IPC_CREAT|0666)) == -1)
    {
        printf("shmget fail\n");
        return;
    }
    if((shm_addr = (shm*)shmat(shm_id, (void*)0, 0)) == (void*)-1)
    {
        printf("shmat fail!\n");
        return;
    }

    // wait until all childs become idle
    pthread_create(&tid, NULL, &GetIdle, &idle);    // get idle count
    pthread_join(tid, NULL);
    while(idle != CurChilds)
    {
        if(++wait_count >= MaxChilds)
            break;  // wait limit
        printf("\nWait for all client's request finish....\n");
        sleep(5);   // wait for client end...
        pthread_create(&tid, NULL, &GetIdle, &idle);    // get idle count
        pthread_join(tid, NULL);
    }

    // send SIGTERM to all child
    pthread_mutex_lock(&TBL_MUTEX);
    //----------    critical section    ----------//
    printf("\n");
    for(idx = 0; idx < (*shm_addr).ExistChilds; idx++) {
        pid_t   target_pid;
        target_pid = (*shm_addr).WorkingTable[idx].pid;
        kill(target_pid, SIGTERM);
        pause();    // wait until SIGCHLD occur...
    }
    //--------------------------------------------//
    pthread_mutex_unlock(&TBL_MUTEX);
        
    getDateTime(buf);   // get current time
    sprintf(log, "[%s] Server is terminated.\n", buf);
    pthread_create(&tid, NULL, &CommitLog, &log);
    printf("%s", log);
    pthread_join(tid, NULL); // print at terminal and log file
  
    //----------    remove shared memory    ----------//
    if(shmctl(shm_id, IPC_RMID, 0) == -1)
        printf("shmctl fail\n");
    //------------------------------------------------//

    close(sd_GLOBAL);       // close server socket
    sem_unlink("40309");    // destory named semaphore
    exit(0);    // process exit
}


//////////////////////////////////////////////////////////////////
// sig_child                                                    // 
// ============================================================ // 
// Input:   int -> signal                                       //
// Output:  void                                                // 
// Purpose: SIGCHLD signal handler                              //
//////////////////////////////////////////////////////////////////

void sig_child(int signo)
{
    pid_t   pid;    // child process id
    int     status; // child exit status
    char    buf[BUFSIZE] = {0, };   // terminated time
    char    log[BUFSIZE] = {0, };   // log message

    // get the termination status of child process
    while((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        struct IdleInfo arg = {-1, 0};  // arg from parent
        pthread_t   tidA, tidB;

        pthread_create(&tidA, NULL, &SetIdle, &arg);
        getDateTime(buf);   // get current time 
        sprintf(log, "[%s] %d process is terminated.\n", buf, pid);
        pthread_create(&tidB, NULL, &CommitLog, &log);
        printf("%s", log);
        pthread_join(tidB, NULL);
        pthread_join(tidA, NULL);
        CurChilds -= 1; // decrease current child count
    }
}


///////////////////////////////)//////////////////////////////////
// sig_alarm                                                    // 
// ============================================================ // 
// Input:   int -> signal                                       //
// Output:  void                                                // 
// Purpose: SIGALRM signal handler                              //
//////////////////////////////////////////////////////////////////

void sig_alarm(int signo)
{
    pthread_t tid;
    int idx;

    puts("\n\n========= Connection History =========");
    printf("NO.\tIP\t\tPID\tPORT\tTIME\n");

    pthread_create(&tid, NULL, GetHistory, NULL);
    pthread_join(tid, NULL);
    
    puts("======================================\n");
    alarm(10);  // set the alarm
}


//////////////////////////////////////////////////////////////////////////////
// MainRoutines                                                             // 
// ======================================================================== // 
// Input:   int     -> client socket descriptor                             //
//          int     -> server socket descriptor                             //
//          char*   -> root path                                            //
// Output:  void                                                            // 
// Purpose: server's main behavior                                          //
//////////////////////////////////////////////////////////////////////////////

void MainRoutines(int client_fd, int server_fd, char* requested_url)
{
    char    received_message[BUFSIZE]  = {0, };     // request message
    char    tmp[BUFSIZE]               = {0, };     // copied request message
        
    char    root_path[URL_LEN] = {0, };    // root path
    char    raw_url[URL_LEN]   = {0, };    // URL with . or ..
    char    url[URL_LEN]       = {0, };    // requested URL(without . or ..)
    char    method[20]         = {0, };    // requested method(GET, POST...)
    char*   tok = NULL;
    
    int     ls_argc = 0;    // argc for ls func
    char*   command[3];     // argv for ls func
    
    char*   MIME[3] = {"text/html", "text/plain", "image/*"};
    char*   MIMETYPE = NULL;

        
    //----------    get ROOT PATH   ----------//
    getcwd(root_path, URL_LEN);
    //----------------------------------------//

    //--------      read message from client      --------//      
    
    while(read(client_fd, received_message, BUFSIZE) > 0)
    {
        strcpy(tmp, received_message);  // copy the message

        //--------    parse the method and URL   --------//
        tok = strtok(tmp, " ");
        strcpy(method, tok);        // parse the method
        if(strcmp(method, "GET") == 0) {
            tok = strtok(NULL, " ");
            strcpy(raw_url, tok);
        }                           // parse the URL
        realpath(raw_url, url);     // convert . and .. to absolute path
        strcpy(requested_url, url); // copy url path for log

        if(strcmp(url, "/favicon.ico") == 0)
            continue;   // ignore favicon.ico request
        //-----------------------------------------------//

        //-----     check whether URL is exist      -----//
        if(access(url, F_OK) == -1)
        {
            NotFound(client_fd, url);    // send 404 ERROR
            continue;
        }
        //-----------------------------------------------//

        //---------     determine MIME type     ---------//
        if(fnmatch("*.html", url, FNM_CASEFOLD) == 0)
            MIMETYPE = MIME[0]; // text/html
        else if(fnmatch("*.jpg",  url, FNM_CASEFOLD) == 0 ||
                fnmatch("*.png",  url, FNM_CASEFOLD) == 0 ||
                fnmatch("*.jpeg", url, FNM_CASEFOLD) == 0)
            MIMETYPE = MIME[2]; // image/*
        else
        {
            struct stat buf;
            if(lstat(url, &buf) < 0) {
                printf("[lstat ERROR]\n");
                return;
            }

            // if file was directory, text/html. otherwise, text/plain
            if(S_ISDIR(buf.st_mode))
                MIMETYPE = MIME[0]; // text/html
            else
                MIMETYPE = MIME[1]; // text/plain
        }
        //-----------------------------------------------//


        //====================        text/html       ====================//
        if(!strcmp(MIMETYPE, "text/html"))
        {
            char pathname[URL_LEN] = {0, };

            //------    create ls command and make html file  ------//
            command[ls_argc++] = "ls.c";
            if(strcmp(url, "/") == 0 || strcmp(url, root_path) == 0)
                command[ls_argc++] = "-l";  // root path = -l option
            else    // otherwise, -al option
            {
                command[ls_argc++] = "-al";     // set the ls option
                strcpy(pathname, url);          // copy the url
                command[ls_argc++] = pathname;  // set the ls URL
            }
            //------------------------------------------------------//

            if(ls(ls_argc, command) == -1)    // unsuccessful
                NotFound(client_fd, url);
            else                              // successful
                TextandImage(client_fd, "html_ls.html", MIMETYPE);
        }
        //================================================================//

        //===============      text/plain and image/*     ================//
        else if(!strcmp(MIMETYPE, "text/plain") || !strcmp(MIMETYPE, "image/*"))
        {
            int fd = open(url, O_RDONLY);    // read system call
            if(fd < 0)  // unsuccessful
                NotFound(client_fd, url);
            else        // successful
                TextandImage(client_fd, url, MIMETYPE);
        }
        //================================================================//  
    }
}


//////////////////////////////////////////////////////////////////////////////
// CheckIPPerm                                                              // 
// ======================================================================== // 
// Input:   char*   -> requested IP                                         //
// Output:  int     -> whether IP has permission                            // 
// Purpose: check whether IP has access permission                          //
//////////////////////////////////////////////////////////////////////////////

int CheckIPPerm(char* IP)
{
    char    pattern[17] = {0, };
    FILE*   fp = fopen("accessible.usr", "r");
    if(!fp){
        printf("server : permission file open error\n");
        return 0;
    }

    // read a file by line until end of file
    while(!feof(fp))
    {
        fgets(pattern, 17, fp);              // read one line
        pattern[strlen(pattern) - 1] = '\0'; // remove \n character

        // pattern matching
        if(!fnmatch(pattern, IP, 0))
            return 1;   // if IP is match, return 1(has permission)
    }
   
    fclose(fp); // close file
    return 0;   // otherwise return 0(no permission)
}


//////////////////////////////////////////////////////////////////////////////
// AccessDenied                                                             // 
// ======================================================================== // 
// Input:   int     -> client socket descriptor                             //
//          char*   -> requested IP                                         //
// Output:  void                                                            // 
// Purpose: make the response message(403 Forbidden) and send to client     //
//////////////////////////////////////////////////////////////////////////////

void AccessDenied(int client_fd, char* IP)
{
    char response_header[BUFSIZE] = {0, };
    char response_message[BUFSIZE] = {0, };
    
    //  make the response message body
    sprintf(response_message,
            "<title>Access Denied</title>"
            "<h1>Access denied!</h1>"
            "<h2>Your IP : %s</h2>"
            "<p>You have no permission to access this web server.<br>"
            "HTTP 403.6 - Forbidden IP address reject</p>", IP);
    //  make the response header
    sprintf(response_header,
            "HTTP/1.0 403 Forbidden\r\n"
            "Server:2019 simple web server\r\n"
            "Content-length:%lu\r\n"
            "Content-type:%s\r\n\r\n", strlen(response_message), "text/html");  

    //----------    send the response message to client    ----------//
    write(client_fd, response_header, strlen(response_header));    // send message header
    write(client_fd, response_message, strlen(response_message));  // send message body
    //---------------------------------------------------------------//   
}


//////////////////////////////////////////////////////////////////////////////
// NotFound                                                                 // 
// ======================================================================== // 
// Input:   int     -> client socket descriptor                             //
//          char*   -> requestes URL                                        //
// Output:  void                                                            // 
// Purpose: make the response message(404 Not Found) and send to client     //
//////////////////////////////////////////////////////////////////////////////

void NotFound(int client_fd, char* url)
{
    char response_header[BUFSIZE] = {0, };
    char response_message[BUFSIZE] = {0, };
    
    //  make the response message body
    sprintf(response_message,
            "<h1>Not Found</h1>"
            "<p>The request URL %s was not found on this server<br>"
            "HTTP 404 - Not Page Found</p>", url);
    //  make the response header
    sprintf(response_header,
            "HTTP/1.0 404 Not Found\r\n"
            "Server:2019 simple web server\r\n"
            "Content-length:%lu\r\n"
            "Content-type:%s\r\n\r\n", strlen(response_message), "text/html");  

    //----------    send the response message to client    ----------//
    write(client_fd, response_header, strlen(response_header));    // send message header
    write(client_fd, response_message, strlen(response_message));  // send message body
    //---------------------------------------------------------------//   
}


//////////////////////////////////////////////////////////////////////////////
// TextandImage                                                             // 
// ======================================================================== // 
// Input:   int     -> client socket descriptor                             //
//          char*   -> requestes URL                                        //
// Output:  void                                                            // 
// Purpose: make the response message(Text and Image) and send to client    //
//////////////////////////////////////////////////////////////////////////////

void TextandImage(int client_fd, char* url, char* MIMETYPE)
{
    char response_header[BUFSIZE] = {0, };  // header buffer
    char response_message[BUFSIZE] = {0, }; // entity body buffer

    int rd_size = 0;                // readed size
    int fd = open(url, O_RDONLY);   // open the file

    //----------    get file size   ----------//
    off_t lastpos = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    //----------------------------------------//

    //-----------    send the response message to client    ----------// 
        //  make the response header
    sprintf(response_header,
            "HTTP/1.0 200 OK\r\n"
            "Server:2019 simple web server\r\n"
            "Content-length:%lu\r\n"
            "Content-type:%s\r\n\r\n", (unsigned long)lastpos, MIMETYPE);
        // send the message header
    write(client_fd, response_header, strlen(response_header));


        // send message body until the end
    while((rd_size = read(fd, response_message, BUFSIZE)) > 0)
        write(client_fd, response_message, rd_size);
    //----------------------------------------------------------------//

    close(fd);
}
