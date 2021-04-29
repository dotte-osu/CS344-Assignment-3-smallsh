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

// Struct to store commands 
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


//Function to replace $$ (pid)
char* replaceStr(char *basestr, char *pid)
{
    // Check for $$ to replace
    // Store how many time we need to replace
    int i = 0;
    int count = 0; 
    for (i = 0; basestr[i] != '\0'; i++) {
        // Remove char from the front of string. Check if string start with "$$"
        if (strstr(&basestr[i], "$$") == &basestr[i]) {
            // If found, check how many time we need to replace
            count++;
            // Skip two index for "$$"
            i = i + 2 - 1;
        }
    }
    // Store the change to result variable  
    int pidLen = strlen(pid);
    int length = strlen(basestr) -1 + count*(pidLen - 2) + 1;
    char *result = (char*)malloc(length);
    i = 0;//this is index for result string
    while (*basestr) {
        // Remove char from the front of string. Check if string start with "$$"
        if (strstr(basestr, "$$") == basestr) {
            // If $$ is found, replace with pid
            strcpy(&result[i], pid);
            // Move the index
            i = i + pidLen;
            basestr = basestr + 2;
        }
        else{
            // Copy the other part of string
            result[i] = *basestr;
            i++;
            basestr++;
        }    
    }  
    // Add \0 at the end
    result[i] = '\0';
    return result;
}

// Function to get user input (commands)
char *getUserInput(){

    // Print prompt with ": "
    printf(": ");
    // Flush the standard output to make sure it shows up on the terminal
    fflush(stdout);
    
    // Get user input
    char *input = NULL;
    ssize_t bufsize = 0;
    getline(&input, &bufsize, stdin);

    // Ignore comment starting with "#"
    if(input[0]=='#') return "";;
    
    // Remove newline from the end. We dont want to store it in args
    input[strlen(input) - 1] = 0;
    
    // Replace $$ with process ID   
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

// Function to parse user input to struct inputData
struct inputData *parseInput(char *input){
    // Create memory space for currData
    struct inputData *currData = malloc(sizeof(struct inputData));
    // Get command
    char *saveptr;
    
    // First word is always command
    char *token = strtok_r(input, " ", &saveptr);
    strcpy(currData->command, token);

    // Store the rest: args, input file, output file, background(&)
    currData->args = malloc(MAX_ARG * sizeof(char*));
    int i = 0;
    while(token){
        token = strtok_r(NULL, " ", &saveptr);
        // If userinput only contains command (e.g echo) return blank struct
        if(!token) return currData;

        if(strcmp(token, "<") == 0 ) // Check input file
        {
            token = strtok_r(NULL, " ", &saveptr);
            currData->inputFile = calloc(strlen(token) + 1, sizeof(char));
            strcpy(currData->inputFile, token);
        }
        else if(strcmp(token, ">") == 0 ) // Check out put file
        {
            token = strtok_r(NULL, " ", &saveptr);
            currData->outputFile = calloc(strlen(token) + 1, sizeof(char));
            strcpy(currData->outputFile, token); 
        }
        else if(strcmp(token, "&") == 0 ) // Check backgroung comment
        {
            // The last word must be &. If the & character appears anywhere else.
            // If this is not the last word, it will set to false (see next line)
            currData->isBackground = true;
            currData->args[i] = NULL; 
            i++;
        }
        else
        {
            // Store the rest of args
            currData->isBackground = false;
            currData->args[i] = token; 
            i++;
        }
    }
    return currData;
}

// Function to handle Built-in Commands "cd"
void cd(char **args){

    // "cd" only    
    if(args[0] == NULL){
        //go to home directly
        if(chdir(getenv("HOME")) != 0) perror("chdir"); //chdir: Bad address
    }
    else // "cd + directly"    
    {
        //move to a directly
        if(chdir(args[0]) != 0) perror("chdir");//chdir: Bad address
    }
}

/* Function to handle Built-in Commands "status"
   code reference: Exploration: Process API - Monitoring Child Processes */
void status(int status){
    // WIFEXITED returns true if the child was terminated normally
    if (WIFEXITED(status)) {
        // It exit normally. If exited by status. Example rerutn: 0 or 1
        printf("exit value %d\n", WEXITSTATUS(status));
    } else {
        // Did not exit normally. If terminated by signal.
        // WTERMSIG will return the signal number that caused the child to terminate.
        printf("terminated by signal %d\n", WTERMSIG(status));
    }
}

/* Function to check background commands
   code reference: Exploration: Process API - Monitoring Child Processes */
void checkBackground(){
    pid_t bgPid;
    int   bgStatus;
    bgPid = waitpid(-1, &bgStatus, WNOHANG);
    // Wait till pgPid returns > 0. If the child hasn't terminated, waitpid will immediately return with value 0
    if(bgPid > 0) {
        // WIFEXITED returns true if the child was terminated normally
        if(WIFEXITED(bgStatus)){
            // It exit normally
            printf("background pid %d is done. exit value %d\n", bgPid, WEXITSTATUS(bgStatus));
            fflush(stdout);
            removePid(&head, bgPid);
            bgCount--;
        } 
        else{
            // Did not exit normally. If terminated by signal.
            printf("background pid %d is done. terminated by signal %d\n", bgPid, WTERMSIG(bgStatus));
            fflush(stdout);
            removePid(&head, bgPid);
            bgCount--;
        }
    }
}

/* Function to excute no build-in command
   code reference: Exploration: Process API - Executing a New Program 
                   Exploration: Processes and I/O                     */
int excuteOthers(char *command, char **args, char* inputFile, char* outputFile, bool isBackground){

    char **newargv = malloc(MAX_ARG * sizeof(char*));
    
    // Set command
    newargv[0] = command;
    int i=0;

    // Copy user input to newargv[]
    while(args[i]){
        newargv[i+1] = args[i];
        i++;
    }

    int childStatus = 0;
    int inFile;
    int outFile;
    int result;

    // Fork a new process
    pid_t spawnPid = fork();
    switch(spawnPid){
    case -1:
        perror("fork()\n");
        exit(1);
        break;
    case 0:
        // Run command in the child process
        // A child running as a foreground process must terminate itself when it receives SIGINT
        if(!fgMode){
            // SIG_DFL – specifying this value means we want the default action to be taken for the signal type.
            SIGINT_action.sa_handler = SIG_DFL;
			SIGINT_action.sa_flags = 0; // no flag
			sigaction(SIGINT, &SIGINT_action, NULL);
            fflush(stdout);
        }
        
        // Exploration: Processes and I/O
        // Set input redirection
        if (inputFile) {
            // Open input file
            inFile = open(inputFile, O_RDONLY);
            if (inFile == -1) {
                // Print if error
                perror("source open()"); 
                exit(1);
            }

            // Redirect stdin to input file
            result = dup2(inFile, 0);
            if (result == -1) {
                // Print if error
                perror("source dup2()"); 
                exit(2);
            }
            // Close
            fcntl(inFile, F_SETFD, FD_CLOEXEC);
        }
        // Set output redirection
        if (outputFile) {
            // Open output file
            outFile = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (outFile == -1) {
                // Print if error
                perror("Unable to open output file\n");
                exit(1);
            }

            // Redirect stdout to output file
            result = dup2(outFile, 1);
            if (result == -1) {
                // Print if error
                perror("Unable to assign output file\n");
                exit(2);
            }
            // Close
            fcntl(outFile, F_SETFD, FD_CLOEXEC);
        }
        // Error handling
        if (execvp(newargv[0], newargv)) {
            perror("execve");
            exit(2);
        }
        break;
    default:
        // In the parent process
        // Exploration: Process API - Monitoring Child Processes
        if (!isBackground || fgMode){
            // Wait for child's termination
            spawnPid = waitpid(spawnPid, &childStatus, 0);
            
            // Signal handling
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
            // Don't wait for child's termination
            printf("background pid is %d\n", spawnPid);

            // Keep background record for later use below
            // 1. To check the background status
            // 2. To kill on-going program when exiting
            bgCount++;
            addPid(&head, spawnPid);
            fflush(stdout);
        }
        
        break;
    } 
    return childStatus;
}


/* Function to handle SIGTSTP signals
   code reference: Exploration: Signal Handling API              */
void handle_SIGTSTP(int signo){
    // If A CTRL-Z command is entered, it will switch to foreground-only mode
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

// Function to kill all the on-going programs before exiting
void killAll(struct Node* node)
{
    while (node != NULL) {
        //SIGTERM is sent to terminate a process. 
        kill(node->pid,SIGTERM);
        node = node->next;
    }
}

int main(){
    bool eof = false;
    char *input;
    int exitStatus = 0;

    // SIGINT handling
    // SIG_IGN – specifying this value means that the signal type should be ignored
    SIGINT_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &SIGINT_action, NULL);
    // SIGTSTP handling
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    //Setting SA_RESTART will cause an automatic restart of the interrupted system call or library function after the signal handler gets done.
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // Loop until end of file
    while(!eof){
        // Get user input
        input = getUserInput();

        // Check background status
        if(bgCount > 0) checkBackground();

        // Ignroe blank line
        if(!strcmp(input, "")) continue;

        // Parse user input
        struct inputData *currData = parseInput(input);

        // Execute commands 
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
    }
    
    return 0;
}