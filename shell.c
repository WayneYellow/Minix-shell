#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_LENGTH 1024
#define DELIM " \t\r\n\a"

//This is a basic shell implementation that run on minix 3.3.0 os
//support bisic command with or without arguments, 
//background excecuting with indicator '&',
//input and output file redirect using '<' and '>',
//file pipe line using '|'.

// Function prototypes
void loop(void);
char *read_line(void);
char **split_line(char *line, int *background, char **file_redirect, char **file_input);
int execute(char **args, int background, char *file_redirect, char *file_input);

int main() {
    // Run command input loop.
    loop();

    // Exit the shell.
    return EXIT_SUCCESS;
}

// main loop
void loop(void) {
    char *line;
    char **args;
    char *file_redirect = NULL;
    char *file_input = NULL;
    int status, background;

    do {
        printf(">> ");
        line = read_line();
        args = split_line(line, &background, &file_redirect, &file_input);
        status = execute(args, background, file_redirect, file_input);

        free(line);
        free(args);
        if (file_redirect != NULL) {
            free(file_redirect);
            file_redirect = NULL;
        }
        if (file_input != NULL ) {
            free(file_input);
            file_input = NULL;
        }
    } while (status);
}

//read from the stdin one line
char *read_line(void) {
    char *line = NULL;
    size_t bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } 
        else {
            perror("read_line");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

// Function to split a line into tokens
char **split_line(char *line, int *background, char **file_redirect, char **file_input) {
    int bufsize = MAX_LENGTH, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    // Check if allocation was successful
    if (!tokens) {
        fprintf(stderr, "split_line: allocation error\n");
        exit(EXIT_FAILURE);
    }

    *background = 0; //is run in background
    *file_redirect = NULL; //output file name
    *file_input = NULL; // input file name

    // Tokenize the input line
    token = strtok(line, DELIM);
    while (token != NULL) {
        // Check for background process indicator
        if (strcmp(token, "&") == 0) {
            *background = 1;
        }
        // Check for output redirection
        else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                fprintf(stderr, "Syntax error: Expected file name after '>'\n");
                exit(EXIT_FAILURE);
            }
            *file_redirect = strdup(token);
        } 
        // Check for input redirection
        else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                fprintf(stderr, "Syntax error: Expected file name after '<'\n");
                exit(EXIT_FAILURE);
            }
            *file_input = strdup(token);
        } 
        //regular tokens
        else {
            tokens[position] = token;
            position++;
        }

        // Resize tokens buffer if needed
        if (position >= bufsize) {
            bufsize += MAX_LENGTH;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "split_line: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        // Get next token
        token = strtok(NULL, DELIM);
    }
    // Set NULL terminator to indicate end of tokens
    tokens[position] = NULL;
    return tokens;
}

//deal with commands and arguments
int execute(char **args, int background, char *file_redirect, char *file_input) {
    int pipefd[2];// files for pipe
    pid_t pid1, pid2, wpid;// process IDs
    int pipe_pos = -1;// Position of the pipe in arguments
    int status; // Status of child process

    // Check for a pipe in the arguments
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_pos = i;
            break;
        }
    }

    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }

    // Handle the 'cd' and 'exit' commands directly in the shell process
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "expected argument to \"cd\"\n");
        } else {
            if (chdir(args[1]) != 0) {
                perror("cd");
            }
        }
        return 1;
    } 
    else if (strcmp(args[0], "exit") == 0) {
        // Exit the shell
        return 0;
    }

    if (pipe_pos != -1) {
        // Found a pipe, set up pipe and fork two processes
        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        args[pipe_pos] = NULL;  // Split the arguments into two parts

        pid1 = fork();
        if (pid1 == 0) {
            // First child process
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);

            // Execute the command use execvp
            if (execvp(args[0], args) == -1) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } 
        else if (pid1 < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        // Fork second child process
        pid2 = fork();
        if (pid2 == 0) {
            // Second child process
            dup2(pipefd[0], STDIN_FILENO);// Redirect stdin to read end of pipe
            close(pipefd[1]);
            close(pipefd[0]);
            
            // Execute the command after the pipe
            if (execvp(args[pipe_pos + 1], &args[pipe_pos + 1]) == -1) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } 
        else if (pid2 < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        // Close both ends of the pipe in the parent process
        close(pipefd[0]);
        close(pipefd[1]);

        // Wait for both child processes to finish
        waitpid(pid1, &status, 0);
        waitpid(pid2, &status, 0);
    } 
    else {
        // No pipe found in arguments, proceed with regular execution

        // Fork a child process
        pid1 = fork();
        if (pid1 == 0) {
            // This is the child process.
            if (file_redirect != NULL) {
                //handle output redirection
                int fd = open(file_redirect, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd);
            }

            if (file_input != NULL) {
                // Handle input redirection
                int fd_input = open(file_input, O_RDONLY);
                if (fd_input == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_input, STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd_input);
            }

            if (execvp(args[0], args) == -1) {
                fprintf(stderr, "%s: not found\n", args[0]);
            }
            exit(EXIT_FAILURE);
        } 
        else if (pid1 < 0) {
            // Error forking
            perror("fork");
        } 
        else {
            // Parent process
            if (!background) {
                do {
                    wpid = waitpid(pid1, &status, WUNTRACED);
                } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            } 
            else {
                printf("Process %d running in background\n", pid1);
            }
        }
    }
    return 1;
}