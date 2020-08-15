/*******************************************************************************
** Program:       Assignment 2 - CS344
** Author:        David Vega
** Date:          7/18/20
** Description:   In this assignment you will write your own shell, called
**                smallsh. This will work like the bash shell you are used to
**                using, prompting for a command line and running commands, but
**                it will not have many of the special features of the bash shell.
**                Your shell will allow for the redirection of standard input
**                and standard output and it will support both foreground and
**                background processes (controllable by the command line and
**                by receiving signals).
*******************************************************************************/

#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/wait.h>
#include <fcntl.h>
#include<unistd.h>
#include "smallsh.h"

/*******************************************************************************
**  main function
*******************************************************************************/
int main()
{
    char *commandArr[512]; // stores parsed user commands
    int pidArray[1000] = {0}; // keeps track of background processes 1000 max
    char userInput[2048]; // stores user input
    struct inputType status; // stores the names of input and output

    // initialize SIGINT and SIGTSTP structs to be empty
    struct sigaction sigINT = {{0}};
    struct sigaction sigTSTP = {{0}};

    // ctrl-c (terminate child processes but keep disabled in parent)
    handle_SIGINT(&sigINT, 1); // struct set to ignore signals

    // ctrl-z (enter and exit foreground mode)
    sigTSTP.sa_handler = handle_SIGTSTP; // register signal handler
    sigfillset(&sigTSTP.sa_mask); // block other signals while running
    sigTSTP.sa_flags = 0; // no flags set
    sigaction(SIGTSTP, &sigTSTP, NULL); // install signal handler

    // Standby to take and execute commands until user exits shell
    int breakOut = 0; //loop door is closed
    while (!breakOut) //loop as long as door remains closed
    {
        // initialize/reset array and struct
        memset(commandArr, 0, sizeof(commandArr));
        memset(&status, 0, sizeof(status));

        // Enter block if user input is valid and ctrl-z wasn't just activated
        if (!inputProcess(userInput, commandArr, &status) && !fg_activated)
        {
            if (!strcmp(userInput, "exit")) // user wishes to exit loop
            {
                breakOut = 1; // open door
            } else
            {
                // execute built in command if exists else system command
                if (!commandBuiltin(commandArr)) // returns 0 if system command
                    commandSystem(commandArr, &status, &sigINT, pidArray);
            }
        }

        fg_activated = 0; // reset the ctrl-z trip wire
    }

    // kill background processes before exiting
    int i = 0;
    while (pidArray[i])
    {
        kill(pidArray[i], SIGKILL);
        i++;
    }
    return 0;
}

/*******************************************************************************
**  process raw input and parse it into an input array used by execvp
*******************************************************************************/
int inputProcess(char *input, char **inputArr, struct inputType *status)
{
    inputGet(input); // get raw input

    if (!strcmp(input, "exit")) // user wants to exit shell
    {
        inputArr[0] = "exit"; // exit will be handled in outer function
        return 0; // indicates array has been successfully populated
    }

    // user entered an empty line or a comment. Ignore input
    if (strcmp(input, "\0") == 0  || input[0] == '#')
        return 1; // array not populated no command was entered

    char delimit[] = " \t\r\n\v\f"; // white space characters
    char *token = strtok(input, delimit); // pointer to locations in string

    int i = 0;
    while (token != NULL) // loops through arguments in string
    {
        // background process, ignore if used in string that starts with echo
        if (!strcmp(token, "&") && strcmp(inputArr[0], "echo") != 0)
        {
            status->bgProcess = 1; // note that user wants this in background

        } else if (!strcmp(token, "<")) // read from
        {
            token = strtok(NULL, delimit);
            status->input = token; // file to read from
        } else if (!strcmp(token, ">")) // write to
        {
            token = strtok(NULL, delimit);
            status->output = token; // file to write to
        } else
        {
            inputArr[i] = token; // add command to array of commands
        }

        token = strtok(NULL, delimit);
        i++;
    }
    return 0; // indicates array has been successfully populated
}

/*******************************************************************************
**  get and store raw user input.
*******************************************************************************/
void inputGet(char *userInput)
{
    printf(": ");
    fflush(stdout);
    fgets(userInput, 2048, stdin); // read and store user input

    // replace new line character with a terminator
    if (userInput[strlen(userInput) - 1] == '\n')
    {
        userInput[strlen(userInput) - 1] = '\0';
    }

    // validate input line consisting of only empty spaces
    int valid = 0; // set to input not valid
    for(int i = 0; i < strlen(userInput); i++)
    {
        if (userInput[i] != ' ')
            valid = 1; // input is valid
    }
    if (!valid) // not valid
        strcpy(userInput, "\0"); // replace string with null terminator

    // replace every instance of $$ in string with pid
    while (strstr(userInput, "$$"))
    {
        strcpy(userInput, replace_str(userInput, "$$", getpid()));
    }
}

/*******************************************************************************
**  replaces substring with an integer substring. Used to replace $$ with pid.
**  https://www.linuxquestions.org/questions/programming-9/replace-a-substring
**  -with-another-string-in-c-170076/
*******************************************************************************/
char *replace_str(char *str, char *orig, int rep)
{
    static char buffer[2048]; // space for new string
    char *p = strstr(str, orig); // points to substring
    strncpy(buffer, str, p - str); // copy chars from str to orig
    buffer[p - str] = '\0'; // terminate the new string
    sprintf(buffer + (p - str), "%d%s", rep, p + strlen(orig));
    return buffer;
}

/*******************************************************************************
**  runs built in commands cd and status. Returns 0 if command is not built-in.
*******************************************************************************/
int commandBuiltin(char **inputArr) // exit is handled in parser
{
    if (!strcmp(inputArr[0], "cd")) // change directory
    {
        if (!inputArr[1])
        {
            chdir(getenv("HOME")); // go home
        } else if (chdir(inputArr[1]) == -1) // go to directory
        {
            printf("Directory does not exist.\n");
            fflush(stdout);
        }
        return 1; // indicates built in command was entered
    }

    if (!strcmp(inputArr[0], "status")) // get status
    {
        statusPrint(); // print child exit status
        return 1;// indicates built in command was entered
    }

    return 0; // built in command not found must be a system command
}

/*******************************************************************************
**  execute system command by spawning a child in parent shell. Channels commands
**  to foreground or background. Ends by killing zombies.
**  https://repl.it/@cs344/51zombieexc#main.c
*******************************************************************************/
void commandSystem(char **inputArr, struct inputType *status,
                   struct sigaction *s, int *arr)
{
    pid_t childPid = fork(); // spawn a child

    if (childPid == -1) // something went wrong with forking process
    {
        printf("\nFailed forking child..");
        fflush(stdout);
        exit(1);
    } else if (!childPid) // fork worked successfully
    {
        handle_SIGINT(s, 0); // struct set to default, ctrl-c will kill

        // read from file https://repl.it/@cs344/54sortViaFilesc#main.c
        if (status->input)
        {
            // open source file
            int sourceFD = open(status->input, O_RDONLY);
            if (sourceFD == -1)
            {
                printf("cannot open %s for input", status->input);
                fflush(stdout);
                exit(1);
            }

            // redirect stdin to source file
            int result = dup2(sourceFD, 0);
            if (result == -1)
                exit(2);

        }

        // write to file https://repl.it/@cs344/54sortViaFilesc#main.c
        if (status->output)
        {
            // open target file
            int targetFD = open(status->output, O_WRONLY | O_CREAT | O_TRUNC,
                                0644);
            if (targetFD == -1)
                exit(1);

            // redirect stdout to target file
            int result = dup2(targetFD, 1);
            if (result == -1)
                exit(2);
        }

        // execute system command print error message if execution fails
        if (execvp(inputArr[0], inputArr) < 0)
        {
            printf("%s:  No such file or directory\n", inputArr[0]);
            fflush(stdout);
            _Exit(1); // got back to parent
        }
    }

    // in foreground mode (block &) or not a background process
    if (foregroundMode || !status->bgProcess)
    {
        waitpid(childPid, &childExited, 0); // wait to finish
    } else // execute command in the background
    {
        waitpid(childPid, &childExited, WNOHANG); // don't wait to finish
        printf("background pid is %d\n", childPid);
        fflush(stdout);

        // add background pid to pidArray to kill pids when user exits
        for (int i = 0; i < 1000; i++)
        {
            if (!arr[i]) // stops when it finds an open slot in array
            {
                arr[i] = childPid;
                break; // fills slots and breaks out of loop
            }
        }
    }
    // kill zombie processes and print status of dead bg processes
    while ((childPid = waitpid(-1, &childExited, WNOHANG)) > 0)
    {
        int i = 0;
        while (arr[i]) // loop until empty slot is found
        {
            if (childPid == arr[i]) // childPid is a background process
            {
                printf("background pid %d is done. ", childPid);
                statusPrint();
                fflush(stdout);
                break;
            }
            i++;
        }
    }
}

/*******************************************************************************
**  toggles SIGINT (ctrl-c) signal from ignore to default. It is set to ignore
**  in parent and kill in child.
*******************************************************************************/
void handle_SIGINT(struct sigaction *signal, int ignore)
{
    if (ignore) // set to ignore when in parent shell
        signal->sa_handler = SIG_IGN; // set signal to ignore

    if (!ignore) // set to kill when child process is running in foreground
        signal->sa_handler = SIG_DFL; // change from ignore signals to default

    sigfillset(&signal->sa_mask); // ignore other signals
    signal->sa_flags = 0; // no flags
    sigaction(SIGINT, signal, NULL); // install
}

/*******************************************************************************
**  handles the action taken when user enters ctrl-z
*******************************************************************************/
void handle_SIGTSTP(int signo)
{
    char *output;
    if (foregroundMode) // switch off foreground mode
    {
        foregroundMode = 0;
        output = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, output, 31);
    } else // switch on foreground mode
    {
        foregroundMode = 1;
        output = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, output, 51);
    }
    fg_activated = 1; // trip the ctrl-z wire so previous command isn't executed
    fflush(stdout);
}

/*******************************************************************************
**  prints exit status. Exited normally or by signal. https://www.geeksforgeeks.
**  org/exit-status-child-process-linux/
*******************************************************************************/
void statusPrint()
{
    // exited normally
    if (WIFEXITED(childExited))
        printf("exit value %d\n", WEXITSTATUS(childExited));

    // exited by signal
    if (WTERMSIG(childExited))
        printf("terminated by signal %d\n", WTERMSIG(childExited));

    fflush(stdout);
}
