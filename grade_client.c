#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <memory.h>
#include <pthread.h>

#define BUFFSIZE 256

#define DO_SYS(syscall) do {		\
    if( (syscall) == -1 ) {		\
        perror( #syscall );		\
        exit(EXIT_FAILURE);		\
    }						\
} while( 0 )


///* Function prototypes *///

struct addrinfo* alloc_tcp_addr(const char *host, uint16_t port, int flags);
int tcp_connect(const char* host, uint16_t port);
char *read_command(void);
void grade_client(const char *host, uint16_t port);

///* Structs and custom types *///

enum {RD, WR};



int main(int argc, char *argv[]) {
    char *server_name = argv[1];
    int port;
    sscanf(argv[2], "%d", &port);

    grade_client(server_name, port);

    return 0;
}



///* Function definitions *///

struct addrinfo*
alloc_tcp_addr(const char *host, uint16_t port, int flags) {
    int err;   struct addrinfo hint, *a;   char ps[16];

    snprintf(ps, sizeof(ps), "%hu", port); // why string?
    memset(&hint, 0, sizeof(hint));
    hint.ai_flags    = flags;
    hint.ai_family   = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = IPPROTO_TCP;

    if( (err = getaddrinfo(host, ps, &hint, &a)) != 0 ) {
        fprintf(stderr,"%s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    return a; // should later be freed with freeaddrinfo()
}


// Return client fd connect()ed to host+port
int tcp_connect(const char* host, uint16_t port) {
    int clifd;
    struct addrinfo *a = alloc_tcp_addr(host, port, 0);

    DO_SYS( clifd = socket( a->ai_family,
				 a->ai_socktype,
				 a->ai_protocol ) 	);
    DO_SYS( connect( clifd,
				 a->ai_addr,
				 a->ai_addrlen  )   );

    freeaddrinfo( a );
    return clifd;
}


// Read a line of any length to a dynamic-sized array
char *read_command(void) {
    int buff_size = BUFFSIZE;
    int position = 0, curr_char;
    char *cmd_buffer = (char *) malloc(buff_size * sizeof(char));

    if (!cmd_buffer) {
        fprintf(stderr, "Command buffer allocation error\n");
        exit(EXIT_FAILURE);
    }

    memset(cmd_buffer, '\0', buff_size);

    while (1) {
        // Read a single char
        curr_char = getchar();

        // If we hit end of line, replace it with a null char
        if (curr_char == EOF || curr_char == '\n') {
            cmd_buffer[position] = '\0';
            return cmd_buffer;
        }
        cmd_buffer[position] = curr_char;
        position++;

        // If the command is longer than the buffer (256 by default), extend by 256
        if (position >= buff_size) {
            buff_size += BUFFSIZE;
            cmd_buffer = (char *) realloc(cmd_buffer, buff_size);
            if (!cmd_buffer) {
                fprintf(stderr, "Command buffer reallocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}


// Client Function
void grade_client(const char *host, uint16_t port) {
    int fd = tcp_connect(host, port);
    int input_pipe[2], output_pipe[2];
    int exit_requested = 0;

    // Input and output pipes initialization and error checks
    if (pipe(input_pipe)) {
        fprintf(stderr, "Input pipe error\n");
        exit(EXIT_FAILURE);
    }
    if (pipe(output_pipe)) {
        fprintf(stderr, "Output pipe error\n");
        exit(EXIT_FAILURE);
    }

    pid_t pid_son = fork();
    if (pid_son != 0) { // Parent
        char *command;
        char server_response[BUFFSIZE];

        // Close irrelevant pipe FDs
        DO_SYS(close(input_pipe[RD]));
        DO_SYS(close(output_pipe[WR]));

        do {
            // Read a command, write it to child process, and free the buffer
            printf("> ");
            command = read_command();
            if (strcmp(command, "") == 0) {
                fprintf(stdout, "Wrong input\n");
                continue;
            }
            exit_requested = !strcmp(command, "Exit");

            DO_SYS(write(input_pipe[WR], command, strlen(command)));

            memset(server_response, '\0', BUFFSIZE);
            DO_SYS(read(output_pipe[RD], server_response, BUFFSIZE));
            fprintf(stdout, "%s\n", server_response);

            free(command);
        } while (!exit_requested);

        wait(NULL);
    }
    else { // Child
        char from_client[BUFFSIZE], from_server[BUFFSIZE];
        int k = 0;
	
	// Close irrelevant pipe FDs
        DO_SYS(close(input_pipe[WR]));
        DO_SYS(close(output_pipe[RD]));
	
	// Keep reading until Exit command is sent
        do {
	    // Reset both buffers
            memset(from_client, '\0', sizeof(from_client) / sizeof(char));
            memset(from_server, '\0', sizeof(from_server) / sizeof(char));
	    
	    // Read from parent and check whether Exit command was requested
            DO_SYS(k = read(input_pipe[RD], from_client, BUFFSIZE));
            exit_requested = !strcmp(from_client, "Exit");
	    // Write to server
            DO_SYS(write(fd, from_client, k));
	    // Read back from server
            DO_SYS(k = read(fd, from_server, BUFFSIZE));
	    // Write server's respone to parent
            DO_SYS(write(output_pipe[WR], from_server, k + 1));
        } while (!exit_requested);

        DO_SYS(close(fd));
    }
}
