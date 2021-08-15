/**
Author: Tanya Yeu (yeut@oreonstate.edu)
Assignment 3: smallsh
CS344 Operating Systems 1
Due Date: 2021-05-03
**/

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h> // for waitpid

int allowBg = 1;


#define MAX_CHAR 2048 //command line max length
#define MAX_ARGS 512  //MAX number of arguments
typedef struct command command;

//protos
void printExitStatus(int childExitMethod);
void expandPid(char* token);

//struct for the command
struct command
{
    char *args[MAX_ARGS]; //array of arguments
    char inputfile[256];
    char outputfile[256];
    int bg; //is background process yay or nay
};



command *initCommand()
{
    command *someCommand = malloc(sizeof(command));
    strcpy(someCommand->inputfile, "");
    strcpy(someCommand->outputfile, "");
    someCommand->bg = 0;
    int i;
    for (i = 0; i < MAX_ARGS; i++)
    {
        someCommand->args[i] = NULL;
    }
    return someCommand;
}

/*
Function: getInput
Parameters: character input, char inputFile, char outputFile, int bg for if background process
Description: This function will get the input from the user and put the 
variables into pointers the follows the general syntax of a command line
command [arg1 arg2..] [< input_file] [> output_file] [&] 
*/
void getInputs(command *someCommand)
{
    char buffer[MAX_CHAR];

    printf(": "); //
    fflush(stdout);
    fgets(buffer, MAX_CHAR, stdin);
    buffer[strcspn(buffer, "\n")] = 0; //remove trailing endline

    //printf("\n%s\n", buffer);

    if (strcmp(buffer, "") == 0)
    {
        return; //cuz this is blank
    }

    //else tokenize that shit into stuff
    char *token;
    char *rest;
    rest = buffer;
    int i = 0;


    while ((token = strtok_r(rest, " ", &rest)))
    {
        //check for input file
        if (strcmp(token, "<") == 0)
        {
            token = strtok_r(rest, " ", &rest);    //fast forward this and get the next arg
            strcpy(someCommand->inputfile, token); //copy that in
        }
        //check for output file
        else if (strcmp(token, ">") == 0)
        {
            token = strtok_r(rest, " ", &rest); //fast forward this and get the next arg
            strcpy(someCommand->outputfile, token);
        }
        //check for background process arg
        else if (strcmp(token, "&") == 0)
        {
            someCommand->bg = 1; //set the background process to true
        }
        else
        {
            if(strstr(token,"$$")!=NULL){
                char* newToken = calloc(MAX_CHAR,sizeof(char));
                strcpy(newToken,token);
                expandPid(newToken);
                someCommand->args[i] = strdup(newToken);
                free(newToken);
            }
            else{
                someCommand->args[i] = strdup(token);
            }
            
            i++;
        }
    }
    
}

void execute(command *c, int* exitStatus, struct sigaction sig)
{
    

    int result;

    if (c->args[0] == NULL){
        return;
    }
    else if(strcmp(c->args[0], "exit")==0){
        exit(0);
    }
    else if (c->args[0][0] == '#')
    {
        return; //this is a comment
    }
    //if args [0] is cd, then do stuff and take it from arg 1 to know the path
    else if (strcmp(c->args[0], "cd")==0){
        if(c->args[1]){                      //if arg was provided
            if(chdir(c->args[1])==-1){      //if file does not exist
                printf("cd: no such file or directory: %s\n", c->args[1]);
                fflush(stdout);
            }
            else {
                chdir(c->args[1]);          //else change to that directory in the first arg
            }
        }
        else{                               //arg not provided, cd to HOME
                chdir(getenv("HOME"));
            }
        return;
    }


    //status prints out the exit status or terminating signal of the last foreground process
    //ran by shell
    else if(strcmp(c->args[0], "status")== 0){
        printExitStatus(*exitStatus);
    }
    //else for not built in functions
    else {
        pid_t spawnPid = -5; //some arbitrary number
        spawnPid = fork();
        switch(spawnPid){
            case -1:
                perror("Hull breach!!\n"); //bad news man
                exit(1);
                break;
            case 0: //all this is basically from exploration i/o
                sig.sa_handler = SIG_DFL;   
                sigaction(SIGINT, &sig, NULL);
                //in the child process
                //if input file
                if(strcmp(c->inputfile,"")!=0){
                    //open source file
                    int infile = open(c->inputfile, O_RDONLY); 
                    if(infile == -1){
                        perror("cannot open file for input ");
                        fflush(stdout);
                    }
                    //write to terminal
                    //printf("infile = %d\n", infile);

                    //redirect to stdin to source file
                    result = dup2(infile, 0);
                    if(result == -1){
                        //perror("source dup2()\n");
                        exit(1);
                    }  
                    //close after exec
                    fcntl(infile, F_SETFD, FD_CLOEXEC);
                }
                //for output file
                if(strcmp(c->outputfile,"")!=0){
                    //open the target outfile
                    int outfile = open(c->outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if(outfile == -1){
                        perror("target outfile open() ");
                        exit(1);
                    }
                    //redirect the stdout to target file
                    result = dup2(outfile,1);
                    if(result==-1){
                        //perror("target dup2()\n");
                        exit(1);
                    }
                    //CLOSE
                    fcntl(outfile, F_SETFD, FD_CLOEXEC);
                }

                //execute
                result = execvp(c->args[0],c->args);
                if(result == -1){
                    perror("");
                    fflush(stdout);
                    exit(1);
                }
            default:
                //in the parent process
                //if background flag = 1, then run this process in the background
                if(allowBg == 1 && c->bg==1){
                    waitpid(spawnPid, exitStatus, WNOHANG);
                    printf("background pid is %d\n", spawnPid);
                    fflush(stdout);
                }
                //wait for child termination
                //normal execution
                else{
                    waitpid(spawnPid, exitStatus, 0);
                }
                while((spawnPid = waitpid(-1, exitStatus, WNOHANG)) > 0){
                    printf("background pid %d is done: ",spawnPid);
                    printExitStatus(*exitStatus);
                }
                
        }
    }
}

/*
Function: resetArgs
Parameters: struct command
Description: will reset all the arguments inside the command back to blanks and nulls
*/
void resetArgs(command *c)
{
    int i;
    for (i = 0; i < MAX_ARGS; i++)
    {
        free(c->args[i]);  //free all the memory allocated to this char array
        c->args[i] = NULL; //set the pointers to null
    }
    strcpy(c->inputfile, "");
    strcpy(c->outputfile, "");
    c->bg = 0;
}

/*
Function: freeCommand(command* c)
Parameters: struct command that needs to be freed
Description: will reset all the args and set them to be blanks and nulls freeing the array and frees the struct
*/

void freeCommand(command* c){
    resetArgs(c);
    free(c);
}



void printExitStatus(int childExitMethod){
        //if process terminates normally, WIFEXITED returns a non zero
    if(WIFEXITED(childExitMethod) != 0){
        //printf("The child process exited normally\n");
        int exitStatus = WEXITSTATUS(childExitMethod);
    printf("exit value %d\n", exitStatus); //this is zero? lol
    }
    //then we can get the actual exit status with this function
    
    //if it was terminated by a signla, then this would return a non zero
    if(WIFSIGNALED(childExitMethod) != 0){
        //printf("The child process was terminated by a signal\n");
        int termSignal = WTERMSIG(childExitMethod);
        printf("terminated by signal %d\n", termSignal);
    }
}

/*
Functionf: expandPid(token)
Parameters: receives a string token that has $$ that needs to be expanded
Description: copies over string without $$ and appends the pid in place
*/
void expandPid(char* token){
    char* newToken = calloc(MAX_CHAR,sizeof(char)); //allocate space for new string
    strncat(newToken, token, strlen(token) -2); //copy without the $$
    char strInt[5] = { 0 };
    sprintf(strInt, "%d", getpid());            //getpid and convert to string
    strcat(&newToken[strlen(token)-2],strInt);  //cat the pid to the end
    strcpy(token,newToken);                     //copy the new token to the old one
    free(newToken);  //free the mem
}


void catchSIGTSTP(int signo){
    if(allowBg == 1){
        char* message = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 49);
        fflush(stdout);
        allowBg = 0;
    }
    else
    {
        char* message = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 29);
        fflush(stdout);
        allowBg = 1;
    }
}

//driver code
int main()
{
    //ignore ctrl c this works just uncomment it later
    struct sigaction SIGINT_action = {{ 0 }};
    SIGINT_action.sa_handler = SIG_IGN; //ignore signals
    sigaction(SIGINT, &SIGINT_action, NULL); //set ctrl c to ignorec

    //ctrl z stuff
    struct sigaction SIGTSTP_action = {{ 0 }};
    SIGTSTP_action.sa_handler = catchSIGTSTP; //set this handler to the function
    sigfillset(&SIGTSTP_action.sa_mask); //block signals arriving while mask is in place
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL); //set ctrl z to the function
    
    
    //int shPid = getpid();
    int exit = 0; //exit flag. if user types exit, this will set to true and exit
    int exitStatus = 0;
    //int allowBg = 1; //this is true and background processes are allowed by default
    
    command* someCommand = initCommand(); //initialize this pointer by allocating space and setting things to null
    do
    {
        getInputs(someCommand);
        
        
        execute(someCommand,&exitStatus, SIGINT_action);
        if(someCommand->args[0] !=NULL && strcmp(someCommand->args[0], "exit") == 0){
            exit = 1;
            break;
        }
        resetArgs(someCommand);
    }while (exit== 0);
    freeCommand(someCommand);
    return 0;
}