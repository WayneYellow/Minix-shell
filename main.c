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
char **split_line(char *line, int *background, char **file_redirect_in, char **file_redirect_out, char ***pipe_commands);
int execute(char **args, int background, char *file_redirect_in, char *file_redirect_out, char **pipe_commands);

int main() {
    // Run command input loop.
    loop();

    // Exit the shell.
    return EXIT_SUCCESS;
}

void loop(void) {
    char *line;
    char **args;
    char *file_redirect_in = NULL;
    char *file_redirect_out = NULL;
    char **pipe_commands = NULL;
    int status;
    int background;

    do {
        printf(">> ");
        line = read_line();
        args = split_line(line, &background, &file_redirect_in, &file_redirect_out, &pipe_commands);
        status = execute(args, background, file_redirect_in, file_redirect_out, pipe_commands);

        free(line);
        free(args);
        if (file_redirect_in != NULL) {
            free(file_redirect_in);
            file_redirect_in = NULL;
        }
        if (file_redirect_out != NULL) {
            free(file_redirect_out);
            file_redirect_out = NULL;
        }
        if (pipe_commands != NULL) {
            free(pipe_commands);
            pipe_commands = NULL;
        }
    } while (status);
}

char *read_line(void) {
    char *line = NULL;
    size_t bufsize = 0; // getline will allocate a buffer for us.

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);  // We received an EOF
        } 
        else  {
            perror("read_line");
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

char **split_line(char *line, int *background, char **file_redirect_in, char **file_redirect_out, char ***pipe_commands) {
    int bufsize = MAX_LENGTH, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    *background = 0;
    if (!tokens) {
        fprintf(stderr, "split_line: allocation error\n");
        exit(EXIT_FAILURE);
    }

    *file_redirect_in = NULL;
    *file_redirect_out = NULL;
    *pipe_commands = NULL;


    token = strtok(line, DELIM);
    while (token != NULL) {
        if (strcmp(token, "&") == 0) {
            *background = 1;
            break;
        }
        else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                fprintf(stderr, "split_line: syntax error\n");
                exit(EXIT_FAILURE);
            }
            *file_redirect_out = token;
            break;
        }
        else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                fprintf(stderr, "split_line: syntax error\n");
                exit(EXIT_FAILURE);
            }
            *file_redirect_in = token;
            break;
        }
        else if (strcmp(token, "|") == 0) {
            *pipe_commands = malloc(2 * sizeof(char*));
            (*pipe_commands)[0] = tokens[position - 1];
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                fprintf(stderr, "split_line: syntax error\n");
                exit(EXIT_FAILURE);
            }
            (*pipe_commands)[1] = token;
            break;
        }
        else {
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

int execute(char **args, int background, char *file_redirect_in, char *file_redirect_out, char **pipe_commands) {
    pid_t pid, wpid;
    int status;
    int in_fd, out_fd, pipe_fd[2];
    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }

    // handdle the cd command internally
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "cd: expected argument to \"cd\"\n");
        } 
        else {
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
        if (file_redirect_in != NULL) {
            in_fd = open(file_redirect_in, O_RDONLY);
            if (in_fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (dup2(in_fd, STDIN_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(in_fd);
        }

        if (file_redirect_out != NULL) {
            out_fd = open(file_redirect_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (dup2(out_fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(out_fd);
        }

        if (pipe_commands != NULL) {
            if (pipe(pipe_fd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            pid_t pid2 = fork();
            if (pid2 == 0) {
                if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                if (execvp(pipe_commands[0], args) == -1) {
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
            }
            else if (pid2 < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            if (execvp(pipe_commands[1], args) == -1) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }

        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "%s: not found\n", args[0]);
        }
        exit(EXIT_FAILURE);
    } 
    else if (pid < 0) {
        // Error forking
        perror("fork");
    } 
    else {
        // This is the parent process.
        if (!background) {
            do {
                wpid = waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
        else {
            printf("Background process started: %d\n", pid);
        }
    }

    return 1;
}