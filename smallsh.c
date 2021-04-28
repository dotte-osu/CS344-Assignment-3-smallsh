#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h> // getpid, getppid
#include <fcntl.h>
#include <signal.h>

#define MAX_ARG 512
#define MAX_COMMAND 2048 
#define PATH_MAX 1000

// Global variables to check the background process
int bgCount = 0; //store background count
bool fgMode = false; //true == Entering foreground-only mode, false == Exiting foreground-only mode

// Global variables to handle Signals
struct sigaction SIGINT_action = {0};
struct sigaction SIGTSTP_action = {0};

// struct for commands 
struct inputData{
    char    command[MAX_COMMAND];
    char    **args;
    char    *inputFile;
    char    *outputFile;
    bool    isBackground;
};

/* A linked list node to store backgroung pid
   When a user exit the program, all the background pids need to be terminated*/
struct Node {
    int pid;
    struct Node* next;
};
struct Node* head = NULL;

/* Function to add background pid to a linked list */
void addPid(struct Node** listHead, int pid)
{
    // Create a memory to save
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    newNode->pid = pid;
    newNode->next = (*listHead);
    (*listHead) = newNode;
}
 
/* Function to remove background pid to a linked list */
void removePid(struct Node** listHead, int pid)
{
    // Keep the head
    struct Node *temp = *listHead, *prev;
 
    // If the pid to remove is the head, replace the head
    if (temp != NULL && temp->pid == pid) {
        *listHead = temp->next; 
        free(temp);
        return;
    }
 
    // Loop the linked list and find the pid to remove
    while (temp != NULL && temp->pid != pid) {
        // Keep the previous node for later
        prev = temp;
        temp = temp->next;
    }
 
    // Unlink
    prev->next = temp->next;
    free(temp);
}


//Function to replace string
char* replaceStr(char *basestr, char *pid)
{
    int i = 0;
    int count = 0; //store how many time we need to replace
    //check for $$
    for (i = 0; basestr[i] != '\0'; i++) {
        //remove char from the front of string. check is string start with "$$"
        if (strstr(&basestr[i], "$$") == &basestr[i]) {
            //if found, check how many time we need to replace
            count++;
  
            //skip index for "$$"
            i = i + 2 - 1;
        }
    }
    //store to result    
    int pidLen = strlen(pid);
    int length = strlen(basestr) -1 + count*(pidLen - 2) + 1;
    char *result = (char*)malloc(length);
    i = 0;//this is index for result string
    while (*basestr) {
        //remove char from the front of string. check is string start with "$$"
        if (strstr(basestr, "$$") == basestr) {
            //if $$ is found, replace with pid
            strcpy(&result[i], pid);
            //move the index
            i = i + pidLen;
            basestr = basestr + 2;
        }
        else{
            //copy the other part of string
            result[i] = *basestr;
            i++;
            basestr++;
        }    
    }
  
    //add \0 at the end
    result[i] = '\0';
    return result;
}

//Get user input (commands)
char *getUserInput(){

    //print prompt
    printf(": ");
    //Flush the standard output to make sure it shows up on the terminal
    fflush(stdout);
    
    //Get user input
    char *input = NULL;
    ssize_t bufsize = 0;
    getline(&input, &bufsize, stdin);
    //ignore comment
    if(input[0]=='#') return "";;
    
    //Remove newline from the end
    input[strlen(input) - 1] = 0;
    
    //Replace $$ with process ID   
    char *replacedInput = NULL;
    if (strstr(input, "$$") != NULL) {
        
        //input contains $$
        int pid = getpid();
        char pidStr[6];
        snprintf(pidStr, 10, "%d", pid);
        
        //replace $$ with pid
        replacedInput = replaceStr(input, pidStr);
    }
    return replacedInput == NULL ? input : replacedInput;
}
struct inputData *parseInput(char *input){
    struct inputData *currData = malloc(sizeof(struct inputData));
    //Get command
    char *saveptr;
    
    //First word is always command
    char *token = strtok_r(input, " ", &saveptr);
    //currData->command = calloc(strlen(token) + 1, sizeof(char));
    strcpy(currData->command, token);
    //printf("command %s\n", currData->command);
    //args, input file, output file
    currData->args = malloc(MAX_ARG * sizeof(char*));
    int i = 0;
    while(token){
        //printf("token %s\n", token);
        token = strtok_r(NULL, " ", &saveptr);
            if(!token) return currData;
        if(strcmp(token, "<") == 0 )
        {
            token = strtok_r(NULL, " ", &saveptr);
            currData->inputFile = calloc(strlen(token) + 1, sizeof(char));
            strcpy(currData->inputFile, token);
        }
        else if(strcmp(token, ">") == 0 )
        {
            token = strtok_r(NULL, " ", &saveptr);
            currData->outputFile = calloc(strlen(token) + 1, sizeof(char));
            strcpy(currData->outputFile, token); 
        }
        else if(strcmp(token, "&") == 0 )
        {
            // The last word must be &. If the & character appears anywhere else.
            // If this is not the last word, it will set to false (see next line)
            currData->isBackground = true;
            currData->args[i] = NULL; 
            i++;
        }
        else
        {
            currData->isBackground = false;
            currData->args[i] = token; 
            i++;
        }
    }
    return currData;
}
void cd(char **args){
    //char cwd[PATH_MAX];
    //getcwd(cwd, sizeof(cwd));
    //printf("Current working dir: %s\n", cwd);
    
    if(args[0] == NULL){
        //go to home directly
        if(chdir(getenv("HOME")) != 0) perror("chdir"); //chdir: Bad address
    }
    else
    {
        //move to a directly
        if(chdir(args[0]) != 0) perror("chdir");//chdir: Bad address
    }
}
//Exploration: Process API - Monitoring Child Processes
void status(int status){
    if (WIFEXITED(status)) {
        // If exited by status
        printf("exit value %d\n", WEXITSTATUS(status));
    } else {
        // If terminated by signal
        printf("terminated by signal %d\n", WTERMSIG(status));
    }
}
//Exploration: Process API - Monitoring Child Processes
void checkBackground(){
    pid_t bgPid;
    int   bgStatus;
    bgPid = waitpid(-1, &bgStatus, WNOHANG);
    // Wait till pgPid returns > 0. If the child hasn't terminated, waitpid will immediately return with value 0
    if(bgPid > 0) {
        if(WIFEXITED(bgStatus)){
            // It exit normally
            printf("background pid %d is done. exit value %d\n", bgPid, WEXITSTATUS(bgStatus));
            fflush(stdout);
            removePid(&head, bgPid);
            bgCount--;
        } 
        else{
            // Did not exit normally
            printf("background pid %d is done. terminated by signal %d\n", bgPid, WTERMSIG(bgStatus));
            fflush(stdout);
            removePid(&head, bgPid);
            bgCount--;
        }
    }
}

int excuteOthers(char *command, char **args, char* inputFile, char* outputFile, bool isBackground){
    //char **newargv[] = { command, "do something", NULL };
    char **newargv = malloc(MAX_ARG * sizeof(char*));
    newargv[0] = command;
    int i=0;
    while(args[i]){
        newargv[i+1] = args[i];
        i++;
    }
    int childStatus = 0;
    int inFile;
    int outFile;
    int result;
    //Exploration: Process API - Executing a New Program
    // Fork a new process
    pid_t spawnPid = fork();
    switch(spawnPid){
    case -1:
        perror("fork()\n");
        exit(1);
        break;
    case 0:
        // A child running as a foreground process must terminate itself when it receives SIGINT
        if(!fgMode){
            // SIG_DFL – specifying this value means we want the default action to be taken for the signal type.
            SIGINT_action.sa_handler = SIG_DFL;
			SIGINT_action.sa_flags = 0;
			sigaction(SIGINT, &SIGINT_action, NULL);
            fflush(stdout);
        }
        //Run command in the child process
        //Exploration: Processes and I/O
        //set input
        if (inputFile) {
            // Open input file
            inFile = open(inputFile, O_RDONLY);
            if (inFile == -1) {
                perror("source open()"); 
                exit(1);
            }
            // Write to terminal
            //printf("inFile == %d\n", inFile); 
            // Redirect stdin to input file
            result = dup2(inFile, 0);
            if (result == -1) {
                perror("source dup2()"); 
                exit(2);
            }
            // Close
            fcntl(inFile, F_SETFD, FD_CLOEXEC);
        }
        //set output
        if (outputFile) {
            // open it
            outFile = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (outFile == -1) {
                perror("Unable to open output file\n");
                exit(1);
            }
            // Write to terminal
            //printf("outFile == %d\n", outFile); 
            // Redirect stdout to output file
            result = dup2(outFile, 1);
            if (result == -1) {
                perror("Unable to assign output file\n");
                exit(2);
            }
            // Close
            fcntl(outFile, F_SETFD, FD_CLOEXEC);
        }
        if (execvp(newargv[0], newargv)) {
            perror("execve");
            exit(2);
        }
        break;
    default:
        //Exploration: Process API - Monitoring Child Processes
        //In the parent process
        if (!isBackground || fgMode){
            //Wait for child's termination
            spawnPid = waitpid(spawnPid, &childStatus, 0);
            
            if (WIFSIGNALED(childStatus)) {
                printf("terminated by signal %d\n", WTERMSIG(childStatus));
                fflush(stdout);
            }
            
            if (WIFSTOPPED(childStatus)) {
                printf("stopped by signal %d\n", WSTOPSIG(childStatus));
                fflush(stdout);
            }
            
        }
        else{
            //Don't wait for child's termination
            //pid_t pid = waitpid(spawnPid, childStatus, WNOHANG);
            printf("background pid is %d\n", spawnPid);
            bgCount++;
            addPid(&head, spawnPid);
            fflush(stdout);
        }
        
        break;
    } 
    return childStatus;
}

//Exploration: Signal Handling API
// Handler for SIGTSTP
void handle_SIGTSTP(int signo){
    if(!fgMode){
        char* message = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 50);
        fgMode = true;
        //print prompt
        printf(": ");
        //Flush the standard output to make sure it shows up on the terminal
        fflush(stdout);
    }else{
        char* message = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30);
        fgMode = false;
        //print prompt
        printf(": ");
        //Flush the standard output to make sure it shows up on the terminal
        fflush(stdout);
    }
}

// This function prints contents of linked list starting
// from the given node
void killAll(struct Node* node)
{
    while (node != NULL) {
        //SIGTERM is sent to terminate a process, but it is catchable unlike SIGKILL. 
        kill(node->pid,SIGTERM);
        node = node->next;
    }
}

int main(){
    bool eof = false;
    char *input;
    int exitStatus = 0;

    
    // SIGINT 
    // SIG_IGN – specifying this value means that the signal type should be ignored
    SIGINT_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &SIGINT_action, NULL);
    // SIGTSTP
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    //Setting SA_RESTART will cause an automatic restart of the interrupted system call or library function after the signal handler gets done.
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    //Loop until end of file
    while(!eof){
        //get user input
        input = getUserInput();
        // Check background status
        if(bgCount > 0) checkBackground();
        //ignroe blank line
        if(!strcmp(input, "")) continue;
        //parse user input
        struct inputData *currData = parseInput(input);
        //print args (debug)
        //int i = 0;
        //while(currData->args[i]){
        //    printf("args: %s\n", currData->args[i]);
        //    i++;
        //}
        //create path
        //char path[] = "/bin/";
        //char *command = calloc(strlen(currData->command) + 1, sizeof(char));
        //strcpy(command, currData->command);
        //strcat(path, command);
        //printf("path: %s\n", path);    
        //printf("command: %s\n", currData->command);    
        
        //execute Built-in Commands first(exit, cd, and status)
        if(strcmp(currData->command, "cd") == 0) cd(currData->args);
        else if(strcmp(currData->command, "status") == 0) status(exitStatus);
        else if (strcmp(input, "exit") == 0) {
            eof = true; 
            // Kill all the background process
            if(bgCount > 0){
                killAll(head);
            }
        }
        else exitStatus = excuteOthers(currData->command, currData->args, currData->inputFile, currData->outputFile, currData->isBackground);

        free(currData);
    }
    
    return 0;
}