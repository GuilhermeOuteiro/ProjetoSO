// @file tracer.c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdlib.h>

#define SERVER_PIPE_NAME "tmp/server_pipe" ///< Name of the server pipe
#define CLIENT_PIPE_NAME "tmp/client_pipe" ///< Name of the client pipe
#define BUFFER_SIZE 1024 ///< Size of every buffer

/**
 * Execute a single program given the request "execute -u"
 * @param[in] program Name of the program
 * @param[in] args Arguments of said program
 */
void execute_program(char *program, char **args) {

    // Execute program
    int pid = fork();

    char buffer[BUFFER_SIZE];

    int num_written;

    if(pid == 0){

        // Child process
        execvp(program, args);

        // Debug: execvp failed 
        perror("execvp");
        _exit(1);

    } else if (pid > 0) {

        // Parent process
        num_written = snprintf(buffer, BUFFER_SIZE, "Running PID %d\n", pid);
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(1, buffer, num_written) != num_written) {
            // Debug: Writing failed
            perror("Writing");
            _exit(1);
        }


        // Send start information to server
        int server_fd = open(SERVER_PIPE_NAME, O_WRONLY);

        if (server_fd == -1) {
            // Debug: opening failed
            perror("Error opening server pipe");
            _exit(1);
        }

        struct timeval start_time;
        gettimeofday(&start_time, NULL);

        long start_to_send = start_time.tv_sec * 1000 + start_time.tv_usec / 1000;

        num_written = snprintf(buffer, BUFFER_SIZE, "start %d %s %ld\n", pid, program, start_to_send);
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(server_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }
        
        close(server_fd);

        int status;
        waitpid(pid, &status, 0);

        struct timeval end_time;
        gettimeofday(&end_time, NULL);

        long end_to_send = end_time.tv_sec * 1000 + end_time.tv_usec / 1000;

        long elapsed_time = end_to_send - start_to_send;

        num_written = snprintf(buffer, BUFFER_SIZE, "Ended in %ld ms\n", elapsed_time);
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(1, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }

        // Send end information to server

        server_fd = open(SERVER_PIPE_NAME, O_WRONLY);

        if (server_fd == -1) {
            // Debug: opening failed
            perror("Error opening server pipe");
            _exit(1);
        }

        num_written = snprintf(buffer, BUFFER_SIZE,  "end %d %ld\n", pid, end_to_send);
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(server_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }
        
        close(server_fd);
        
    } else {
        // Debug: fork failed
        perror ("fork");
        _exit(1);
    }
}

/**
 * Execute a pipeline given the request "execute -p" followed by the arguments in between ""
 * @param[in] pipeline 
 */
void execute_pipeline(char *pipeline) {

    // Split pipeline into programs

    int pid_for_end;
    
    char buffer[BUFFER_SIZE];

    int num_written;

    struct timeval start_time;

    long start_to_send;

    char *programs[1024];

    int num_programs = 0;

    char *program = strtok(pipeline, "|");

    while (program != NULL) {
        programs[num_programs++] = program;
        program = strtok(NULL, "|");
    }

    int pipes[num_programs - 1][2];

    for (int i = 0; i < num_programs - 1; i++) {

        if (pipe(pipes[i]) == -1) {
            // Debug: pipe failed
            perror("pipe");
            _exit(1);
        }   
    }

    for (int i = 0; i < num_programs; i++) {
        
        char *program = programs[i];
        char *args[1024];
        int num_args = 0;
        char *arg = strtok(program, " ");

        while (arg != NULL) {
            args[num_args++] = arg;
            arg = strtok(NULL, " ");
        }

        args[num_args] = NULL;

        int pid = fork();

        if (pid == 0) {

            // Child process
            if (i > 0) {
                dup2(pipes[i - 1][0], 0);
            }

            if (i < num_programs - 1) {
                dup2(pipes[i][1], 1);
            }

            for (int j = 0; j < num_programs - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            execvp(args[0], args);

            // Debug: execvp failed
            perror("execvp");

            _exit(1);

        } else if (pid > 0) {

            // Parent process
            if (i == 0) {

                pid_for_end = pid;

                num_written = snprintf(buffer, BUFFER_SIZE, "Running PID %d\n", pid);
        
                if (num_written < 0 || num_written >= BUFFER_SIZE) {
                    // Debug: message formatting failed
                    perror("Formatting message!");
                    _exit(1);
                }

                if (write(1, buffer, num_written) != num_written) {
                    // Debug: writing failed
                    perror("Writing");
                    _exit(1);
                }

                // Send start information to server

                int server_fd = open(SERVER_PIPE_NAME, O_WRONLY);

                if (server_fd == -1) {
                    // Debug: opening failed
                    perror("Error opening server pipe");
                    _exit(1);
                }

                gettimeofday(&start_time, NULL);

                start_to_send = start_time.tv_sec * 1000 + start_time.tv_usec / 1000;

                num_written = snprintf(buffer, BUFFER_SIZE, "start %d %s %ld\n", pid, pipeline, start_to_send);
                
                if (num_written < 0 || num_written >= BUFFER_SIZE) {
                    // Debug: message formatting failed
                    perror("Formatting message!");
                    _exit(1);
                }

                if (write(server_fd, buffer, num_written) != num_written) {
                    // Debug: writing failed
                    perror("Writing");
                    _exit(1);
                }
                
                close(server_fd);
            
            }

        } else {
            // Debug: fork failed
            perror ("fork");
            _exit(1);
        }
    }


    for (int i = 0; i < num_programs - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int i = 0; i < num_programs; i++) {
        int status;
        wait(&status);
    }

    struct timeval end_time;
    gettimeofday(&end_time, NULL);

    long end_to_send = end_time.tv_sec * 1000 + end_time.tv_usec / 1000;

    long elapsed_time = end_to_send - start_to_send;

    num_written = snprintf(buffer, BUFFER_SIZE, "Ended in %ld ms\n", elapsed_time);
    
    if (num_written < 0 || num_written >= BUFFER_SIZE) {
        // Debug: message formatting failed
        perror("Formatting message!");
        _exit(1);
    }

    if (write(1, buffer, num_written) != num_written) {
        // Debug: writing failed
        perror("Writing");
        _exit(1);
    }

    // Send information to server

    int server_fd = open(SERVER_PIPE_NAME, O_WRONLY);

    if (server_fd == -1) {
        // Debug: opening failed
        perror("Error opening server pipe");
        _exit(1);
    }

    num_written = snprintf(buffer, BUFFER_SIZE, "end %d %ld\n", pid_for_end, end_to_send);
    
    if (num_written < 0 || num_written >= BUFFER_SIZE) {
        // Debug: message formatting failed
        perror("Formatting message!");
        _exit(1);
    }

    if (write(server_fd, buffer, num_written) != num_written) {
        // Debug: writing failed
        perror("Writing");
        _exit(1);
    }
    
    close(server_fd);
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
        num_written = snprintf(buffer, BUFFER_SIZE, "Usage: %s [execute [-u | -p] program [args...] | status | stats-time pids... | stats-command command pids... | stats-uniq pids...]\n", argv[0]);
    
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(2, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }

        _exit(1);
    }

    if (strcmp(argv[1], "execute") == 0) {
        if (argc < 4 || (strcmp(argv[2], "-u") != 0 && strcmp(argv[2], "-p") != 0)) {

            // Instructions on the usage of the program
            num_written = snprintf(buffer, BUFFER_SIZE, "Usage: %s execute [-u | -p] program [args...]\n", argv[0]);
    
            if (num_written < 0 || num_written >= BUFFER_SIZE) {
                // Debug: message formatting failed
                perror("Formatting message!");
                _exit(1);
            }

            if (write(2, buffer, num_written) != num_written) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }
            _exit(1);
        }

        // Execute program or pipeline

        if (strcmp(argv[2], "-u") == 0) {


            char *program = argv[3];
            char **args = &argv[3];

            execute_program(program, args);


        } else if (strcmp(argv[2], "-p") == 0) {

            char *pipeline = argv[3];

            execute_pipeline(pipeline);

        }





    } else if (strcmp(argv[1], "status") == 0) {

        // Send status request to server

        int server_fd = open(SERVER_PIPE_NAME, O_WRONLY);

        if (server_fd == -1) {
            // Debug: opening failed
            perror("Error opening server pipe");
            _exit(1);
        }

        num_written = snprintf(buffer, BUFFER_SIZE, "status\n");
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(server_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }
        
        close(server_fd);

        // Receive response from server

        int client_fd = open(CLIENT_PIPE_NAME, O_RDONLY);

        if (client_fd == -1) {
            // Debug: opening failed
            perror("Opening client pipe");
            _exit(1);
        }

        ssize_t bytes_read;

        while ((bytes_read = read(client_fd, buffer, sizeof(buffer))) > 0) {
            if (write(1, buffer, bytes_read) != bytes_read) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }
        }

        if (bytes_read == -1) {
            // Debug: reading failed
            perror("Reading");
            _exit(1);
        }

        close(client_fd);


    } else if (strcmp(argv[1], "stats-time") == 0) {

        if (argc < 3) {

            // Instructions on the usage of the program
            num_written = snprintf(buffer, BUFFER_SIZE, "Usage: %s stats-time pids...\n", argv[0]);
    
            if (num_written < 0 || num_written >= BUFFER_SIZE) {
                // Debug: message formatting failed
                perror("Formatting message!");
                _exit(1);
            }

            if (write(2, buffer, num_written) != num_written) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }

            _exit(1);
        }



        // Send stats-time request to server

        int server_fd = open(SERVER_PIPE_NAME, O_WRONLY);

        if (server_fd == -1) {
            // Debug: opening failed
            perror("Error opening server pipe");
            _exit(1);
        }

        num_written = snprintf(buffer, BUFFER_SIZE, "stats-time");
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(server_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }

        for (int i = 2; i < argc; i++) {
            num_written = snprintf(buffer, BUFFER_SIZE, " %s", argv[i]);
        
            if (num_written < 0 || num_written >= BUFFER_SIZE) {
                // Debug: message formatting failed
                perror("Formatting message!");
                _exit(1);
            }

            if (write(server_fd, buffer, num_written) != num_written) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }
        }

        num_written = snprintf(buffer, BUFFER_SIZE, "\n");
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(server_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }
        
        close(server_fd);



        // Receive response from server

        int client_fd = open(CLIENT_PIPE_NAME, O_RDONLY);

        if (client_fd == -1) {
            // Debug: opening failed
            perror("Opening client pipe");
            _exit(1);
        }

        ssize_t bytes_read;

        while ((bytes_read = read(client_fd, buffer, sizeof(buffer))) > 0) {
            if (write(1, buffer, bytes_read) != bytes_read) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }
        }

        if (bytes_read == -1) {
            // Debug: reading failed
            perror("Reading");
            _exit(1);
        }

        close(client_fd);



    } else if (strcmp(argv[1], "stats-command") == 0) {

        if (argc < 4) {

            // Instructions on the usage of the program
            num_written = snprintf(buffer, BUFFER_SIZE, "Usage: %s stats-command command pids...\n", argv[0]);
    
            if (num_written < 0 || num_written >= BUFFER_SIZE) {
                // Debug: message formatting failed
                perror("Formatting message!");
                _exit(1);
            }

            if (write(2, buffer, num_written) != num_written) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }

            _exit(1);
        }

        char *command = argv[2];

        // Send stats-command request to server

        int server_fd = open(SERVER_PIPE_NAME, O_WRONLY);

        if (server_fd == -1) {
            // Debug: opening failed
            perror("Error opening server pipe");
            _exit(1);
        }

        num_written = snprintf(buffer, BUFFER_SIZE, "stats-command %s", command);
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(server_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }
        
        for (int i = 3; i < argc; i++) {
            num_written = snprintf(buffer, BUFFER_SIZE, " %s", argv[i]);
        
            if (num_written < 0 || num_written >= BUFFER_SIZE) {
                // Debug: message formatting failed
                perror("Formatting message!");
                _exit(1);
            }

            if (write(server_fd, buffer, num_written) != num_written) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }
        }

        num_written = snprintf(buffer, BUFFER_SIZE, "\n");
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(server_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }

        close(server_fd);

        // Receive response from server

        int client_fd = open(CLIENT_PIPE_NAME, O_RDONLY);

        if (client_fd == -1) {
            // Debug: opening failed
            perror("Opening client pipe");
            _exit(1);
        }

        ssize_t bytes_read;

        while ((bytes_read = read(client_fd, buffer, sizeof(buffer))) > 0) {
            if (write(1, buffer, bytes_read) != bytes_read) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }
        }

        if (bytes_read == -1) {
            // Debug: reading failed
            perror("Reading");
            _exit(1);
        }

        close(client_fd);



    } else if (strcmp(argv[1], "stats-uniq") == 0) {

        if (argc < 3) {

            // Instructions on the usage of the program
            num_written = snprintf(buffer, BUFFER_SIZE, "Usage: %s stats-uniq pids...\n", argv[0]);
    
            if (num_written < 0 || num_written >= BUFFER_SIZE) {
                // Debug: message formatting failed
                perror("Formatting message!");
                _exit(1);
            }

            if (write(2, buffer, num_written) != num_written) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }

            _exit(1);
        }

        // Send stats-uniq request to server

        int server_fd = open(SERVER_PIPE_NAME, O_WRONLY);

        if (server_fd == -1) {
            // Debug: opening failed
            perror("Error opening server pipe");
            _exit(1);
        }

        num_written = snprintf(buffer, BUFFER_SIZE, "stats-uniq");
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(server_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }
        
        for (int i = 2; i < argc; i++) {
            num_written = snprintf(buffer, BUFFER_SIZE, " %s", argv[i]);
        
            if (num_written < 0 || num_written >= BUFFER_SIZE) {
                // Debug: message formatting failed
                perror("Formatting message!");
                _exit(1);
            }

            if (write(server_fd, buffer, num_written) != num_written) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }
        }

        num_written = snprintf(buffer, BUFFER_SIZE, "\n");
        
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(server_fd, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }

        close(server_fd);

        // Receive response from server

        int client_fd = open(CLIENT_PIPE_NAME, O_RDONLY);

        if (client_fd == -1) {
            // Debug: opening failed
            perror("Opening client pipe");
            _exit(1);
        }

        ssize_t bytes_read;

        while ((bytes_read = read(client_fd, buffer, sizeof(buffer))) > 0) {
            if (write(1, buffer, bytes_read) != bytes_read) {
                // Debug: writing failed
                perror("Writing");
                _exit(1);
            }
        }

        if (bytes_read == -1) {
            // Debug: reading failed
            perror("Reading");
            _exit(1);
        }

        close(client_fd);



    } else {

        // Instructions on the usage of the program
        num_written = snprintf(buffer, BUFFER_SIZE, "Usage: %s [execute [-u | -p] program [args...] | status | stats-time pids... | stats-command command pids... | stats-uniq pids...]\n", argv[0]);
    
        if (num_written < 0 || num_written >= BUFFER_SIZE) {
            // Debug: message formatting failed
            perror("Formatting message!");
            _exit(1);
        }

        if (write(2, buffer, num_written) != num_written) {
            // Debug: writing failed
            perror("Writing");
            _exit(1);
        }
        _exit(1);

    }
    return 0;
}
