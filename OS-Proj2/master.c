#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <time.h>

#define MEMSIZE  4096
#define SHAREDFILE "./master"

pid_t g_pid;
pid_t g_child_pids[100];

//Shared memory globals
int *g_numberArray; // For the number data
int *g_N; // the size of the number array

int *g_Tokens;  //The tokens array for the processes for the Bakery Algorithm
int *g_Choosing; //The doorway guard array for the Bakery Algorithm
pid_t *g_PIDs; //id's array for the processes for the Bakery Algorithm
int *g_NumProcesses; // The number of processes

int *g_MaxDepth; // The maximum depth for the binary tree for the current data set



void destroySharedMemForData();
void destroySharedMemForBA();

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

bool destroy_memory_block(char* filename,int proj_id)
{
    int shared_block_id = get_shared_block(filename, 0, proj_id);
    if(shared_block_id == -1)
        return 0;
    return(shmctl(shared_block_id, IPC_RMID, NULL) != -1);
}


// Results placed in log file
void logResults()
{
    long curTime;
    struct timespec now;  
    FILE *fpOut = fopen("output.log", "w");
    
    fprintf(fpOut,"The Final state of the shared memory int array at time\n");
    fprintf(fpOut, "of application exit is:\n");            
    for(int i=0; i < *g_N; i++){
        fprintf(fpOut," %d, ",g_numberArray[i]);           
    }
    
    fprintf(fpOut, "\nThe first location value, %d, is the result of the sum of integers if\n",g_numberArray[0]);
    fprintf(fpOut," the application wasn't ended too abruptly.\n");
    clock_gettime(CLOCK_REALTIME, &now);
    curTime = ((((long)now.tv_sec) * 1000000000) + (long)now.tv_nsec);
    fprintf(fpOut,"\nTime of application end: %ld\n", curTime);    
    fclose(fpOut);
}

// Results printed to screen.
void resultsData()
{
    int i;
    printf("\n\n  Results of the master process:\n\n");
            
    printf("The Final state of the shared memory int array at time\n");
    printf("of application exit is:\n");            
    for(int i=0; i < *g_N; i++){
        printf(" %d, ",g_numberArray[i]);           
    }
    
    printf("\nThe first location value, %d, is the result of the sum of integers if\n",g_numberArray[0]);
    printf(" the application wasn't ended too abruptly.\n\n");
    printf(" See the files \"output.log\" and \"adder_log\" for more details.\n" );
}
// Determine if the string is an integer
// return 1 if it is, 0 if is is not
int isInteger(char *str)
{
    char *ptr;
    
    ptr = str;
    while(*ptr != '\0'){
        if(isdigit(*ptr)==0){
            return 0;
        }
        ptr++;
    }
    return 1;
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


int getDepth(int number){
    int v=1;
    int depth=0;
    
    while(v < number){
        v *= 2;
        depth++;
    }
    return depth;
}


// Reset the shared memory values for the Bakery Algorithm
void resetSharedMemBA()
{
    int i=0;
    //Initialize arrays to zero
    for(i=0; i < *g_NumProcesses; i++){
        g_Tokens[i] = 0;
        g_Choosing[i] = 0;
        g_PIDs[i] = 0;
    }
}

int numberOfChildProcessRunning()
{
    int status;
    int i;
    int count=0;
    
    for(i=0; i < *g_NumProcesses; i++){
        if(waitpid(g_PIDs[i], &status, WNOHANG)==0){       
            count++; // Process assumed still alive
        }
    }
    return count;
}

void spawnProcesses(int number, int maxProcesses)
{
    int status=0;
    char command[150];
    char **args;
    int count=1;
    int depth = getDepth(number*2);
    int index=0;
    int loop = 1; 
    int deltaIndex = 2;   
    int runningProcesses;
    int loop2;

    
    *g_MaxDepth = depth; //Provide maximum depth for child processes
    pid_t pid;
    while(loop){
        //Spawning a duplicate child process with a different pid
        for(count=0; count < number; count++){
            //Make sure that the number of running processes doesn't exceed maxProcesses
            loop2 = 1;
            while(loop2){
              runningProcesses =  numberOfChildProcessRunning();
              if(runningProcesses < maxProcesses){
                  loop2 = 0;
              }
            }
            sprintf(command,"./bin_adder %d %d",index,depth); 
            index += deltaIndex;         
            args = split(command,4);
           // printf("Going To Fork!\n");
  
            pid = fork();
            switch(pid){ // Error
                case -1:
                    perror("fork failed\n");
                    exit(1);
                case 0: // Child process follows this route              
                    // This command executes the process passed as parameters in cmdLine and 
                    // replaces the child process, retaining the child process PID
                    execvp(args[0],args);
                    perror("execvp"); // execvp doesn't return unless there is a problem
                    exit(1);                      
                    break;
                default:
                    g_PIDs[count] = pid;
            }
        }

        //printf(" waiting on processes\n");
        for(count =0; count < number; count++){
            // wait for the child to complete, which will end up the process
            // launched by execvp() above.            
            while (waitpid(g_PIDs[count], &status, 0) == -1); 
            //WIFEXITED() returns true if the child terminated normally.
            // WEXITSTATUS() returns the exit status of the child. This macro should be employed only if WIFEXITED returned true.
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            {
                // handle error                        
                printf("process %s  (pid = %d) failed\n", args[0], g_child_pids[count]);               
            }
        }
        
        //printf(" Finished with depth %d\n",depth);
        // Go to the next level if necessary
        //If one process ran last time, then the multip processing is done.
        if(number == 1){
            loop = false;
        }else{ //Go to the next depth
            //Halve the number of processes
            number = number/2;
            *g_NumProcesses = number; // Set the shared number to the reduced count
            resetSharedMemBA(); //Reset the shared memory for the Bakery Algorithm
            depth--; 
            index = 0;
            deltaIndex *= 2;
        }
        
        
    }
    logResults();
}

void spawnProcesses2()
{
    int status=0;
    char command[150];
    char **args;
    
    
            //Spawning a duplicate child process with a different pid
    pid_t pid = fork();
    g_pid = pid;
    strcpy(command,"./bin_adder 2 3");
    args = split(command,3);
    //printf(" parent: childpid = %d\n",g_pid);
    switch(pid){ // Error
        case -1:
            perror("fork");
            exit(1);
        case 0: // Child process follows this route              
            // This command executes the process passed as parameters in cmdLine and 
            // replaces the child process, retaining the child process PID
            execvp(args[0],args);
            perror("execvp"); // execvp doesn't return unless there is a problem
            exit(1);
        default: // Parent process follows this route1
            // wait for the child to complete, which will end up the process
            // launched by execvp() above.                         
            while (waitpid(pid, &status, 0) == -1); 
            //WIFEXITED() returns true if the child terminated normally.
            // WEXITSTATUS() returns the exit status of the child. This macro should be employed only if WIFEXITED returned true.
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            {
                // handle error                        
                printf("process %s  (pid = %d) failed\n", args[0], pid);               
            }            
            break;            
    }

}

void handle_SIGINT()
{
    int i; 
    
    printf("Kill Child Processes, exit master.\n");
    for(i=0; i < *g_NumProcesses; i++){        
        kill(g_PIDs[i], SIGKILL); 
    }
    logResults();
    resultsData();
    destroySharedMemForBA();
    destroySharedMemForData();
    exit(0);
}



//Setting up shared memory for the Bakery Algorithm
void setupSharedMemForBA(int numberOfProcesses)
{
    int i = 0;
    g_NumProcesses = (pid_t *)attach_memory_block(SHAREDFILE,sizeof(int),123450);
    *g_NumProcesses = numberOfProcesses; // Set the number of processes right away
    g_Tokens = (int *)attach_memory_block(SHAREDFILE,numberOfProcesses*sizeof(int),123451);
    g_Choosing = (int *)attach_memory_block(SHAREDFILE,numberOfProcesses*sizeof(int),123452);    
    g_PIDs = (pid_t *)attach_memory_block(SHAREDFILE,numberOfProcesses*sizeof(pid_t),123453);

    g_MaxDepth = (int *)attach_memory_block(SHAREDFILE,sizeof(int),123456);
    printf(" setupSharedMemForBA, numberOfProcesses = %d\n",numberOfProcesses);
    
    //Initialize arrays to zero
    for(i=0; i < numberOfProcesses; i++){
        g_Tokens[i] = 0;
        g_Choosing[i] = 0;
        g_PIDs[i] = 0;
    }
}

// Destroying the shared memory for the Bakery Algorithm
void destroySharedMemForBA()
{
    destroy_memory_block(SHAREDFILE,123450);
    destroy_memory_block(SHAREDFILE,123451);
    destroy_memory_block(SHAREDFILE,123452);
    destroy_memory_block(SHAREDFILE,123453);
    destroy_memory_block(SHAREDFILE,123456);
}

void setupSharedMemForData(int* array,int arraySize)
{
    int i;
    g_N = (int *)attach_memory_block(SHAREDFILE,sizeof(int),123454); 
    *g_N = arraySize;
    g_numberArray = (int *)attach_memory_block(SHAREDFILE,arraySize*sizeof(int),123455);

    for(i=0; i < arraySize; i++){
        g_numberArray[i] = array[i];
    }
}

void destroySharedMemForData()
{
    destroy_memory_block(SHAREDFILE,123454);
    destroy_memory_block(SHAREDFILE,123455);
}


// Make sure array size is of 2^n size
// pass in the current size. Returns the smallest 
// 2^N size that is equal to or greater than size
int twoToNArraySize(int size)
{
    int pow = 1;
  
    while(pow < size){
        pow *= 2;     
    }
 
    return pow;
}

int readInFileData(char* filename,int** numArray)
{    
    int arraySize, value;
    int arraySize2N;
    int i;
    FILE* fp = fopen(filename, "r");

     if(fp == NULL){
         //errno = ?
         printf("Unable to open the file %s\n",filename);
         return -1;
     }
     arraySize = 0;
      while( fscanf(fp, "%d",&value) != EOF ){
          arraySize++;
     }

     fclose(fp);
     arraySize2N = twoToNArraySize(arraySize);
     
     (*numArray) = (int *)calloc(arraySize2N,(sizeof(int)));
     fp = fopen(filename, "r");

     i = 0;
     for(i=0; i < arraySize; i++){
         fscanf(fp, "%d",&value);
         (*numArray)[i] = value;       
     }
     
     fclose(fp);
  
     return arraySize2N;
}

// Thread function to calculate the fibonacci
static void *timerForMaster(void *threadData) {  
   struct timespec now1, now2;   
   int *maxTime=(int *)(threadData);
   int loop = 1;
 
   clock_gettime(CLOCK_REALTIME, &now1);
   printf("\n In Timer time = %d\n",*maxTime);
   while(loop){
        clock_gettime(CLOCK_REALTIME, &now2);
        if((int)now2.tv_sec - (int)now1.tv_sec >= *maxTime){
            loop = 0;
        }
   }
   printf("\nnow2.tv_sec = %d, now1.tv_sec = %d\n",(int)now2.tv_sec,(int)now1.tv_sec);
   printf("Timer Timed Out!\n");
   kill(getpid(), SIGINT);  
}

void testWriteToMemory()
{
    int i;
    for(i=0; i < *g_NumProcesses; i++){
        g_Tokens[i] = i + 10;
        g_Choosing[i] = i + 20;
        g_PIDs[i] = i + 30;
    }
}

void helpInfo()
{
   
    printf("\n  Welcome To Help for the master/bin_adder program\n\n");
    
    printf("\nThis program sums up a list of integers from a datafile input on the command line.\n");
    printf("The integers should be one to a line in the file.  It uses a binary tree multiprocessing algorithm to sum them.\n\n");

    printf("Command line use is as follows:\n\n");

    printf("master [-h] [-s i] [-t time] datafile\n\n");

    printf("-h          Gets you this, which is help in order to use this program.\n");
    printf("-s x        Indicates the number of children that are allowed to exist in the system at a the same time. (Default 20\n");
    printf("-t time     The time in seconds after which the process will terminate, even if it has not finished. (Default 100)\n");
    printf("Datafile    Input file containing one integer on each line.\n\n");

    printf("You can terminate this program and its child processes at any time with at ctrl-C.\n\n");

    printf("The output file \"adder_log\" shows the state of each child called. It has the following format for each line:\n\n");

    printf("Time PID Index Depth\n\n");

    printf("Depth is the binary depth if the binary tree algorithm.  Index is the starting index in the shared memory\n");
    printf("integer array.  This is the array that is being summed.  PID is the index of the child process.\n\n");

    printf("The \"output.log\" file shows the final results of the program.  The left most array value will be the\n");
    printf("sum of the integers, if the application hasn't been terminated too early.\n\n");

    printf("This master program works in conjunction with bin_adder, the child process that does the adding.\n\n");
}

int main(int argc,char **argv)
{
    int option;    
    int maxProcesses=20; 
    int maxTime=100;    
    int i;
    struct sigaction handler;
    int arraySize;
    int *numArray;
    int numProcesses;
    pthread_t timerThread; 
    
    handler.sa_handler = handle_SIGINT;
    sigemptyset(&handler.sa_mask);
        
    handler.sa_flags = 0;
    sigaction(SIGINT,&handler, NULL);

   
    while((option = getopt(argc, argv, "hs:t:drc")) != -1){       
        switch(option){
            case 'h':                               
                helpInfo();
                return (EXIT_SUCCESS);               
            case  's':
                if(isInteger(optarg) == 0){
                    printf("s option is not valid. Should be an integer\n");
                    exit(1);
                }                
                maxProcesses = atoi(optarg);                
                printf("s, optarg = %s\n",optarg);
                break;
            case 't':
                if(isInteger(optarg) == 0){
                    printf("t option is not valid. Should be an integer\n");
                    exit(1);
                }
                maxTime = atoi(optarg);
                printf("t, optarg = %s\n", optarg);
                break;
        }               
              
    }
    
    
    
    if(argc > 1){
        printf("\nmaxProccesses = %d, maxTime = %d\n",maxProcesses, maxTime);
        arraySize = readInFileData(argv[argc-1],&numArray);
        if(arraySize == -1){
            return 0;
        }
        printf("\n main array = :");
        for(i=0; i < arraySize; i++){
            printf(" %d, ", numArray[i]);
        }
        printf("\n");
        numProcesses = arraySize/2; //Start out with this many processes, half of the total for binary calculation
        setupSharedMemForData(numArray, arraySize);        
        setupSharedMemForBA(numProcesses);
       
        printf(" shared numArray at start: ");
        for(i=0; i < arraySize; i++){
            printf(" %d, ",g_numberArray[i]);           
        }
 
        //Zero out log file for this pass
        FILE *fpOut = fopen("adder_log", "w");
        fclose(fpOut);
        
        // Launcher timer, a time limit on how long this application can run.
        pthread_create(&timerThread, NULL, timerForMaster,(void *)&maxTime);
        spawnProcesses(numProcesses,maxProcesses);        
        resultsData();
        destroySharedMemForBA();
        destroySharedMemForData();
        free(numArray);
    }
   
    return 0;    
}
