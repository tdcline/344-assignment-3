/* Include statements */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Maximum command line length is 2048 characters
// Need two additional for \n that getline() adds and \0 
#define MAX_COMMAND_SIZE 2050
#define MAX_ARGS 512

/* Create struct for elements of command line */
struct userCommand {
  char* command;
  char* args[MAX_ARGS];
  int numArgs;
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

/* Reads the user's command and returns it as a string. A blank line will return 
   as a \n character */
char* getCommand(void) {

  // Allocate variables for reading the user input
  char* newCommand = malloc(sizeof(MAX_COMMAND_SIZE));
  size_t len = 0;
  ssize_t numRead;

  // Referenced getline() man page
  // Read the user's command line entry into the memory pointed to with newCommand
  numRead = getline(&newCommand, &len, stdin);

  // getline() adds the newline character to the end of the string
  // Replace it with null-termination for non-blank lines since we don't need it
  if(strlen(newCommand) > 1) {
    newCommand[strlen(newCommand)-1] = '\0';
  }

  // Return the user's command as a string
  return newCommand;
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

  // Return 
  return skipIt;
}

/* Parse command */
struct userCommand *parseCommand(char* commandLine) {
  
  char* saveptr;
  int numArgs = 0;
  int numRedirects = 0;
  char* inputR = "<";
  char* outputR = ">";
  char* background = " &";
  bool redirectReached = false;

  struct userCommand *newCommand = malloc(sizeof(struct userCommand));

  
  // Check for background first
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

  newCommand->numArgs = numArgs;
  newCommand->numRedirects = numRedirects;

  free(token);
  return newCommand;
}



/* Main function */
int main(void) {
  
  // allocate variables
  char* newCommand;
  struct userCommand *commandInfo;
  bool skip = false;

  // display new line
  displayLine();

  // get new command
  newCommand = getCommand();
  skip = skipTest(newCommand);
  if(skip != true) {
    // parse command elements
    commandInfo = parseCommand(newCommand);
    free(newCommand);

    // testing
    for(int i = 0; i < commandInfo->numRedirects; i++) {
      printf("%s\n", commandInfo->redirects[i]);
    }
  }

  // process/do command

  // cleanup each time

  
  
  
  

  return EXIT_SUCCESS;
}