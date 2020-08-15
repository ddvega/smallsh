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

#ifndef SMALLSH_H
#define SMALLSH_H

/*******************************************************************************
**  Struct and global variable definitions
*******************************************************************************/
struct inputType
{
   char *input;
   char *output;
   int  bgProcess;
};

int foregroundMode =  0; // blocks process from becoming a background process
int childExited    =  0; // tracks exit status of child
int fg_activated    = 0; // keeps previous command from executing after ctrl-z



/*******************************************************************************
**  Function declarations
*******************************************************************************/
int commandBuiltin(char **);
int inputProcess(char *, char **, struct inputType *);
void commandSystem(char **, struct inputType *, struct sigaction *, int *);
void handle_SIGTSTP(int);
void inputGet(char *);
void handle_SIGINT(struct sigaction *, int);
void statusPrint();
char *replace_str(char *str, char *orig, int rep);

#endif