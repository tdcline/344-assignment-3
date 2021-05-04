README
Author: Tanner Cline
Due Date: 5/3/21
Description: This program runs a small shell for the user to interact with. The program 
             includes three built-in commands that are run from the shell - cd, status, 
             and exit - and all other commands are run through an exec() function in a 
             forked child process. The shell supports input and output redirection and 
             foreground vs background commands with a foreground-only mode as well. 

Instructions to compile code and run program from the command line:
- Navigate using cd to the folder or space where the main.c file is located
- Enter the following two commands:
    
    gcc --std=gnu99 -o smallsh main.c
    ./smallsh
