#include <stdio.h>    // Standard input/output functions (e.g., printf, fopen, fclose, fgets, fprintf, perror)
#include <stdlib.h>   // Standard library functions (e.g., atoi for string to integer conversion, remove, rename, getenv)
#include <string.h>   // String manipulation functions (e.g., strcmp for string comparison, strcat for string concatenation, snprintf)
#include <stdbool.h>  // Boolean type (bool, true, false)
#include <sys/stat.h> // For mkdir
#include <errno.h>    // For errno

// Define ANSI color codes for terminal output
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m" // Resets all attributes
#define ANSI_BOLD          "\x1b[1m" // Bold text

// Define a maximum length for task descriptions
#define MAX_DESCRIPTION_LEN 256
// Define the directory and filename components
#define TASK_DIR_SUFFIX "/.local/taskmanager"
#define TASK_FILENAME "tasks.txt"
// Define a maximum path length (e.g., for full path to tasks.txt)
#define MAX_PATH_LEN 512

// Global buffer for the full task file path
// This will store the path like "/home/youruser/.local/taskmanager/tasks.txt"
char full_task_file_path[MAX_PATH_LEN];

// Structure to represent a single task
typedef struct {
    int id;
    char description[MAX_DESCRIPTION_LEN];
    bool completed; // true if completed, false if pending
} Task;

// Function to ensure the ~/.local/taskmanager directory exists
void ensure_task_directory_exists() {
    char task_dir[MAX_PATH_LEN];
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set. Cannot create task directory.\n");
        exit(EXIT_FAILURE); // Exit if home directory cannot be found
    }

    // Construct the full path to the task directory
    snprintf(task_dir, sizeof(task_dir), "%s%s", home_dir, TASK_DIR_SUFFIX);

    // Check if the directory exists
    struct stat st = {0};
    if (stat(task_dir, &st) == -1) {
        // Directory does not exist, try to create it
        // 0700 gives read, write, execute permissions to the owner only
        if (mkdir(task_dir, 0700) == -1) {
            // Check if error is due to directory already existing (e.g., race condition)
            if (errno != EEXIST) {
                perror("Error creating task directory");
                exit(EXIT_FAILURE); // Exit if directory cannot be created for other reasons
            }
        }
    }
}

// Function to get the next available task ID
// Reads the file to find the highest existing ID and returns highest + 1
int getNextTaskId() {
    // Use the global full_task_file_path
    FILE *file = fopen(full_task_file_path, "r"); // Open the task file in read mode
    if (file == NULL) {
        // If file doesn't exist, it's fine, we'll create it when adding the first task.
        // errno will be ENOENT (No such file or directory) which is expected.
        return 1; // Start with ID 1 if file doesn't exist
    }

    int maxId = 0;
    char line[MAX_DESCRIPTION_LEN + 20]; // Buffer for a whole line (ID,STATUS,DESCRIPTION)
    while (fgets(line, sizeof(line), file) != NULL) { // Read file line by line
        int id;
        // Parse the ID from the beginning of the line
        if (sscanf(line, "%d,", &id) == 1) {
            if (id > maxId) {
                maxId = id; // Keep track of the highest ID found
            }
        }
    }
    fclose(file); // Close the file
    return maxId + 1; // Return the next available ID
}

// Function to add a new task
void addTask(const char *description) {
    // Use the global full_task_file_path
    FILE *file = fopen(full_task_file_path, "a"); // Open in append mode (creates file if it doesn't exist)
    if (file == NULL) {
        perror("Error opening task file for writing"); // Print system error message
        return;
    }

    int id = getNextTaskId(); // Get a new unique ID
    // Write task in format: ID,STATUS,DESCRIPTION\n
    // STATUS: 0 for pending, 1 for completed
    fprintf(file, "%d,%d,%s\n", id, 0, description); // Write the new task (initially pending)
    fclose(file); // Close the file
    printf("Task added: ID %d - \"%s\"\n", id, description); // Confirm to user
}

// Function to list all tasks
void listTasks() {
    // Use the global full_task_file_path
    FILE *file = fopen(full_task_file_path, "r"); // Open in read mode
    if (file == NULL) {
        printf("No tasks found. Create one using 'add' command.\n"); // Inform if file doesn't exist
        return;
    }

    printf("\n%s%s------------------------------------------------------%s\n", ANSI_BOLD, ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
    char line[MAX_DESCRIPTION_LEN + 20]; // Buffer for reading lines
    int count = 0;
    while (fgets(line, sizeof(line), file) != NULL) { // Read file line by line
        int id, status;
        char description[MAX_DESCRIPTION_LEN];
        // Parse the line: ID,STATUS,DESCRIPTION
        if (sscanf(line, "%d,%d,%[^\n]", &id, &status, description) == 3) {
            // Print task details formatted with colors
            const char* status_text = (status == 1 ? "[DONE]" : "[PENDING]");
            const char* status_color = (status == 1 ? ANSI_COLOR_GREEN : ANSI_COLOR_YELLOW);

            printf("%sID: %-4d%s Status: %s%-10s%s Description: %s%s\n",
                   ANSI_COLOR_CYAN, id, ANSI_COLOR_RESET, // ID in Cyan
                   status_color, status_text, ANSI_COLOR_RESET, // Status in Green/Yellow
                   description, ANSI_COLOR_RESET); // Description (default color)
            count++;
        }
    }
    fclose(file); // Close the file
    if (count == 0) {
        printf("No tasks found.\n"); // Handle case where file exists but is empty
    }
    printf("%s------------------------------------------------------%s\n\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
}

// Function to modify a task's status (mark as done)
void modifyTaskStatus(int taskId, bool complete) {
    // Use the global full_task_file_path
    FILE *originalFile = fopen(full_task_file_path, "r"); // Open original file for reading
    if (originalFile == NULL) {
        printf("No tasks found.\n");
        return;
    }

    // Create a temporary file in the same directory as tasks.txt
    char temp_file_path[MAX_PATH_LEN];
    // Construct the path for the temporary file
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set. Cannot create temporary file.\n");
        fclose(originalFile);
        return;
    }
    snprintf(temp_file_path, sizeof(temp_file_path), "%s%s/%s", home_dir, TASK_DIR_SUFFIX, "temp_tasks.txt");


    FILE *tempFile = fopen(temp_file_path, "w"); // Open temporary file for writing
    if (tempFile == NULL) {
        perror("Error creating temporary file");
        fclose(originalFile);
        return;
    }

    bool taskFound = false;
    char line[MAX_DESCRIPTION_LEN + 20];
    while (fgets(line, sizeof(line), originalFile) != NULL) {
        int id, status;
        char description[MAX_DESCRIPTION_LEN];
        if (sscanf(line, "%d,%d,%[^\n]", &id, &status, description) == 3) {
            if (id == taskId) {
                // Found the task, update its status
                fprintf(tempFile, "%d,%d,%s\n", id, complete ? 1 : 0, description);
                taskFound = true;
            } else {
                // Copy other tasks as they are
                fprintf(tempFile, "%s", line);
            }
        } else {
            // Copy malformed lines as they are (to preserve file integrity)
            fprintf(tempFile, "%s", line);
        }
    }

    fclose(originalFile); // Close both files
    fclose(tempFile);

    // Replace the original file with the temporary file
    remove(full_task_file_path); // Delete the original file
    rename(temp_file_path, full_task_file_path); // Rename temp file to original filename

    if (taskFound) {
        printf("Task ID %d marked as %s.\n", taskId, complete ? "DONE" : "PENDING");
    } else {
        printf("Task ID %d not found.\n", taskId);
    }
}

// Function to delete a task
void deleteTask(int taskId) {
    // Use the global full_task_file_path
    FILE *originalFile = fopen(full_task_file_path, "r"); // Open original file for reading
    if (originalFile == NULL) {
        printf("No tasks found.\n");
        return;
    }

    // Create a temporary file in the same directory as tasks.txt
    char temp_file_path[MAX_PATH_LEN];
    // Construct the path for the temporary file
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set. Cannot create temporary file.\n");
        fclose(originalFile);
        return;
    }
    snprintf(temp_file_path, sizeof(temp_file_path), "%s%s/%s", home_dir, TASK_DIR_SUFFIX, "temp_tasks.txt");

    FILE *tempFile = fopen(temp_file_path, "w"); // Open temporary file for writing
    if (tempFile == NULL) {
        perror("Error creating temporary file");
        fclose(originalFile);
        return;
    }

    bool taskFound = false;
    char line[MAX_DESCRIPTION_LEN + 20];
    while (fgets(line, sizeof(line), originalFile) != NULL) {
        int id;
        // Peek at the ID to decide if we should copy the line
        if (sscanf(line, "%d,", &id) == 1) { // Parse ID from line
            if (id == taskId) {
                taskFound = true; // Found the task to delete, so DON'T write this line to tempFile
            } else {
                fprintf(tempFile, "%s", line); // Copy other lines to tempFile
            }
        } else {
            // Copy malformed lines (or handle error)
            fprintf(tempFile, "%s", line);
        }
    }

    fclose(originalFile); // Close both files
    fclose(tempFile);

    remove(full_task_file_path); // Delete the original file
    rename(temp_file_path, full_task_file_path); // Rename temp file to original filename

    if (taskFound) {
        printf("Task ID %d deleted.\n", taskId);
    } else {
        printf("Task ID %d not found.\n", taskId);
    }
}


// Main function to handle command-line arguments
int main(int argc, char *argv[]) {
    // --- IMPORTANT: Initialize the full task file path and ensure directory exists ---
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set. Cannot determine task file path.\n");
        return 1;
    }
    // Construct the full path to tasks.txt
    snprintf(full_task_file_path, sizeof(full_task_file_path), "%s%s/%s", home_dir, TASK_DIR_SUFFIX, TASK_FILENAME);

    // Ensure the directory ~/.local/taskmanager exists
    ensure_task_directory_exists();
    // --- END IMPORTANT INITIALIZATION ---

    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s add <description>\n", argv[0]);
        printf("  %s list\n", argv[0]);
        printf("  %s done <task_id>\n", argv[0]);
        printf("  %s pending <task_id>\n", argv[0]);
        printf("  %s delete <task_id>\n", argv[0]);
        return 1;
    }

    // Check the command argument
    if (strcmp(argv[1], "add") == 0) {
        if (argc < 3) {
            printf("Usage: %s add <description>\n", argv[0]);
            return 1;
        }
        // Combine all subsequent arguments into a single description string
        char description[MAX_DESCRIPTION_LEN] = "";
        for (int i = 2; i < argc; i++) {
            strcat(description, argv[i]);
            if (i < argc - 1) {
                strcat(description, " ");
            }
        }
        addTask(description);
    } else if (strcmp(argv[1], "list") == 0) {
        listTasks();
    } else if (strcmp(argv[1], "done") == 0) {
        if (argc < 3) {
            printf("Usage: %s done <task_id>\n", argv[0]);
            return 1;
        }
        int taskId = atoi(argv[2]);
        if (taskId <= 0) {
            printf("Invalid task ID. Please provide a positive integer.\n");
            return 1;
        }
        modifyTaskStatus(taskId, true);
    } else if (strcmp(argv[1], "pending") == 0) {
        if (argc < 3) {
            printf("Usage: %s pending <task_id>\n", argv[0]);
            return 1;
        }
        int taskId = atoi(argv[2]);
        if (taskId <= 0) {
            printf("Invalid task ID. Please provide a positive integer.\n");
            return 1;
        }
        modifyTaskStatus(taskId, false);
    } else if (strcmp(argv[1], "delete") == 0) {
        if (argc < 3) {
            printf("Usage: %s delete <task_id>\n", argv[0]);
            return 1;
        }
        int taskId = atoi(argv[2]);
        if (taskId <= 0) {
            printf("Invalid task ID. Please provide a positive integer.\n");
            return 1;
        }
        deleteTask(taskId);
    } else {
        printf("Unknown command: %s\n", argv[1]);
        printf("Usage:\n");
        printf("  %s add <description>\n", argv[0]);
        printf("  %s list\n", argv[0]);
        printf("  %s done <task_id>\n", argv[0]);
        printf("  %s pending <task_id>\n", argv[0]);
        printf("  %s delete <task_id>\n", argv[0]);
        return 1;
    }

    return 0;
}

