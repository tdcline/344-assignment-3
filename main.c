/* Include statements */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

// Maximum command line length is 2048 characters
// Need two additional for \n that getline() adds and \0 
#define MAX_COMMAND_SIZE 2050
#define MAX_ARGS 512

// Global variable for foreground only mode
int foregroundOnly = 0;

/* Create struct for elements of command line */
struct userCommand {
  char* command;
  // Add 2 to args array for initial command and NULL ending element
  char* args[MAX_ARGS];
  int numArgs;
  // redirects array holds up to 4 elements: the redirect symbols and their filenames
  char* redirects[4];
  int numRedirects;
  bool background;
};

/* Create struct to store the exit status or signal code of the last 
   foreground process. */
struct lastStatus {
  int code;
  bool signal;
};

/* Displays the ': ' characters to the terminal, creating the frontend 
   for the shell */
int displayLine(void) {
  printf(": ");
  fflush(stdout);

  return 0;
}

/* Replaces the instance of $$ in a string with the program's pid. Takes 
   two arguments: the pointer to the start of the user's command line and a 
   pointer to where $$ was found in the same string. */
char* expandVar(char* start, char* foundVar) {

  // Set up variables for replacing characters in a string
  char* newLine;
  int startLen = strlen(start);
  int varToEnd = strlen(foundVar);
  int mainPid;
  // Create a temp array to store the pid as a string in - max 10 chars goes up to 9,999,999,999
  char temp[10];
  // Find number of characters between start of string and instance of $$
  int firstChars = startLen - varToEnd;
  
  // Get smallsh pid, store in teh temp array, and get string length
  mainPid = getpid();
  sprintf(temp, "%d", mainPid);
  int pidLen = strlen(temp);

  // Allocate space for new string = starting string length - 2 for the $$ plus the pid Length + 1 for the null terminator
  newLine = calloc((startLen+pidLen-1), sizeof(char));

  // Copy into newLine ptr the first part of the string
  strncpy(newLine, start, firstChars);

  // Copy into newLine ptr the pid
  strcpy((newLine+firstChars), temp);

  // Copy into newLine ptr the ending part of the string
  strcpy((newLine+firstChars+pidLen), foundVar+2);

  // Free original memory space since new pointer will replace
  free(start);

  // Return pointer for the new string
  return newLine;
}

/* Takes a string representing the user's command, checks if the command line 
   contains any instances of $$, and replaces those instances with the smallsh 
   pid. Returns a pointer to the string representing the user's command line 
   after revision if necessary. */
char* expansionCheck(char* commandLine) {

  // Set up variables for searching for $$
  char* searchPtr = commandLine;
  char* needle = "$$";
  char* found;
  char* toReturn;
  bool changed = false;
  bool done = false;

  // Loop makes sure to search for multiple instances of $$
  while(done != true) {
    // Use strstr() to search for $$
    found = strstr(searchPtr, needle);
    // found is NULL if end of string is reached without any instances of $$
    if(found == NULL) {
      done = true;
    } else {
      // If $$ is found, replace with the smallsh pid
      searchPtr = expandVar(searchPtr, found);
      changed = true;
    }
  }

  // Set return pointer to original pointer if unchanged
  if(changed == false) {
    toReturn = commandLine;
  } else {
    // Set return pointer to updated search pointer if $$ found
    toReturn = searchPtr;
  }

  // Return the appropriate pointer for the string
  return toReturn;
}

/* Reads the user's command and returns it as a string. A blank line will return 
   as a \n character */
char* getCommand(void) {

  // Allocate variables for reading the user input
  char* newCommand = malloc(sizeof(MAX_COMMAND_SIZE));
  size_t len = 0;
  char* finalForm;

  // Referenced getline() man page
  // Read the user's command line entry into the memory pointed to with newCommand
  getline(&newCommand, &len, stdin);

  // getline() adds the newline character to the end of the string
  // Replace it with null-termination for non-blank lines since we don't need it
  if(strlen(newCommand) > 1) {
    newCommand[strlen(newCommand)-1] = '\0';
  }
  
  // Test for and expand $$ variables
  finalForm = expansionCheck(newCommand);

  // Return the user's command as a string
  return finalForm;
}

/* Test the a command line entry for blank line or commented line. Returns true if 
   the command is a blank line or begins with a '#' */
bool skipTest(char* commandLine) {

  // Define skip condition
  bool skipIt = false;

  // Check for 
  if(commandLine[0] == '#' || commandLine[0] == '\n') {
    skipIt = true;
  }

  // Return bool result
  return skipIt;
}

/* Takes the user's command line pointer as an argument and parses it into a 
   userCommand struct. */
struct userCommand *parseCommand(char* commandLine) {
  
  // Set up variables for parsing into the command line into struct
  char* saveptr;
  int numArgs = 0;
  int numRedirects = 0;
  char* inputR = "<";
  char* outputR = ">";
  char* background = " &";
  bool redirectReached = false;

  // Allocate space for new struct
  struct userCommand *newCommand = malloc(sizeof(struct userCommand));

  // Check for background character first
  if((strcmp(commandLine + strlen(commandLine) - 2, background) == 0) && foregroundOnly == 0) {
    newCommand->background = true;
  } else {
    newCommand->background = false;
  }

  // Process command
  char* token = strtok_r(commandLine, " ", &saveptr);
  newCommand->command = strdup(token);

  // Begin processing additional information
  token = strtok_r(NULL, " ", &saveptr);

  while(token != NULL) {
    // If redirect symbol reached, there are no more arguments
    if(redirectReached || strcmp(token, inputR) == 0 || strcmp(token, outputR) == 0) {
      // Ensure token is not the ampersand
      if(strcmp(token, background + 1) != 0) {
        redirectReached = true;
        newCommand->redirects[numRedirects] = strdup(token);
        numRedirects++;
      }
    // Otherwise, still loading arguments
    } else {
      if(strcmp(token, background + 1) != 0) {
        newCommand->args[numArgs] = strdup(token);
        numArgs++;
      }
    }
    // Parse to next piece of information in the command
    token = strtok_r(NULL, " ", &saveptr);
  }

  // Store number of arguments and redirects for later use
  newCommand->numArgs = numArgs;
  newCommand->numRedirects = numRedirects;
  // Free the token pointer and return the build struct
  free(token);
  return newCommand;
}

/* Redirect standard input to the inputFile argument. */
int redirectInput(char* inputFile) {

  // Open the new passed in file in read only mode
  int sourceFd = open(inputFile, O_RDONLY);
  if(sourceFd == -1) {
    printf("Error opening source file.\n");
    fflush(stdout);
    exit(1);
  }

  // Redirect standard input to specified file
  int result = dup2(sourceFd, 0);
  if(result == -1) {
    printf("Error redirecting standard input from file.\n");
    fflush(stdout);
    exit(1);
  }

  // Return success
  return 0;
}

/* Redirect standard output to the outputFile argument. */
int redirectOutput(char* outputFile) {

  // Open the new target file for output in write only mode
  int targetFd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if(targetFd == -1) {
    printf("Error opening output file.\n");
    fflush(stdout);
    exit(1);
  }

  // Redirect standard output to the specified file
  int result = dup2(targetFd, 1);
  if(result == -1) {
    printf("Error redirecting standard output to file.");
    fflush(stdout);
  }

  // fcntl(targetFd, F_SETFD, FD_CLOEXEC);

  // Return success
  return 0;
}

/* Checks for any terminated background processes. Displays message regarding 
   terminated process and termination code or exit value. */
int checkBg(void) {

  // Set up variables to wait on pids
  pid_t childPid;
  int childStatus;

  // Loop unitl waitpid returns a 0 marking no recent child process terminations
  // Concept of using waitpid with -1 and WNOHANG referenced from Exploration: Process API 
  // - Monitoring Child Processes Interpreting the Termination Status example with added loop. 
  while((childPid = waitpid(-1, &childStatus, WNOHANG)) > 0) {
    // Display message regarding ending process
    printf("background pid %d is done: ", childPid);

    // Continue message with exit status or termination code
    if(WIFEXITED(childStatus)) {
      printf("exit value %d\n", WEXITSTATUS(childStatus));
    } else {
      printf("terminated by signal %d\n", WTERMSIG(childStatus));
    }
    fflush(stdout);
  }

  return 0;  
}

/* Changes the current directory to the one in HOME environment variable or to 
   the path specified by the user if given. Takes a struct of the user's command 
   information as an argument. */
int changedir(struct userCommand *info) {

  // Set up variables to change directory
  char* desiredDir;
  int success;
  // Test vars:
  // char* test;
  // char testtwo[256];

  // Check if the user passed in an argument with cd
  if(info->numArgs == 0) {
    // No path given means change to directory specified in HOME environment var
    desiredDir = getenv("HOME");
    success = chdir(desiredDir);
  } else if(info->numArgs == 1) {
    // Path given means change to specified path
    success = chdir(info->args[0]);
  } else {
    printf("Error: too many arguments.\n");
    fflush(stdout);
  }
  // Print error message if change was unsuccessful
  if(success != 0) {
    printf("\nError: chdir unsuccessful.\n");
    fflush(stdout);
  }

  // Return to previous function
  return 0;
}

/* Displays the exit status code or termination signal code of the last 
   foreground process terminated. */
int statusDisplay(struct lastStatus *status) {

  // Check whether signal or exit code
  if(status->signal == true) {
    printf("terminated by signal %d\n", status->code);
  } else {
    printf("exit value %d\n", status->code);
  }
  fflush(stdout);
  
  return 0;
}

/* Processes and executes the user's command. Implements built-in commands from 
   the shell and all other foreground or background commands with an exec() 
   function. */
bool processCommand(struct userCommand *info, struct lastStatus *exitStatus, struct sigaction ignoreInt, struct sigaction handleTSTP) {

  // Establish vars for built-in commands and exit condition
  char* changeDir = "cd";
  char* status = "status";
  char* exitVar = "exit";
  bool done = false;

  // Check for built-in commands first
  if(strcmp(info->command, changeDir) == 0) {
    // cd function
    changedir(info);
  } else if(strcmp(info->command, status) == 0) {
    // Status function
    statusDisplay(exitStatus);
  } else if(strcmp(info->command, exitVar) == 0) {
    // Set exit condition for loop in startShell()
    done = true;
  } else {
    // Fork to run exec() in child process
    int childStatus;
    pid_t childPid = fork();

    // Error message for fork failure
    if(childPid == -1) {
      perror("fork() failed!\n");
      fflush(stdout);
      exit(1);
    } else if(childPid == 0) {
      // CHILD PROCESS
      // Set to ignore STSP
      handleTSTP.sa_handler = SIG_IGN;
      sigaction(SIGTSTP, &handleTSTP, NULL);
      
      // Restore SIGINT for FG
      if(info->background != true) {
        ignoreInt.sa_handler = SIG_DFL;
        sigaction(SIGINT, &ignoreInt, NULL);
      }

      // Ran out of time to clean up and test. In between the lines is redirecting
      // standard input and output. Would put into separate function with more time.
      // --------------------------------------------------------------------
      // Allocate a string for /dev/null background commands
      char* tempString = "/dev/null";
      char* devNull = strdup(tempString);

      // Redirect standard in or standard out if needed
      if(info->numRedirects > 0) {
        char* greaterThan = ">";
        bool outputFirst = strcmp(info->redirects[0], greaterThan) == 0; 

        if(info->numRedirects == 2 && outputFirst) {
          // Redirect output only
          redirectOutput(info->redirects[1]);
          if(info->background == true){
            redirectInput(devNull);
          }
        } else if(info->numRedirects == 2) {
          // Redirect input only 
          redirectInput(info->redirects[1]);
          if(info->background == true){
            redirectOutput(devNull);
          }
        } else if(outputFirst){
          // Redirect output then input
          redirectOutput(info->redirects[1]);
          redirectInput(info->redirects[3]);
        } else {
          // Redirect input then output
          redirectInput(info->redirects[1]);
          redirectOutput(info->redirects[3]);
        }
      } else if(info->background == true) {
        redirectInput(devNull);
        redirectOutput(devNull);
      }
      
      // Free devNull pointer that was created with strdup()
      free(devNull);
      // --------------------------------------------------------------------
      
      // Set up pass to execArgs
      int toPass = info->numArgs + 2;
      char* execArgs[toPass];

      // Modify arguments for passing into execvp() by adding command as the 
      // first element and NULL as the last element
      for(int a = 0; a < toPass; a++) {
        if(a == 0) {
          execArgs[a] = info->command;
        } else if(a == (toPass-1)) {
          execArgs[a] = NULL;
        } else {
          execArgs[a] = info->args[a-1];
        }
      }

      // Run command through execvp
      execvp(execArgs[0], execArgs);
      
      // if exec fails
      printf("Error: exec() failed to run that command.\n");
      fflush(stdout);
      exit(1);

    } else {
      // PARENT PROCESS
      // Wait for child if running in foreground
      fflush(stdout);
      // Foreground command
      if(info->background == false) {
        // Wait for child to finish since foreground command
        waitpid(childPid, &childStatus, 0);
        // Document exit status or signal code
        if(WIFEXITED(childStatus)) {
          exitStatus->signal = false;
          exitStatus->code = WEXITSTATUS(childStatus);
        } else {
          exitStatus->signal = true;
          exitStatus->code = WTERMSIG(childStatus);
          // If killed by a signal - display signal number
          printf("terminated by signal %d\n", exitStatus->code);
          fflush(stdout);
        }
      // Background command
      } else {
        // Display message for starting background command
        printf("The background pid is %d\n", childPid);
        fflush(stdout);
      }
      checkBg();
    }
  }
  return done;
}

/* Takes a userCommand struct as an argument and deallocates the memory of its 
   variables and the struct. */
int freeCommandStruct(struct userCommand *info) {

  // Free command
  free(info->command);

  // Free args
  for(int i = 0; i < info->numArgs; i++) {
    free(info->args[i]);
  }
  
  // Free redirects
  for(int m = 0; m < info->numRedirects; m++) {
    free(info->redirects[m]);
  }

  // Free the struct
  free(info);

  return 0;
}

/* Handler function that ignores the SIGTSTP signal and toggles in or out 
   of foreground-only mode. */ 
void catchSIGTSTP(int signo) {

  // Set up initial variable to hold message to display
  char* message;

  // Toggle global variable keeping track of foreground only mode
  // Display appropriate message
  if(foregroundOnly == 0) {
    // Toggle into foreground-only mode
    message = "\nEntering foreground-only mode (& is now ignored)\n: ";
    write(STDOUT_FILENO, message, 52);
    foregroundOnly = 1;
  } else {
    // Toggle out of foreground-only mode
    message = "\nExiting foreground-only mode\n: ";
    write(STDOUT_FILENO, message, 32);
    foregroundOnly = 0;
  }
}

/* Starts the program putting the user into the shell. */
int startShell(void) {

  // Set up variables for getting and processing commands
  char* newCommand;
  struct userCommand *commandInfo;
  bool skip = false;
  bool exit = false;

  // Set up sigaction structs for SIGINT and SIGTSTP
  struct sigaction SIGINT_ignore = {{0}};
  struct sigaction SIGTSTP_handle = {{0}};
  
  // Ignore SIGINT and handle SIGTSTP with catchTSTP function
  SIGINT_ignore.sa_handler = SIG_IGN;
  SIGTSTP_handle.sa_handler = catchSIGTSTP;
  SIGTSTP_handle.sa_flags = SA_RESTART;

  // Initialize signal handlers
  sigaction(SIGINT, &SIGINT_ignore, NULL);
  sigaction(SIGTSTP,  &SIGTSTP_handle, NULL);

  // Initialize status struct members
  struct lastStatus *status = malloc(sizeof(struct lastStatus));
  status->code = 0;
  status->signal = false;

  // Loop to stay in shell
  while(exit != true) {
    // Display new line and get new command
    displayLine();
    newCommand = getCommand();

    // Check for blank line or comment line
    skip = skipTest(newCommand);
    
    if(skip != true) {
      // Parse command elements into struct
      commandInfo = parseCommand(newCommand);

      // Process command
      exit = processCommand(commandInfo, status, SIGINT_ignore, SIGTSTP_handle);
      freeCommandStruct(commandInfo);
    }
    // Free pointer and check for terminated background processes
    free(newCommand);
    checkBg();
  }
  // Free pointer for storing status
  free(status);
  // End shell
  return 0;
}

/* Main function */
int main(void) {

  // Start the shell
  startShell();

  // Exit the program
  return EXIT_SUCCESS;
}