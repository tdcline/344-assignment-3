/* Include statements */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

// Maximum command line length is 2048 characters
// Need two additional for \n that getline() adds and \0 
#define MAX_COMMAND_SIZE 2050
#define MAX_ARGS 512

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
  ssize_t numRead;
  char* finalForm;

  // Referenced getline() man page
  // Read the user's command line entry into the memory pointed to with newCommand
  numRead = getline(&newCommand, &len, stdin);

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

  // Define variables to test against
  bool skipIt = false;
  char blankCheck = '\n';
  char commentCheck = '#';
  
  // Since getline() keeps the \n for a blank line, test for blank line by comparing against \n
  if(blankCheck == commandLine[0] || commentCheck == commandLine[0]) {
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
  if(strcmp(commandLine + strlen(commandLine) - 2, background) == 0) {
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
    printf("Error open output file.\n");
    fflush(stdout);
    exit(1);
  }

  // Redirect standard output to the specified file
  int result = dup2(targetFd, 1);
  if(result == -1) {
    printf("Error redirecting standard output to file.");
    fflush(stdout);
  }

  fcntl(targetFd, F_SETFD, FD_CLOEXEC);

  // Return success
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
  // Testing:
  // test = getenv("PWD");
  // printf("\nThe PWD getenv is %s\n", test);

  // getcwd(testtwo, sizeof(testtwo));
  // printf("\nThe CWD is %s\n", testtwo);

  // Return to previous function
  return 0;
}


/* Process Command */
bool processCommand(struct userCommand *info, int* fgExit) {

  // Establish built-in commands
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
    printf("exit value %d\n", *fgExit);
    fflush(stdout);
  } else if(strcmp(info->command, exitVar) == 0) {
    //set exit condition for loop in startShell()
    done = true;
  } else {
    // Fork
        //fg vs bg foreground first - which waits
    int childStatus;
    pid_t childPid = fork();
    // Error message for fork failure
    if(childPid == -1) {
      perror("fork() failed!");
      fflush(stdout);
      exit(1);
    } else if(childPid == 0) {
      // CHILD PROCESS - do exec here

      printf("We're in the child process pid: %d\n", getpid());
      fflush(stdout);

      if(info->numRedirects > 0) {
        char* greaterThan = ">";
        bool outputFirst = strcmp(info->redirects[0], greaterThan) == 0; 
        if(info->numRedirects == 2 && outputFirst) {
          // redirect output
          redirectOutput(info->redirects[1]);
        } else if(info->numRedirects == 2) {
          // redirect input
          redirectInput(info->redirects[1]);
        } else if(outputFirst){
          // redirect output then input
          redirectOutput(info->redirects[1]);
          redirectInput(info->redirects[3]);
        } else {
          // redirect input then output
          redirectInput(info->redirects[1]);
          redirectOutput(info->redirects[3]);
        }
      }

      printf("numArgs is %d\n", info->numArgs);

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

      execvp(execArgs[0], execArgs);
      // if exec fails
      printf("Error: Exec() failed.");
      fflush(stdout);
      exit(1);

    } else {
      // PARENT PROCESS - wait for child if running in foreground
      pid_t child;
      fflush(stdout);

      if(info->background == false) {
        child = waitpid(childPid, &childStatus, 0);
        if(WIFEXITED(childStatus)) {
          printf("The child exited normally with status %d\n", WEXITSTATUS(childStatus));
          *fgExit = WEXITSTATUS(childStatus);
        } else {
          printf("The child exited abnormally due to signal %d\n", WTERMSIG(childStatus));
          *fgExit = WTERMSIG(childStatus);
        }
      } else {
        printf("background coming up");
      }
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

int startShell(void) {

  // Set up variables for getting and processing commands
  char* newCommand;
  struct userCommand *commandInfo;
  int lastFgExit = 0;
  bool skip = false;
  bool exit = false;
  
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
      // Free original command line
      free(newCommand);
      // Process command
      exit = processCommand(commandInfo, &lastFgExit);
    }

    //free struct
    freeCommandStruct(commandInfo);
  }

  // End shell
  return 0;
}





/* Main function */
int main(void) {


  startShell();
  // at exit: kill existing processes


  

  return EXIT_SUCCESS;
}