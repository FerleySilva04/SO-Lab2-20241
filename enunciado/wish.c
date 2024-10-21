#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>

#define MAX_PATHS 64
#define MAX_ARGS 64

char *path[MAX_PATHS];  // Stores system paths for command lookup
int path_count = 0;     // Number of paths in PATH

// Function to split a string into tokens
int split_line(char *line, char **tokens) {
    int count = 0;
    char *token = strtok(line, " \t\n");
    while (token != NULL && count < MAX_ARGS) {
        tokens[count++] = token;
        token = strtok(NULL, " \t\n");
    }
    tokens[count] = NULL;  // Terminate the token list
    return count;
}

// Checks if the line contains only spaces or is empty
bool is_blank_line(char *line) {
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] != ' ' && line[i] != '\t' && line[i] != '\n') {
            return false;
        }
    }
    return true;
}

// Searches for the executable in PATH
char *find_executable(char *command) {
    char *full_path = malloc(256);
    for (int i = 0; i < path_count; i++) {
        snprintf(full_path, 256, "%s/%s", path[i], command);
        if (access(full_path, X_OK) == 0) {  // Check if executable
            return full_path;
        }
    }
    free(full_path);
    return NULL;
}

// Changes the working directory
void handle_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Error: Directory argument missing\n");
    } else if (chdir(args[1]) != 0) {
        perror("cd");
    }
}

// Modifies the PATH
void handle_path(char **args) {
    path_count = 0;
    for (int i = 1; args[i] != NULL && path_count < MAX_PATHS; i++) {
        path[path_count++] = strdup(args[i]);
    }
}

// Executes an external command
void run_command(char **args, char *output_file, bool parallel) {
    pid_t pid = fork();  // Create a new process

    if (pid == 0) {  // Child process
        if (output_file != NULL) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                perror("Error opening output file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);  // Redirect standard output to file
            close(fd);
        }
        char *exec_path = find_executable(args[0]);
        if (exec_path == NULL) {
            fprintf(stderr, "Error: Command not found\n");
            exit(EXIT_FAILURE);
        }
        execv(exec_path, args);  // Execute command
        perror("Error executing command");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {  // Parent process
        if (!parallel) {
            waitpid(pid, NULL, 0);  // Wait for child to finish if not parallel
        }
    } else {
        perror("Error creating process");
    }
}

// Processes output redirection if present
void process_redirection(char **args, char **output_file) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            *output_file = args[i + 1];
            args[i] = NULL;  // Terminate args before ">"
            return;
        }
    }
}

// Interprets and executes input commands
void interpret_command(char *line) {
    char *args[MAX_ARGS];
    int arg_count = split_line(line, args);
    if (arg_count == 0 || is_blank_line(line)) {
        return;  // Do nothing if the line is empty
    }

    bool parallel = false;
    if (strcmp(args[arg_count - 1], "&") == 0) {
        parallel = true;  // Detect if the command should be parallel
        args[arg_count - 1] = NULL;  // Remove "&" from args
    }

    char *output_file = NULL;
    process_redirection(args, &output_file);

    if (strcmp(args[0], "exit") == 0) {
        exit(0);  // Exit command
    } else if (strcmp(args[0], "cd") == 0) {
        handle_cd(args);  // Change directory
    } else if (strcmp(args[0], "path") == 0) {
        handle_path(args);  // Modify PATH
    } else {
        run_command(args, output_file, parallel);  // Execute external command
    }
}

// Interactive mode
void interactive_mode() {
    char *line = NULL;
    size_t len = 0;

    while (1) {
        printf("wish> ");
        if (getline(&line, &len, stdin) == -1) {
            break;  // End of input
        }
        interpret_command(line);  // Interpret the entered line
    }
    free(line);
}

// Batch mode
void batch_mode(char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening batch file");
        exit(EXIT_FAILURE);
    }

    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, file) != -1) {
        interpret_command(line);  // Interpret each line in the file
    }

    free(line);
    fclose(file);
}

// Main function
int main(int argc, char *argv[]) {
    // Initialize default PATH
    path[0] = "/bin";
    path_count = 1;

    if (argc == 1) {
        interactive_mode();  // Interactive mode
    } else if (argc == 2) {
        batch_mode(argv[1]);  // Batch mode
    } else {
        fprintf(stderr, "Error: Incorrect usage\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
