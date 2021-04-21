#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h> // getpid, getppid

//Function to replace string
char* replaceStr(char *basestr, char *pid)
{
    int i = 0;
    int count = 0; //store how many time we need to replace

    //check for $$
    for (i = 0; basestr[i] != '\0'; i++) {
        //remove char from the front of string. check is string start with "$$"
        if (strstr(&basestr[i], "$$") == &basestr[i]) {
            //found
            count++;
  
            //skip index for "$$"
            i = i + 2 - 1;
        }
    }

    //store to result    
    int pidLen = strlen(pid);
    int length = strlen(basestr) -1 + count*(pidLen - 2) + 1;
    char *result = (char*)malloc(length);

    i = 0;
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
            result[i++] = *basestr++;
        }    
    }
  
    //add \0 at the end
    result[i] = '\0';

    return result;
}


//Get user input (commands)
char *getUserInput(){

    //Flush the standard output to make sure it shows up on the terminal
	fflush(stdout);
	
	//Get user input
	char *input = NULL;
	ssize_t bufsize = 0;
	getline(&input, &bufsize, stdin);

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

char** parseInput(char *input, char* command, char **args){
    printf(":%s \n", input);
}

int main(){

    bool eof = false;
    char *input;
    char *command;
    char **args;

    //Loop until end of file
    while(!eof){

        //get user input
        input = getUserInput();
        
        //parse user input
        parseInput(input, command, args);


        if (strcmp(input, "exit") == 0) eof = true;
    }
    
    

    
    
    return 0;
}