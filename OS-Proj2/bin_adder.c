#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>

#define MEMSIZE 4096
#define SHAREDFILE "./master"

//Shared memory globals
int *g_numberArray; // For the number data
int *g_N; // the size of the number array

int *g_Tokens;  //The tokens array for the processes for the Bakery Algorithm
int *g_Choosing; //The doorway guard array for the Bakery Algorithm
pid_t *g_PIDs; //id's array for the processes for the Bakery Algorithm
int *g_NumProcesses; // The number of processes

int *g_MaxDepth; // The maximum depth for the binary tree for the current data set


int g_Index; // the index position for this child within the g_Tokens, g_Choosing, and g_PIDs arrays above

int get_shared_block(char* filename,int size, int proj_id)
{
    key_t key;
    key = ftok(filename,proj_id);
    return shmget(key,size,0644 | IPC_CREAT);
}

char* attach_memory_block(char* filename, int size,int proj_id)
{
    int shared_block_id = get_shared_block(filename,size, proj_id);
    if(shared_block_id == -1)
        return NULL;
    
    char *result = shmat(shared_block_id,NULL, 0);
    if(result == (void *)-1)
        return NULL;
    return result;
}

char ** split(char *source, int max)
{
	//char **parts = new char *[max+1]; // NULL is stored in the last element
	char** parts = malloc((max+1) * sizeof(char*));
	int i = 0;
	char *token;
	token = strtok(source," ");
	while(token != NULL && i < max){
		parts[i] = strdup(token);
		token = strtok(NULL, " ");
		i++;
	};
	
	parts[i] = NULL; // used to indicate the end of the token list;
	return parts;
}

int getLevel(int number){
    int v=2;
    int level=0;
    
    while(v < number){
        v *= 2;
        level++;
    }
    return level+1;
}

//Setting up shared memory for the Bakery Algorithm
void setupSharedMemForBA()
{    
    int numberOfProcesses;
    g_NumProcesses = (pid_t *)attach_memory_block(SHAREDFILE,sizeof(int),123450);
    numberOfProcesses = *g_NumProcesses; // Set the number of processes right away
    g_Tokens = (int *)attach_memory_block(SHAREDFILE,numberOfProcesses*sizeof(int),123451);
    g_Choosing = (int *)attach_memory_block(SHAREDFILE,numberOfProcesses*sizeof(int),123452);    
    g_PIDs = (pid_t *)attach_memory_block(SHAREDFILE,numberOfProcesses*sizeof(pid_t),123453);
    g_MaxDepth = (int *)attach_memory_block(SHAREDFILE,sizeof(int),123456);
}

void setupSharedMemForData()
{
    int arraySize;   
    g_N = (int *)attach_memory_block(SHAREDFILE,sizeof(int),123454); 
    arraySize = *g_N;
    g_numberArray = (int *)attach_memory_block(SHAREDFILE,arraySize*sizeof(int),123455);  
}

void testReadFromMemory()
{
    printf(" In bin_addr testReadMemory()\n\n");
    
    printf(" data array size = %d\n", *g_N);
    printf(" num of processes = %d\n", *g_NumProcesses);
    int i=0;
    printf(" data: ");
    for(i=0; i < *g_N; i++){
        printf("%d, ",g_numberArray[i]);
    }
    printf("\n");
    
    printf(" tokens: ");
    for(i=0; i < *g_NumProcesses; i++){
        printf("%d, ", g_Tokens[i]);   
    }
    printf("\n");
    printf(" choosing: ");
    for(i=0; i < *g_NumProcesses; i++){
        printf("%d, ", g_Choosing[i]);
    }
    printf("\n");
    printf(" pids: ");
    for(i=0; i < *g_NumProcesses; i++){
        printf("%d, ", g_PIDs[i]);
    }
    printf("\n\n");
}

// This gets the index of this child process within
// Bakery Algorithm Arrays by matching the PIDs
// Returns -1 if it's not found
int getIndexOfChildProcess()
{
    int i = 0;
    pid_t pid = getpid();
    
    for(i=0; i < *g_NumProcesses; i++){
        if(g_PIDs[i] == pid){
            return i;            
        }
    }
    return -1;
}

int main(int argc,char **argv)
{
    int i;
    int j;
    int level; // Opposite of depth... it starts at 1
    int delta = 1;
    int depth;
    int max=0;
    int p, loop;
    FILE* fpOut;
    long curTime;
    struct timespec now;
    int numIndex;
    pid_t pid = getpid();
    setupSharedMemForBA();
    setupSharedMemForData();    
    g_Index = getIndexOfChildProcess();

  //  printf("Hello World, from bin_adder\n");
    if(argc == 3){ 
      //  printf("bin_adder child, pid = %d, index = %s, depth = %s\n",getpid(), argv[1], argv[2]);        
        
       // printf("read memory block, create if not there.\n");
        i = atoi(argv[1]);
        numIndex = i;
        depth = atoi(argv[2]);
        
        level = *g_MaxDepth - depth;
        
       // pInt = (int *)attach_memory_block(SHAREDFILE, MEMSIZE, 0);
        
        //Determine the two numbers to add according to level and starting index position
        for(j=0; j < level; j++)   {
            delta *=2;
        }
       if(delta < *g_N){
           g_numberArray[i] = g_numberArray[i] + g_numberArray[i+delta];            
       }
        
        //Implement Bakery Algorithm here:
        //Lock
        // Get a token
        g_Choosing[g_Index] = 1; //1 means set true
        for(i=0; i < *g_NumProcesses; i++){
            if(g_Tokens[i] > max){
                max = g_Tokens[i];
            }
        }        
        g_Tokens[g_Index]  = max + 1;
   
        g_Choosing[g_Index] = 0; //Set to false
   
        for(p=0; p < *g_NumProcesses; p++){
            while(g_Choosing[p]);
            loop = 1;
     
            //Only continue if this child process's token at g_Index is the smallest value
            // If it and another have the smallest value, compare process IDs to break the tie.
            while(loop){
                if(p == g_Index){
                    loop = 0; // Don't compare a process to itself
                }else if(g_Tokens[p] == 0){
                    loop = 0;                   
                }else if(g_Tokens[p] > g_Tokens[g_Index]){
                    loop = 0;                 
                }else if(g_Tokens[p] == g_Tokens[g_Index]){
                    if(g_PIDs[g_Index] < g_PIDs[p]){
                        loop = 0;                         
                    }
                }     
            }          
        }
        
        clock_gettime(CLOCK_REALTIME, &now);        
        curTime = ((((long)now.tv_sec) * 1000000000) + (long)now.tv_nsec);
        fprintf(stderr, "PID = %d entering the critical section at %ld\n", pid, curTime);
        sleep(1); // sleep for one second
        //Critical section
        //the file will be adder_log, open it to write and append
        fpOut = fopen("adder_log", "a");
        clock_gettime(CLOCK_REALTIME, &now);
        curTime = ((((long)now.tv_sec) * 1000000000) + (long)now.tv_nsec);
        fprintf(fpOut,"%ld %d %d %d\n", curTime,  g_PIDs[g_Index],numIndex, depth);
        fclose(fpOut);
        fprintf(stderr, "PID = %d exiting the critical section at %ld\n", pid, curTime);
        // unlock critical section
        g_Tokens[g_Index] = 0;
    }    

}
