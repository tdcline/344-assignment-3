/* Include statements */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Maximum command line length is 2048 characters
// Need two additional for \n that getline() adds and \0 
#define MAX_COMMAND_SIZE 2050
#define MAX_ARGS 512

/* Create struct for elements of command line */
struct userCommand {
  char* command;
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

/* Process Command */
int processCommand(struct userCommand *info) {

  // Establish built-in commands
  char* changeDir = "cd";
  char* status = "status";
  char* exit = "exit";

  // Check for built-in commands first
  if(strcmp(info->command, changeDir) == 0) {
    // CD function
  } else if(strcmp(info->command, status) == 0) {
    // Status function
  } else if(strcmp(info->command, exit) == 0) {
    // Exit function
  } else {
    // Fork
    // Exec()
    
  }




  return 0;
}







/* Main function */
int main(void) {
  
  // Set up variables for getting command info from user
  char* newCommand;
  struct userCommand *commandInfo;
  bool skip = false;

  // Display new line
  displayLine();

  // Get new command from user
  newCommand = getCommand();
  // Check if blank line or comment line
  skip = skipTest(newCommand);
  if(skip != true) {
    // Parse command elements into struct
    commandInfo = parseCommand(newCommand);
    free(newCommand);

    // Process command
    processCommand(commandInfo);

  }

  // process/do command

  // cleanup each time

  
  
  
  

  return EXIT_SUCCESS;
}