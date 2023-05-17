// @file monitor.c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>     
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SERVER_PIPE_NAME "tmp/server_pipe" ///< Name of the server pipe
#define CLIENT_PIPE_NAME "tmp/client_pipe" ///< Name of the client pipe
#define BUFFER_SIZE 1024 ///< Size of every buffer
#define MAX_ENTRIES BUFFER_SIZE - 1 ///< Number of the maximum entry in the information array

char *output_dir; ///< Directory of the output it's read from argv[1]

/**
 *  Struct to save information about the requests
 */
struct Info {
    int pid; ///< Pid of the program
    char name[BUFFER_SIZE]; ///< Name of the program
    long time; ///< Time it took to run in case running equals 0 or start time in case running equals 1
    int running; ///< 1 if true 0 if false
};

struct Info *information[BUFFER_SIZE] = {0}; ///< Initialize an array from struct Info with null pointers


/**
 * This function creates a new entry in the iformation array
 * 
 * @param[in] pid
 * @param[in] name
 * @param[in] time
 * @paran[in] running
 */
void create_info(int pid, char name[], long time, int running) {
    struct Info *new_info = malloc(sizeof(struct Info));
    new_info->pid = pid;
    strcpy(new_info->name, name);
    new_info->time = time;
    new_info->running = running;
    
    // Find the first null pointer in the array
    int i = 0;
    while (i < MAX_ENTRIES && information[i] != NULL) {
        i++;
    }
    
    if (i < MAX_ENTRIES) {
        information[i] = new_info;
    } else {
        // Error: array is full
        free(new_info);
        new_info = NULL;
    }
}

/**
 * This function updates the information namely the time and the running status given a certain pid
 * @param[in] pid
 * @param[in] new_time
 * @param[in] new_running
 */
void update_info(int pid, long new_time, int new_running) {
    // Find the corresponding Info struct in the array
    int i = 0;
    while (i < MAX_ENTRIES && information[i] != NULL && information[i]->pid != pid) {
        i++;
    }
    
    if (information[i] != NULL) {
        // Update the Info struct
        information[i]->time = new_time;
        information[i]->running = new_running;
    }
}

/**
 * This is the readln function
 * @param[in] fd file from witch is the lines will be read
 * @param[in] line
 * @param[in] size
 * @param[out] total_bytes_read
 */
ssize_t readln(int fd, char *line, size_t size) {
    char buffer[BUFFER_SIZE];
    ssize_t total_bytes_read = 0;
    int bytes_read = 0;
    int i;

    while (total_bytes_read < size - 1) {
        bytes_read = read(fd, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) {
            break;
        }
        for (i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\n') {
                line[total_bytes_read] = '\0';
                return total_bytes_read;
            } else {
                line[total_bytes_read] = buffer[i];
                total_bytes_read++;
            }
        }
    }

    line[total_bytes_read] = '\0';
    return total_bytes_read;
}

/**
 * This function processes the request that as been given by the client
 * @param[in] request
 */
void process_request (char *request){

    char buffer[BUFFER_SIZE];
    int num_written;

    if(strncmp (request, "start", 5) == 0) {
        int pid;

        char program[BUFFER_SIZE];

        long start_time;

        char *token;
        token = strtok(request, " ");
        token = strtok(NULL, " ");
        pid = atoi(token);
        token = strtok(NULL, " ");
        strcpy(program, token);
        token = strtok(NULL, " ");
        start_time = atol(token);

        // Save start information to file
        char filename[BUFFER_SIZE];
        sprintf(filename, "%s/%d", output_dir, pid);

        int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

        if (file_fd == -1) {
            // Debug: opening failed
            perror("Error opening file");
            _exit(1);
        }

        num_written = snprintf(buffer, BUFFER_SIZE, "%s %ld\n", program, start_time);
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(file_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }

        close(file_fd);

        create_info(pid, program, start_time, 1);

    } else if (strncmp (request, "end", 3) == 0) {

        int pid;

        long end_time;

        char *token;
        token = strtok(request, " ");
        token = strtok(NULL, " ");
        pid = atoi(token);
        token = strtok(NULL, " ");
        end_time = atol(token);

        // Load start information from file
        char filename[BUFFER_SIZE];
        sprintf(filename, "%s/%d", output_dir, pid);

        int file_fd = open(filename, O_RDONLY);

        if (file_fd == -1) {
            // Debug: opening failed
            perror("Error opening file");
            _exit(1);
        }

        ssize_t bytes_read = read(file_fd, buffer, BUFFER_SIZE - 1);

        close(file_fd);

        if (bytes_read < 0) {
            // Debug: reading failed
            perror("read");
            _exit(1);
        }

        buffer[bytes_read] = '\0';

        char program[BUFFER_SIZE];

        long start_time;

        char *token_before;
        token_before = strtok(buffer, " ");
        strcpy(program, token_before);
        token_before = strtok(NULL, " ");
        start_time = atol(token_before);

        long elapsed_time = end_time - start_time;

        // Save end information to file
        file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

        if (file_fd == -1) {
            // Debug: opening failed
            perror("Error opening file");
            _exit(1);
        }

        num_written = snprintf(buffer, BUFFER_SIZE,  "%s %ld\n", program, elapsed_time);
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(file_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }

        close(file_fd);

        update_info(pid, elapsed_time, 0);

    } else if (strncmp (request, "status", 6) == 0) {
    
        // Open the server pipe for writing
        int client_fd = open(CLIENT_PIPE_NAME, O_WRONLY);
        if (client_fd == -1) {
            perror("Error opening client pipe");
            _exit(1);
        }

        int i = 0;
        while (i < MAX_ENTRIES && information[i] != NULL){
            if (information[i]->running == 1) {

                struct timeval time_so_far;
                gettimeofday(&time_so_far, NULL);

                long time_now = time_so_far.tv_sec * 1000 + time_so_far.tv_usec / 1000;

                char buffer[BUFFER_SIZE];
                int num_written = snprintf(buffer, BUFFER_SIZE, "%d %s %ld \n", information[i]->pid, information[i]->name, time_now - information[i]->time);

                if (num_written < 0 || num_written >= BUFFER_SIZE) {
                    perror("Error formatting message");
                    _exit(1);
                }
                if (write(client_fd, buffer, num_written) != num_written) {
                    perror("Error writing to client pipe");
                    _exit(1);
                }
            }
            i++;
        }

        close(client_fd);

    } else if (strncmp (request, "stats-time", 10) == 0) {
        
        // Open the client pipe for writing
        int client_fd = open(CLIENT_PIPE_NAME, O_WRONLY);
        if (client_fd == -1) {
            perror("Error opening client pipe");
            _exit(1);
        }

        // Parse the list of PIDs from the request
        char *token = strtok(request, " ");
        token = strtok(NULL, " ");
        long total_time = 0;

        int i = 0;
        while (token != NULL) {
            int pid = atoi(token);
            while (i < MAX_ENTRIES && information[i] != NULL){
                if (information[i]->pid == pid) {
                    total_time += information[i]->time;
                }
                i++;
            }
            token = strtok(NULL, " ");
        }


        // Write the total time to the client pipe
        char buffer[BUFFER_SIZE];
        int num_written = snprintf(buffer, BUFFER_SIZE, "Total execution time is %ld ms\n", total_time);
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            perror("Error formatting message");
            _exit(1);
        }
        if (write(client_fd, buffer, num_written) != num_written) {
            perror("Error writing to client pipe");
            _exit(1);
        }

        close(client_fd);
    } else if (strncmp(request, "stats-command", 13) == 0) {
        
        // Open the client pipe for writing
        int client_fd = open(CLIENT_PIPE_NAME, O_WRONLY);
        if (client_fd == -1) {
            perror("Error opening client pipe");
            _exit(1);
        }

        // Parse the program name and list of PIDs from the request
        char *token = strtok(request, " ");
        token = strtok(NULL, " ");
        char *program_name = token;
        token = strtok(NULL, " ");
        int count = 0;
        int i = 0;

        while (token != NULL) {
            int pid = atoi(token);

            while (i < MAX_ENTRIES && information[i] != NULL){
                if (information[i]->pid == pid) {
                    if (strcmp(information[i]->name, program_name) == 0) {
                        count++;
                    }
                }
                i++;
            }

            token = strtok(NULL, " ");
        }

        // Write the count to the client pipe
        char buffer[BUFFER_SIZE];
        int num_written = snprintf(buffer, BUFFER_SIZE, "%s was executed %d times\n", program_name, count);
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            perror("Error formatting message");
            _exit(1);
        }
        if (write(client_fd, buffer, num_written) != num_written) {
            perror("Error writing to client pipe");
            _exit(1);
        }

        close(client_fd);
    }else if (strncmp(request, "stats-uniq", 10) == 0) {
        // Open the client pipe for writing
        int client_fd = open(CLIENT_PIPE_NAME, O_WRONLY);
        if (client_fd == -1) {
            perror("Error opening client pipe");
            _exit(1);
        }

        // Parse the list of PIDs from the request
        char *token = strtok(request, " ");
        token = strtok(NULL, " ");
        char buffer[BUFFER_SIZE];
        int i = 0;
        char *program_save[BUFFER_SIZE];
        int num_progs = 0;
        int flag = 0;

        while (token != NULL) {
            int pid = atoi(token);

            while (i < MAX_ENTRIES && information[i] != NULL){
                if (information[i]->pid == pid) {
                    for(int j = 0; j < num_progs ; j++){
                        if(strcmp(information[i]->name, program_save[j]) == 0){
                            flag = 1;
                        }
                    }
                    if(flag == 0){
                        program_save[num_progs] = malloc(sizeof(char));
                        strcpy(program_save[num_progs], information[i]->name);
                        num_progs++;

                        // Write the program name to the client pipe
                        int num_written = snprintf(buffer, BUFFER_SIZE, "%s\n", information[i]->name);
                        if (num_written < 0 || num_written >= BUFFER_SIZE) {
                            perror("Error formatting message");
                            _exit(1);
                        }
                        if (write(client_fd, buffer, num_written) != num_written) {
                            perror("Error writing to client pipe");
                            _exit(1);
                        }
                    }
                }
                flag = 0;
                i++;
            }

            token = strtok(NULL, " ");
        }

        close(client_fd);

        // Free memory
        for (int i = 0; i < num_progs; i++) {
            free(program_save[i]);
        }

    } else {
        // Debug: request failed
        perror("request");
        _exit(1);
    }
}

/**
 * Main of the Server
 * @param[in] argc
 * @param[in] argv
 */

int main(int argc, char *argv[]) {

    char buffer[BUFFER_SIZE];

    int num_written;

    if (argc < 2) {

        // Instructions on the usage of the program
        num_written = snprintf(buffer, BUFFER_SIZE, "Usage: %s output_dir\n", argv[0]);
    
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            perror("Formatting message!");
            _exit(1);
        }

        if (write(2, buffer, num_written) != num_written) {
            perror("Writing");
            _exit(1);
        }
        _exit(1);
    }

    mkfifo(CLIENT_PIPE_NAME, 0666);
    mkfifo(SERVER_PIPE_NAME, 0666);

    output_dir = argv[1];


    while (1) {

        // Receive request from client
        int server_fd = open(SERVER_PIPE_NAME, O_RDONLY);
        if (server_fd == -1) {
            // Debug: open failed
            perror("open");
            _exit(1);
        }

        char line[BUFFER_SIZE];
        ssize_t bytes_read = readln(server_fd, line, BUFFER_SIZE - 1);
        close(server_fd);

        if (bytes_read > 0) {

            //line[bytes_read] = '\0';

            // Create new process to process request
            int pid = fork();

            if (pid == 0) {
                // Child process
                process_request(line);
                _exit(0);

            } else if (pid > 0) {
                // Parent process
                int status;
                waitpid(pid, &status, WNOHANG);

            } else {
                // Debug: fork failed
                perror("fork");
                _exit(1);
            }

        }

    }

    /*
    // This is not working But it would be good if it did!
    // Receive request from client
    int server_fd = open(SERVER_PIPE_NAME, O_RDONLY);
    open(SERVER_PIPE_NAME, O_WRONLY);
    if (server_fd == -1) {
        perror("open");
        _exit(1);
    }

    char line[BUFFER_SIZE];
    ssize_t bytes_read;
    

    if ((bytes_read = readln(server_fd, line, BUFFER_SIZE - 1)) > 0) {

        line[bytes_read] = '\0';

        // Create new process to process request
        int pid = fork();

        if (pid == 0) {
            // Child process
            process_request(line);
            _exit(0);

        } else if (pid > 0) {
            // Parent process
            int status;
            waitpid(pid, &status, WNOHANG);

        } else {
            perror("fork");
            _exit(1);
        }

    }

    close(server_fd);*/

    unlink(SERVER_PIPE_NAME);
    unlink(CLIENT_PIPE_NAME);

    return 0;
}