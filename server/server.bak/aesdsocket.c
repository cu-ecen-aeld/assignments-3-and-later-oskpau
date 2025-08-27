
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT "9000"
#define MAXIMUM_BACKLOG_CONNECTIONS 10
#define SOCKET_DATA_FILE_PATH "/var/tmp/aesdsocketdata"

static int sfd = -1;
static int cfd = -1;
static volatile sig_atomic_t program_exit_signal_received = 0;

void cleanup_resources(void) {
    if (cfd != -1) close(cfd);
    if (sfd != -1) close(sfd);
    unlink(SOCKET_DATA_FILE_PATH);
    closelog();
}

void signal_handler_function(int signal_number) {
    syslog(LOG_INFO, "Caught signal, exiting");
    program_exit_signal_received = 1;
}

void run_as_daemon(void) {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    for (int file_descriptor = sysconf(_SC_OPEN_MAX); file_descriptor >= 0; file_descriptor--) {
        close(file_descriptor);
    }
}

void handle_error(char* error_text) {
	perror(error_text);
	cleanup_resources();
	exit(EXIT_FAILURE);
}

int main(int argument_count, char *argument_values[]) {
    int run_in_daemon_mode = (argument_count == 2 && strcmp(argument_values[1], "-d") == 0);

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    struct sigaction signal_action;
    memset(&signal_action, 0, sizeof(signal_action));
    signal_action.sa_handler = signal_handler_function;
    sigaction(SIGINT, &signal_action, NULL);
    sigaction(SIGTERM, &signal_action, NULL);

    struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = INADDR_ANY;
    hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(NULL, PORT, &hints, &res) == -1) {
		handle_error("getaddrinfo");
	}
    
	sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sfd < 0) {
        syslog(LOG_ERR, "socket failed: %s", strerror(errno));
		handle_error("socket");
    }

    int socket_option_value = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &socket_option_value, sizeof(socket_option_value));

    if (bind(sfd, res->ai_addr, res->ai_addrlen) == -1) {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
		handle_error("bind");
    }

    if (run_in_daemon_mode) run_as_daemon();

    if (listen(sfd, MAXIMUM_BACKLOG_CONNECTIONS) < 0) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
		handle_error("listen");
    }

    while (!program_exit_signal_received) {
        struct sockaddr_in client_address_structure;
        socklen_t client_address_length = sizeof(client_address_structure);

        cfd = accept(sfd, (struct sockaddr*)&client_address_structure, &client_address_length);
        if (cfd < 0) {
            if (errno == EINTR) break;
            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }

        char client_ip_address_string[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address_structure.sin_addr, client_ip_address_string, sizeof(client_ip_address_string));
        syslog(LOG_INFO, "Accepted connection from %s", client_ip_address_string);

        FILE *data_file_pointer = fopen(SOCKET_DATA_FILE_PATH, "a+");
        if (!data_file_pointer) {
            syslog(LOG_ERR, "failed to open data file: %s", strerror(errno));
            close(cfd);
            continue;
        }

        char receive_buffer[1024];
        size_t bytes_in_buffer = 0;

        while (!program_exit_signal_received) {
            ssize_t number_of_bytes_received = recv(cfd, receive_buffer + bytes_in_buffer, sizeof(receive_buffer) - bytes_in_buffer - 1, MSG_DONTWAIT);
            if (number_of_bytes_received <= 0) break;
            bytes_in_buffer += number_of_bytes_received;
            receive_buffer[bytes_in_buffer] = '\0';

            char *newline_character_position = strchr(receive_buffer, '\n');
            if (newline_character_position) {
                size_t packet_length = newline_character_position - receive_buffer + 1;
                fwrite(receive_buffer, 1, packet_length, data_file_pointer);
                fflush(data_file_pointer);

                fseek(data_file_pointer, 0, SEEK_SET);
                char send_buffer[1024];
                size_t number_of_bytes_read;
                while ((number_of_bytes_read = fread(send_buffer, 1, sizeof(send_buffer), data_file_pointer)) > 0) {
                    send(cfd, send_buffer, number_of_bytes_read, 0);
                }

                size_t leftover_bytes = bytes_in_buffer - packet_length;
                memmove(receive_buffer, newline_character_position + 1, leftover_bytes);
                bytes_in_buffer = leftover_bytes;
            }
        }

        fclose(data_file_pointer);
        close(cfd);
        cfd = -1;
        syslog(LOG_INFO, "Closed connection from %s", client_ip_address_string);
    }

    cleanup_resources();
    return 0;
}
