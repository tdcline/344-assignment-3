/* Include statements */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Maximum command line length is 2048 characters
// Need two additional for \n that getline() adds and \0 
#define MAX_COMMAND_SIZE 2050

/* Create struct for elements of command line */
struct userCommand {
  char* command = NULL;
  char* args = NULL;
  char* inputFile;
  char* outputFile;
  bool background;
};

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
int parseCommand(char* commandLine) {

  


  return 0;
}



/* Main function */
int main(void) {
  
  // allocate variables
  char* userCommand;
  bool skip = false;

  // display new line
  displayLine();

  // get new command
  userCommand = getCommand();
  skip = skipTest(test);
  if(skip != true) {
    // parse command elements
    parseCommand(userCommand);

  }

  // process/do command

  // cleanup each time

  free(userCommand);
  
  
  

  return EXIT_SUCCESS;
}