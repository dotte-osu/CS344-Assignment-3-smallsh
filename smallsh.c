#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h> // getpid, getppid

/* struct for commands */
struct inputData{
    char    *command;
    char    *args;
    char    *input_file;
    char    *output_file;
    bool    isBackground;
    char    *comment;
    bool    isComment;
};

//Function to replace string
//reference https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-word/
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

struct inputData *parseInput(char *input){
    struct inputData *currData = malloc(sizeof(struct inputData));
    
    //Check for comment
    if(input[0] == '#') {
        currData->comment = calloc(strlen(input) + 1, sizeof(char));
        strcpy(currData->comment, input);
        currData->isComment = true;
        //printf("comment %s\n", currData->comment);
        return currData;
    }

    //Get command
    char *saveptr;
    
    //First word is always command
    char *token = strtok_r(input, " ", &saveptr);
    currData->command = calloc(strlen(token) + 1, sizeof(char));
    strcpy(currData->command, token);
    //printf("command %s\n", currData->command);

    //args, input file, output file
    while(token){
        //printf("token %s\n", token);
        if(strcmp(token, "<") == 0 ){
            token = strtok_r(NULL, " ", &saveptr);
            if(!token) return currData;
            currData->input_file = calloc(strlen(token) + 1, sizeof(char));
            strcpy(currData->input_file, token);
            //printf("input_file %s\n", currData->input_file);
        }
        else if(strcmp(token, ">") == 0 ){
            token = strtok_r(NULL, " ", &saveptr);
            if(!token) return currData;
            currData->output_file = calloc(strlen(token) + 1, sizeof(char));
            strcpy(currData->output_file, token);
            //printf("output_file %s\n", currData->output_file);
 
        }else if(strcmp(token, "&") == 0 ){
            token = strtok_r(NULL, "\n", &saveptr);
            if(!token) return currData;
            currData->isBackground = true;

        }
        else{

            token = strtok_r(NULL, "\n", &saveptr);
            if(!token) return currData;
            currData->args = calloc(strlen(token) + 1, sizeof(char));
            strcpy(currData->args, token);
            //printf("args %s\n", currData->args);
            return currData;
        }
    }
    


    return currData;
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
        struct inputData *currData = parseInput(input);


        if (strcmp(input, "exit") == 0) eof = true;
    }
    
    

    
    
    return 0;
}