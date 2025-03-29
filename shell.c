#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>
#include <time.h>

/* ANSI color definitions for terminal output */
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"

#define MAX_LINE 80 /* Maximum characters per command line */
char history[20][MAX_LINE]; /* Array to store command history */
int historyCount = 0;  /* Counter for command history entries */

/**
 * implementCd - Changes current working directory
 * @param path: Directory path to change to
 */
void implementCd(char* path) {
    if (chdir(path) != 0) {
        perror("Error changing directory");
    }
}

/**
 * setup - Reads and parses the command line input
 * @param inputBuffer: Buffer to store raw input
 * @param args: Array to store tokenized command arguments
 * @param background: Flag to indicate if command runs in background
 *
 * Reads user input and tokenizes it into arguments, handles ampersand
 * for background processes, and terminates args with NULL.
 */
void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # of characters in the command line */
        i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */
    if (length < 0){
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

    /* examine every character in the inputBuffer */
    for (i=0;i<length;i++) {
        switch (inputBuffer[i]){
            case ' ':
            case '\t' :               /* argument separators */
                if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = ' '; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */
                if (start != -1){
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default :             /* some other character */
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&'){
                    *background  = 1;
                    inputBuffer[i] = '\0';
                }
        }
     }
     args[ct] = NULL; /* just in case the input line was > 80 */
}

/**
 * readPath - Gets the PATH environment variable
 * @return: Pointer to PATH string or NULL on error
 */
char* readPath() {
    char* temp = getenv("PATH");
    if (!temp) {
        perror("Error reading PATH");
        return NULL;
    }
    return temp;
}

/**
 * tokenizePath - Splits PATH into individual directory components
 * @param path: The PATH string to tokenize
 * @return: Array of strings with individual PATH directories
 *
 * Breaks down the PATH environment variable into separate directory paths
 * for command search functionality.
 */
char** tokenizePath(const char* path) {
    if (path == NULL) return NULL;

    // Duplicate path to avoid modifying the original string
    char* pathCopy = strdup(path);
    if (!pathCopy) {
        perror("Memory allocation failed");
        return NULL;
    }

    // First pass: count tokens
    int count = 0;
    char* token = strtok(pathCopy, ":");
    while (token != NULL) {
        count++;
        token = strtok(NULL, ":");
    }

    // Allocate memory for token array (+1 for NULL terminator)
    char** paths = malloc((count + 1) * sizeof(char *));
    if (!paths) {
        perror("Memory allocation failed");
        free(pathCopy);
        return NULL;
    }

    // Second pass: tokenize again
    strcpy(pathCopy, path); // Restore the original pathCopy
    token = strtok(pathCopy, ":");
    int index = 0;
    while (token != NULL) {
        paths[index] = strdup(token); // Store a new copy of each token
        if (!paths[index]) {
            perror("Memory allocation failed");
            // Free previously allocated strings
            for (int i = 0; i < index; i++) free(paths[i]);
            free(paths);
            free(pathCopy);
            return NULL;
        }
        index++;
        token = strtok(NULL, ":");
    }
    paths[index] = NULL; // Null-terminate the array

    free(pathCopy); // Free the duplicated string (tokens are now separate copies)
    return paths;
}

/**
 * getCommands - Lists all executable files in PATH directories
 * @param paths: Array of PATH directory strings
 * @return: Array of executable command names found in PATH
 *
 * Scans all directories in PATH to build a list of available commands
 * for auto-correction feature.
 */
char** getCommands(char** paths) {
    int capacity = 200;
    char **commands = malloc(capacity * sizeof(char *));
    for (int i = 0; i < 10; i++) {
        struct dirent *entry;
        DIR *dir = opendir(paths[i]);

        if (dir == NULL) {
            continue;
        }

        int count = 0;

        if (!commands) {
            perror("Memory allocation failed");
            closedir(dir);
            return NULL;
        }

        while ((entry = readdir(dir)) != NULL) {
            if (count >= capacity) {
                capacity *= 2;
                char **temp = realloc(commands, capacity * sizeof(char *));
                if (!temp) {
                    perror("Memory reallocation failed");
                    break;
                }
                commands = temp;
            }
            commands[count] = strdup(entry->d_name);
            if (!commands[count]) {
                perror("Memory allocation for command name failed");
                break;
            }
            count++;
        }
        closedir(dir);
    }
    return commands;
}

/**
 * hammingDistance - Calculates Hamming distance between two strings
 * @param first: First string to compare
 * @param second: Second string to compare
 * @return: Hamming distance or -1 if strings have different lengths
 *
 * Counts the number of positions where characters differ between strings.
 * Used for command spelling correction.
 */
int hammingDistance(char* first, char* second) {
    if (strlen(first) != strlen(second)) return -1;
    int count = 0;
    for (int i = 0; first[i] != '\0'; i++) {
        if (first[i] != second[i]) count++;
    }
    return count;
}

/**
 * min - Returns the minimum of three integers
 * @param a: First integer
 * @param b: Second integer
 * @param c: Third integer
 * @return: Minimum value among the three
 */
int min(const int a, const int b, const int c) {
    return a < b ? (a < c ? a : c) : (b < c ? b : c);
}

/**
 * levenshteinDistance - Calculates edit distance between two strings
 * @param s1: First string
 * @param s2: Second string
 * @return: Levenshtein distance between strings
 *
 * Implements the Levenshtein algorithm to find minimum edit distance.
 * Used for more sophisticated command spelling correction.
 */
int levenshteinDistance(char* s1, char* s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);

    // Allocate memory for distance matrix
    int **dp = (int **)malloc((len1 + 1) * sizeof(int *));
    for (int i = 0; i <= len1; i++) {
        dp[i] = (int *)malloc((len2 + 1) * sizeof(int));
    }

    // Initialize base cases
    for (int i = 0; i <= len1; i++) dp[i][0] = i;
    for (int j = 0; j <= len2; j++) dp[0][j] = j;

    // Fill DP table
    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            const int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            dp[i][j] = min(
                dp[i - 1][j] + 1,       // Deletion
                dp[i][j - 1] + 1,       // Insertion
                dp[i - 1][j - 1] + cost // Substitution
            );

            // Handle swaps (transpositions)
            if (i > 1 && j > 1 && s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1]) {
                dp[i][j] = dp[i - 2][j - 2] + 1; // Count swap as 1 operation
            }
        }
    }

    const int distance = dp[len1][len2];

    // Free allocated memory
    for (int i = 0; i <= len1; i++) free(dp[i]);
    free(dp);

    return distance;
}

/**
 * closestMatches - Finds commands similar to user input
 * @param commands: Array of available commands
 * @param command: User input command to compare against
 * @return: Array of closest matching commands
 *
 * Uses edit distance algorithms to find commands that are close matches
 * to what the user typed, for the "Do What I Mean" feature.
 */
char** closestMatches(char** commands, char* command) {
    char** result = malloc(10 * sizeof(char*));
    if (!result) return NULL;
    if (!commands) return NULL;

    for (int i = 0; i < 10; i++) result[i] = NULL;

    int closeMatches = 1;
    for (int i = 0; commands[i] != NULL; i++) {
        const int temp = hammingDistance(command, commands[i]);
        const int temp2 = levenshteinDistance(command, commands[i]);
        if (temp == 0) {
            result[0] = command;
            return result;
        }
        if (temp == 1) {
            result[closeMatches++] = commands[i];
            if (closeMatches >= 10) break;
            continue;
        }
        if (temp2 == 1) {
            result[closeMatches++] = commands[i];
            if (closeMatches >= 10) break;
        }
    }
    return result;
}

/**
 * checkIfRight - Prompts user to select from similar commands
 * @param commands: Array of command suggestions
 * @param rest: Rest of the command line after the first token
 * @return: Selected command or NULL if no selection
 *
 * Interactive part of the DWIM feature - asks user if they meant
 * a similar command when typo is detected.
 */
char* checkIfRight(char** commands, char* rest) {
    char response[10];
    for (int i = 1; commands[i] != NULL; i++) {

        printf(YELLOW "Did you mean \"%s%s\"? [y/n] " RESET, commands[i], rest);
        fflush(stdout);

        if (scanf("%9s", response) != 1) {
            while (getchar() != '\n') {}
            // Clear input buffer
            continue;
        }

        // If user confirms, return the command
        if (strcmp(response, "y") == 0 || strcmp(response, "yes") == 0) {
            return commands[i];
        }
    }
    return NULL;
}

/**
 * run - Executes a command with arguments
 * @param tempBuffer: Command line with arguments
 *
 * Parses the command line and calls execvp to run the command.
 * Child process function that replaces itself with the command.
 */
void run(char tempBuffer[80]) {
    char *args[MAX_LINE / 2 + 1];  // At most 40 arguments + NULL
    int i = 0;

    // Tokenize inputBuffer
    char *token = strtok(tempBuffer, " ");
    while (token != NULL) {
        args[i++] = token;  // Store pointer to token
        token = strtok(NULL, " ");
    }
    args[i] = NULL;  // `execvp` needs NULL at the end

    execvp(args[0], args);
}

/**
 * checkIntegratedCommands - Checks and runs built-in shell commands
 * @param command: Command line to check
 * @return: 0 if built-in command executed, -1 otherwise
 *
 * Handles special built-in commands like cd, exit, clear, echo,
 * history, and the custom tepisan command.
 */
int checkIntegratedCommands(char* command) {
    char commandCopy[MAX_LINE];
    strcpy(commandCopy, command);
    char* firstCommand = strtok(commandCopy, " ");

    if (strcmp(firstCommand, "cd") == 0) {
        char* path = strtok(NULL, " ");  // Get the directory argument
        if (path != NULL) {
            implementCd(path);  // Call your cd implementation
            return 0;
        }
        printf(RED "cd: missing argument\n" RESET);
        return 0;
    }
    if (strcmp(firstCommand, "exit") == 0) {
        exit(0);
    }
    if (strcmp(firstCommand, "clear") == 0) {
        system("clear");
        return 0;
    }
    if (strcmp(firstCommand, "echo") == 0) {
        char* message = strtok(NULL, "\0");

        if (message != NULL) {
            int j = 0;
            for (int i = 0; message[i] != '\0'; i++) {
                if (message[i] != '\"') {
                    message[j++] = message[i];
                }
            }
            message[j] = '\0';
            printf("%s\n", message);
            return 0;
        }
        printf("\n");
        return 0;
    }
    if (strcmp(firstCommand, "history") == 0) {
        for (int i = 0; i < historyCount; i++) {
            printf("%d: %s\n", i + 1, history[i]);
        }
        return 0;
    }
    if (strcmp(firstCommand, "tepisan") == 0) {
        struct timespec ts;
        ts.tv_sec = 0;          // 0 seconds
        ts.tv_nsec = 5000000;  // 5 milliseconds
        char* who = strtok(NULL, " ");
        char* times = strtok(NULL, " ");
        int index = atoi(times);
        for (int i = 0; i < index; i++) {
            //Cycle Colors
            switch (i%6) {
                case 0:
                    printf(RED "tepisan%s\n" RESET, who);
                nanosleep(&ts, NULL);
                break;
                case 1:
                    printf(YELLOW "tepisan%s\n" RESET, who);
                nanosleep(&ts, NULL);
                break;
                case 2:
                    printf(GREEN "tepisan%s\n" RESET, who);
                nanosleep(&ts, NULL);
                break;
                case 3:
                    printf(BLUE "tepisan%s\n" RESET, who);
                nanosleep(&ts, NULL);
                break;
                case 4:
                    printf(MAGENTA "tepisan%s\n" RESET, who);
                nanosleep(&ts, NULL);
                break;
                case 5:
                    printf(CYAN "tepisan%s\n" RESET, who);
                nanosleep(&ts, NULL);
                break;
            }
        }
        return 0;
    }
    return -1;
}

/**
 * saveHistory - Adds command to history buffer
 * @param inputBuffer: Command to save
 *
 * Maintains a circular buffer of the last 20 commands.
 * Avoids storing duplicate consecutive commands.
 */
void saveHistory(char* inputBuffer) {
    if (strlen(inputBuffer) > 0 && (historyCount == 0 || strcmp(history[historyCount - 1], inputBuffer) != 0)) {
        if (historyCount < 20) {
            strcpy(history[historyCount], inputBuffer);
            historyCount++;
        } else {
            // Shift history when full
            for (int i = 1; i < 20; i++) {
                strcpy(history[i - 1], history[i]);
            }
            strcpy(history[20 - 1], inputBuffer);
        }
    }
}

/**
 * main - Main shell function
 *
 * Initializes shell environment, displays prompt, reads and processes
 * commands in an infinite loop until exit command is received.
 * Implements the "Do What I Mean" (DWIM) feature for typo correction.
 */
int main(void)
{
    char cwd[1024], inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
    int background;             /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE/+1];/* command line (of 80) has max of 40 arguments */
    pid_t parent_pid = getpid();
    char* path = readPath();
    char** paths = tokenizePath(path);
    char** commands = getCommands(paths);
    char** matches = NULL;

    printf(YELLOW "Bienvenido a DWIM Shell --- Escrito por Kevin Banegas\n" RESET);

    while (1){            /* Program terminates normally inside setup */
        background = 0;
        getcwd(cwd, sizeof(cwd));
        printf(GREEN "dwimsh" RESET ":" BLUE "%s" RESET "$ ", cwd);
        fflush(stdout);

        setup(inputBuffer,args,&background);       /* get next command */
        saveHistory(inputBuffer);
        int executed = checkIntegratedCommands(inputBuffer);

        if (executed == 0) continue;

        fork();
        if (getpid() != parent_pid) {
            char inputCopy[MAX_LINE];
            strcpy(inputCopy, inputBuffer);

            char* firstCommand = strtok(inputCopy, " ");
            matches = closestMatches(commands, firstCommand);

            if (matches[0] != NULL) {
                run(inputBuffer);
            }
            if (matches[1] == NULL) {
                printf(RED "Command not found: %s \n" RESET, inputCopy);
            }
            else {
                printf(RED "Command not found: %s \n" RESET, inputBuffer);
                char* restOfCommand = inputBuffer + strlen(firstCommand);
                char* right = checkIfRight(matches, restOfCommand);

                char tempBuffer[MAX_LINE];

                snprintf(tempBuffer, MAX_LINE, "%s%s", right, restOfCommand);

                run(tempBuffer);
            }

            //free matches
            for (int i = 0; i < 10; i++) {
                if (matches[i] != NULL) free(matches[i]);
            }
            exit(0);
        }
        wait(NULL);
    }
}