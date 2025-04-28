#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define PATH_MAX 4096

// Environment variables
char **environ_vars = NULL;
int environ_count = 0;
int environ_size = 0;

// Function prototypes
void initialize_environment();
void add_environment_variable(const char *name, const char *value);
char *get_environment_variable(const char *name);
void set_environment_variable(const char *name, const char *value);
char **tokenize(char *line, const char *delim);
void free_tokens(char **tokens);
int execute_builtin(char **tokens);
int handle_redirection(char **tokens);
int execute_command(char **tokens);
void execute_commands(char *input);
int execute_logical_commands(char *input);
int execute_pipe_commands(char *input);
char *find_executable(const char *cmd);

// Initialize environment variables
void initialize_environment() {
    environ_size = 10;
    environ_vars = (char **)malloc(environ_size * sizeof(char *));
    
    // Set default PATH
    char *path = getenv("PATH");
    if (path) {
        add_environment_variable("PATH", path);
    } else {
        add_environment_variable("PATH", "/bin:/usr/bin:/usr/local/bin");
    }
    
    // Set HOME
    char *home = getenv("HOME");
    if (home) {
        add_environment_variable("HOME", home);
    }
}

// Add an environment variable
void add_environment_variable(const char *name, const char *value) {
    if (environ_count >= environ_size) {
        environ_size *= 2;
        environ_vars = (char **)realloc(environ_vars, environ_size * sizeof(char *));
    }
    
    char *var = (char *)malloc(strlen(name) + strlen(value) + 2); // +2 for '=' and null terminator
    sprintf(var, "%s=%s", name, value);
    environ_vars[environ_count++] = var;
}

// Get the value of an environment variable
char *get_environment_variable(const char *name) {
    for (int i = 0; i < environ_count; i++) {
        char *var = environ_vars[i];
        char *equals = strchr(var, '=');
        
        if (equals) {
            int name_len = equals - var;
            if (strncmp(var, name, name_len) == 0 && name_len == strlen(name)) {
                return equals + 1;
            }
        }
    }
    return NULL;
}

// Set the value of an environment variable
void set_environment_variable(const char *name, const char *value) {
    // Check if it already exists
    for (int i = 0; i < environ_count; i++) {
        char *var = environ_vars[i];
        char *equals = strchr(var, '=');
        
        if (equals) {
            int name_len = equals - var;
            if (strncmp(var, name, name_len) == 0 && name_len == strlen(name)) {
                free(environ_vars[i]);
                char *new_var = (char *)malloc(strlen(name) + strlen(value) + 2);
                sprintf(new_var, "%s=%s", name, value);
                environ_vars[i] = new_var;
                return;
            }
        }
    }
    
    // If not found, add it
    add_environment_variable(name, value);
}

// Tokenize the input string
char **tokenize(char *line, const char *delim) {
    char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
    char *token = strtok(line, delim);
    int i = 0;
    
    while (token != NULL && i < MAX_NUM_TOKENS - 1) {
        tokens[i] = strdup(token);
        i++;
        token = strtok(NULL, delim);
    }
    tokens[i] = NULL;
    return tokens;
}

// Free memory allocated for tokens
void free_tokens(char **tokens) {
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

// Find executable in PATH
char *find_executable(const char *cmd) {
    // If cmd contains '/', it's a path
    if (strchr(cmd, '/') != NULL) {
        if (access(cmd, X_OK) == 0) {
            return strdup(cmd);
        }
        return NULL;
    }
    
    // Search in PATH
    char *path = get_environment_variable("PATH");
    if (!path) return NULL;
    
    char *path_copy = strdup(path);
    char *dir = strtok(path_copy, ":");
    
    while (dir != NULL) {
        char full_path[PATH_MAX];
        snprintf(full_path, PATH_MAX, "%s/%s", dir, cmd);
        
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return strdup(full_path);
        }
        
        dir = strtok(NULL, ":");
    }
    
    free(path_copy);
    return NULL;
}

// Execute built-in commands
int execute_builtin(char **tokens) {
    if (!tokens || !tokens[0]) return 0;
    
    if (strcmp(tokens[0], "cd") == 0) {
        if (tokens[1] == NULL) {
            // cd with no args - go to home directory
            char *home = get_environment_variable("HOME");
            if (home) {
                chdir(home);
            } else {
                fprintf(stderr, "cd: HOME not set\n");
            }
        } else {
            if (chdir(tokens[1]) != 0) {
                perror("cd failed");
            }
        }
        return 1;
    } else if (strcmp(tokens[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd failed");
        }
        return 1;
    } else if (strcmp(tokens[0], "clear") == 0) {
        printf("\033[H\033[J");  // ANSI escape sequence to clear screen
        return 1;
    } else if (strcmp(tokens[0], "exit") == 0) {
        exit(tokens[1] ? atoi(tokens[1]) : 0);
    } else if (strcmp(tokens[0], "export") == 0) {
        if (tokens[1]) {
            char *equals = strchr(tokens[1], '=');
            if (equals) {
                *equals = '\0';
                set_environment_variable(tokens[1], equals + 1);
                *equals = '=';  // Restore the string
            }
        }
        return 1;
    } else if (strcmp(tokens[0], "echo") == 0) {
        for (int i = 1; tokens[i] != NULL; i++) {
            // Handle variable expansion $VAR
            if (tokens[i][0] == '$') {
                char *var_value = get_environment_variable(tokens[i] + 1);
                if (var_value) {
                    printf("%s", var_value);
                }
            } else {
                printf("%s", tokens[i]);
            }
            
            if (tokens[i + 1] != NULL) {
                printf(" ");
            }
        }
        printf("\n");
        return 1;
    }
    
    return 0;  // Not a built-in command
}

// Handle I/O redirection
int handle_redirection(char **tokens) {
    int i = 0;
    int in_fd = -1, out_fd = -1;
    int original_stdin = dup(STDIN_FILENO);
    int original_stdout = dup(STDOUT_FILENO);
    int has_redirection = 0;
    
    // Find redirection operators
    while (tokens[i] != NULL) {
        if (strcmp(tokens[i], "<") == 0) {
            // Input redirection
            if (tokens[i+1] == NULL) {
                fprintf(stderr, "Syntax error: Expected filename after <\n");
                return -1;
            }
            
            in_fd = open(tokens[i+1], O_RDONLY);
            if (in_fd < 0) {
                perror("Failed to open input file");
                return -1;
            }
            
            // Redirect stdin
            if (dup2(in_fd, STDIN_FILENO) < 0) {
                perror("dup2 failed");
                close(in_fd);
                return -1;
            }
            
            // Remove redirection tokens
            free(tokens[i]);
            free(tokens[i+1]);
            tokens[i] = NULL;
            
            // Shift remaining tokens
            int j = i;
            while (tokens[j+2] != NULL) {
                tokens[j] = tokens[j+2];
                j++;
            }
            tokens[j] = NULL;
            has_redirection = 1;
            continue; // Don't increment i since we've shifted tokens
            
        } else if (strcmp(tokens[i], ">") == 0 || strcmp(tokens[i], ">>") == 0) {
            // Output redirection
            if (tokens[i+1] == NULL) {
                fprintf(stderr, "Syntax error: Expected filename after %s\n", tokens[i]);
                return -1;
            }
            
            int flags = O_WRONLY | O_CREAT;
            if (strcmp(tokens[i], ">") == 0) {
                flags |= O_TRUNC;
            } else {
                flags |= O_APPEND;
            }
            
            out_fd = open(tokens[i+1], flags, 0644);
            if (out_fd < 0) {
                perror("Failed to open output file");
                return -1;
            }
            
            // Redirect stdout
            if (dup2(out_fd, STDOUT_FILENO) < 0) {
                perror("dup2 failed");
                close(out_fd);
                return -1;
            }
            
            // Remove redirection tokens
            free(tokens[i]);
            free(tokens[i+1]);
            tokens[i] = NULL;
            
            // Shift remaining tokens
            int j = i;
            while (tokens[j+2] != NULL) {
                tokens[j] = tokens[j+2];
                j++;
            }
            tokens[j] = NULL;
            has_redirection = 1;
            continue; // Don't increment i
        }
        
        i++;
    }
    
    return has_redirection;
}

// Execute a command with arguments
int execute_command(char **tokens) {
    if (!tokens || !tokens[0]) return 0;
    
    // Check if it's a built-in command
    if (execute_builtin(tokens)) {
        return 0;  // Success exit code
    }
    
    // Handle redirection before forking
    int has_redirection = handle_redirection(tokens);
    if (has_redirection < 0) {
        return 1; // Error in redirection
    }
    
    // External command - create a child process
    pid_t pid = fork();
    
    if (pid == 0) {  // Child process
        // Execute the command
        char *cmd_path = find_executable(tokens[0]);
        if (cmd_path) {
            execv(cmd_path, tokens);
            free(cmd_path);
        } else {
            fprintf(stderr, "Command not found: %s\n", tokens[0]);
        }
        exit(127);  // Command not found
    } else if (pid < 0) {  // Error in fork
        perror("Fork failed");
        return 1;
    } else {  // Parent process
        // Wait for the child to terminate
        int status;
        waitpid(pid, &status, 0);
        
        // Restore redirected file descriptors
        if (has_redirection) {
            dup2(STDIN_FILENO, 0);
            dup2(STDOUT_FILENO, 1);
        }
        
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
}

// Parse and execute commands separated by semicolons
void execute_commands(char *input) {
    char *input_copy = strdup(input);
    char **commands = tokenize(input_copy, ";");
    
    for (int i = 0; commands[i] != NULL; i++) {
        char *cmd = commands[i];
        while (*cmd == ' ' || *cmd == '\t') cmd++; // Skip leading whitespace
        
        if (*cmd != '\0') {  // Skip empty commands
            execute_logical_commands(cmd);
        }
    }
    
    free_tokens(commands);
    free(input_copy);
}

// Handle logical operators && and ||
int execute_logical_commands(char *input) {
    char *input_copy = strdup(input);
    char *cmd = input_copy;
    int exit_status = 0;
    
    // Look for && or ||
    char *and_op = strstr(cmd, "&&");
    char *or_op = strstr(cmd, "||");
    
    if (!and_op && !or_op) {
        // No logical operators, check for pipes
        exit_status = execute_pipe_commands(cmd);
        free(input_copy);
        return exit_status;
    }
    
    // Determine which operator comes first
    char *op_ptr;
    int is_and;
    
    if (and_op && (!or_op || and_op < or_op)) {
        op_ptr = and_op;
        is_and = 1;
    } else {
        op_ptr = or_op;
        is_and = 0;
    }
    
    // Split at the operator
    *op_ptr = '\0';
    char *first_cmd = cmd;
    char *second_cmd = op_ptr + 2;  // Skip && or ||
    
    // Execute first command
    char *first_copy = strdup(first_cmd);
    exit_status = execute_pipe_commands(first_copy);
    free(first_copy);
    
    // Execute second command based on result and operator
    if ((is_and && exit_status == 0) || (!is_and && exit_status != 0)) {
        exit_status = execute_logical_commands(second_cmd);
    }
    
    free(input_copy);
    return exit_status;
}

// Handle pipe operator
int execute_pipe_commands(char *input) {
    char *pipe_pos = strstr(input, "|");
    
    if (!pipe_pos) {
        // No pipe, execute as simple command
        char *input_copy = strdup(input);
        char **tokens = tokenize(input_copy, " \t\n");
        
        if (tokens[0]) {
            int status = execute_command(tokens);
            free_tokens(tokens);
            free(input_copy);
            return status;
        }
        
        free_tokens(tokens);
        free(input_copy);
        return 0;
    }
    
    // Split the command at the pipe
    *pipe_pos = '\0';
    char *cmd1 = input;
    char *cmd2 = pipe_pos + 1;
    
    // Remove leading whitespace
    while (*cmd1 == ' ' || *cmd1 == '\t') cmd1++;
    while (*cmd2 == ' ' || *cmd2 == '\t') cmd2++;
    
    // Create pipe
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return 1;
    }
    
    // First command
    pid_t pid1 = fork();
    if (pid1 == 0) {  // Child for cmd1
        // Redirect stdout to pipe write end
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        // Execute the command
        char *cmd1_copy = strdup(cmd1);
        char **tokens = tokenize(cmd1_copy, " \t\n");
        if (tokens[0]) {
            execute_command(tokens);
        }
        free_tokens(tokens);
        free(cmd1_copy);
        exit(0);
    } else if (pid1 < 0) {
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }
    
    // Second command
    pid_t pid2 = fork();
    if (pid2 == 0) {  // Child for cmd2
        // Redirect stdin to pipe read end
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        
        // Check if cmd2 contains another pipe
        if (strstr(cmd2, "|")) {
            int result = execute_pipe_commands(cmd2);
            exit(result);
        } else {
            // Execute as simple command
            char *cmd2_copy = strdup(cmd2);
            char **tokens = tokenize(cmd2_copy, " \t\n");
            if (tokens[0]) {
                int result = execute_command(tokens);
                free_tokens(tokens);
                free(cmd2_copy);
                exit(result);
            }
            free_tokens(tokens);
            free(cmd2_copy);
            exit(0);
        }
    } else if (pid2 < 0) {
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid1, NULL, 0);
        return 1;
    }
    
    // Parent process
    close(pipefd[0]);
    close(pipefd[1]);
    
    int status1, status2;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);
    
    return WIFEXITED(status2) ? WEXITSTATUS(status2) : 1;
}

// Main shell function
int main(int argc, char *argv[]) {
    char input[MAX_INPUT_SIZE];
    FILE *input_stream;
    
    // Initialize environment variables
    initialize_environment();
    
    // Initialize readline for command history and completion
    rl_attempted_completion_function = NULL;  // Use default completion
    using_history();
    
    // Determine if we're in batch or interactive mode
    if (argc > 1) {
        // Batch mode - read from file
        input_stream = fopen(argv[1], "r");
        if (input_stream == NULL) {
            fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
            exit(1);
        }
    } else {
        // Interactive mode - read from stdin
        input_stream = stdin;
    }
    
    while (1) {
        if (input_stream == stdin) {
            // Interactive mode - use readline
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                char prompt[PATH_MAX + 10];
                sprintf(prompt, "mysh:%s$ ", cwd);
                
                char *line = readline(prompt);
                if (!line) break;  // EOF
                
                // Add to history if non-empty
                if (*line) {
                    add_history(line);
                    strcpy(input, line);
                    free(line);
                } else {
                    free(line);
                    continue;
                }
            } else {
                char *line = readline("mysh$ ");
                if (!line) break;  // EOF
                
                if (*line) {
                    add_history(line);
                    strcpy(input, line);
                    free(line);
                } else {
                    free(line);
                    continue;
                }
            }
        } else {
            // Batch mode - read from file
            if (fgets(input, MAX_INPUT_SIZE, input_stream) == NULL) {
                break;  // EOF
            }
            
            // Remove trailing newline
            size_t len = strlen(input);
            if (len > 0 && input[len-1] == '\n') {
                input[len-1] = '\0';
            }
        }
        
        // Execute command(s)
        execute_commands(input);
    }
    
    // Cleanup
    if (input_stream != stdin) {
        fclose(input_stream);
    }
    
    // Free environment variables
    for (int i = 0; i < environ_count; i++) {
        free(environ_vars[i]);
    }
    free(environ_vars);
    
    // Clear readline history
    clear_history();
    
    return 0;
}
