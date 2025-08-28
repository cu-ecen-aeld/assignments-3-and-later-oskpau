/*
 * aesdsocket.c - a stream socket server for aesd
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT "9000"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold

#define MAX_LENGTH 1024

#define DATA_FILE "/var/tmp/aesdsocketdata"

volatile sig_atomic_t exit_flag = 0;

void signal_handler(int sig)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    exit_flag = 1;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int daemon_mode = 0;
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Setup signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        closelog();
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        closelog();
        return -1;
    }

    // listen on sock_fd, new connection on cfd
    int sfd, cfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address info
    socklen_t sin_size;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(rv));
        closelog();
        return -1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            syslog(LOG_ERR, "server: socket");
            continue;
        }

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            syslog(LOG_ERR, "setsockopt");
            close(sfd);
            continue;
        }

        if (bind(sfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sfd);
            syslog(LOG_ERR, "server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        syslog(LOG_ERR, "server: failed to bind");
        closelog();
        return -1;
    }

    // Daemonize if requested
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "fork");
            close(sfd);
            closelog();
            return -1;
        }
        if (pid > 0) {
            // Parent exits
            close(sfd);
            closelog();
            return 0;
        }
        // Child continues
    }

    if (listen(sfd, BACKLOG) == -1) {
        syslog(LOG_ERR, "listen");
        close(sfd);
        closelog();
        return -1;
    }

    syslog(LOG_INFO, "server: waiting for connections...");

    while (!exit_flag) {  // main accept() loop
        sin_size = sizeof their_addr;
        cfd = accept(sfd, (struct sockaddr *)&their_addr, &sin_size);
        if (cfd == -1) {
            if (errno == EINTR && exit_flag) {
                break;
            }
            syslog(LOG_ERR, "accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        syslog(LOG_INFO, "Accepted connection from %s", s);

        // Handle connection
        char *buf = NULL;
        size_t buf_capacity = 0;
        size_t data_len = 0;

        while (1) {
            char recv_buf[MAX_LENGTH];
            ssize_t recv_len = recv(cfd, recv_buf, sizeof(recv_buf), 0);

            if (recv_len == 0) {
                // Connection closed
                break;
            } else if (recv_len == -1) {
                if (errno == EINTR) {
                    if (exit_flag) {
                        // Gracefully exit on signal
                        goto close_connection;
                    }
                    continue;
                }
                syslog(LOG_ERR, "recv");
                goto close_connection;
            }

            // Grow buffer if needed
            if (data_len + recv_len + 1 > buf_capacity) {
                size_t new_capacity = buf_capacity ? buf_capacity * 2 : MAX_LENGTH;
                while (new_capacity < data_len + recv_len + 1) {
                    new_capacity *= 2;
                }
                char *new_buf = realloc(buf, new_capacity);
                if (!new_buf) {
                    syslog(LOG_ERR, "realloc failed, discarding packet");
                    // Discard current accumulated data
                    data_len = 0;
                    continue;
                }
                buf = new_buf;
                buf_capacity = new_capacity;
            }

            memcpy(buf + data_len, recv_buf, recv_len);
            data_len += recv_len;
            buf[data_len] = '\0';  // Null-terminate for strchr

            // Process complete packets
            char *nl_pos;
            while ((nl_pos = strchr(buf, '\n')) != NULL) {
                size_t packet_len = nl_pos - buf + 1;

                // Append packet to file
                int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                if (fd == -1) {
                    syslog(LOG_ERR, "open append");
                    goto close_connection;
                }
                ssize_t written = write(fd, buf, packet_len);
                close(fd);
                if (written != (ssize_t)packet_len) {
                    syslog(LOG_ERR, "write");
                    goto close_connection;
                }

                // Send full file content
                fd = open(DATA_FILE, O_RDONLY);
                if (fd == -1) {
                    syslog(LOG_ERR, "open read");
                    goto close_connection;
                }
                char buffer[MAX_LENGTH];
                ssize_t read_len;
                while ((read_len = read(fd, buffer, sizeof(buffer))) > 0) {
                    ssize_t sent = 0;
                    while (sent < read_len) {
                        ssize_t this_sent = send(cfd, buffer + sent, read_len - sent, 0);
                        if (this_sent == -1) {
                            if (errno == EINTR) {
                                if (exit_flag) {
                                    close(fd);
                                    goto close_connection;
                                }
                                continue;
                            }
                            syslog(LOG_ERR, "send");
                            close(fd);
                            goto close_connection;
                        }
                        sent += this_sent;
                    }
                }
                close(fd);
                if (read_len < 0) {
                    syslog(LOG_ERR, "read");
                    goto close_connection;
                }

                // Shift remaining data
                size_t remain_len = data_len - packet_len;
                memmove(buf, nl_pos + 1, remain_len);
                data_len = remain_len;
                buf[data_len] = '\0';
            }
        }

close_connection:
        free(buf);
        syslog(LOG_INFO, "Closed connection from %s", s);
        close(cfd);
    }

    close(sfd);
    unlink(DATA_FILE);
    closelog();
    return 0;
}
