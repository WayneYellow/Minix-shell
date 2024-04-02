#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_LENGTH 1024
#define DELIM " \t\r\n\a"

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
    } while (status);
}

char *read_line(void) {
    char *line = NULL;
    size_t bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } else  {
            perror("read_line");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

char **split_line(char *line, int *background, char **file_redirect, char **file_input) {
    int bufsize = MAX_LENGTH, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "split_line: allocation error\n");
        exit(EXIT_FAILURE);
    }

    *background = 0;
    *file_redirect = NULL;
    *file_input = NULL; // Initialize file_input

    token = strtok(line, DELIM);
    while (token != NULL) {
        if (strcmp(token, "&") == 0) {
            *background = 1;
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                fprintf(stderr, "Syntax error: Expected file name after '>'\n");
                exit(EXIT_FAILURE);
            }
            *file_redirect = strdup(token);
        } else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                fprintf(stderr, "Syntax error: Expected file name after '<'\n");
                exit(EXIT_FAILURE);
            }
            *file_input = strdup(token);
        } else {
            tokens[position] = token;
            position++;
        }

        if (position >= bufsize) {
            bufsize += MAX_LENGTH;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "split_line: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int execute(char **args, int background, char *file_redirect, char *file_input) {
    pid_t pid, wpid;
    int status;

    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }

    // Handling the 'cd' command directly in the shell process
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

    if (strcmp(args[0], "exit") == 0) {
        return 0;
    }

    pid = fork();
    if (pid == 0) {
        // This is the child process.
        if (file_redirect != NULL) {
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
    } else if (pid < 0) {
        // Error forking
        perror("fork");
    } else {
        // Parent process
        if (!background) {
            do {
                wpid = waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        } else {
            printf("Process %d running in background\n", pid);
        }
    }

    return 1;
}