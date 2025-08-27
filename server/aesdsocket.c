/*
** server.c -- a stream socket server demo
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

#include "threading.h"

#define PORT "9000"  // the port users will be connecting to
#define DATAFILE "/var/tmp/aesdsocketdata"
#define BACKLOG 10   // how many pending connections queue will hold
#define MAX_LENGTH 1024

void sigchld_handler(int s)
{
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void* handle_recv_data(void* thread_param) {
	struct thread_data* thread_func_args = (struct thread_data *) thread_param;

	int *sfd = thread_func_args->server_socket;
	close(*sfd); // child doesn't need the listener
    
	int *cfd = thread_func_args->client_socket;
	char buffer[MAX_LENGTH];
	int bytes_recv = recv(*cfd, buffer, MAX_LENGTH, 0);
	
	if (bytes_recv == -1) {
		perror("bytes_recv");
		exit(EXIT_FAILURE);
	}
	
	if (send(*cfd, buffer, bytes_recv, 0) == -1)
		perror("send");
	thread_func_args->thread_complete_success = true;
	return thread_param;
}

int main(void)
{
	// threading params
	int ret;
	pthread_t thread;
	pthread_mutex_t *mutex;

	struct thread_data *td = (struct thread_data*)malloc(sizeof(struct thread_data));
	td->thread_complete_success = false;
	td->wait_to_obtain_ms = 1000;
	td->wait_to_release_ms = 1000;
    
	// listen on sock_fd, new connection on cfd
    int sfd, cfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr; // connector's address info
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            exit(EXIT_FAILURE);
        }

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        if (bind(sfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sfd);
            perror("server: bind");
            exit(EXIT_FAILURE);
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(EXIT_FAILURE);
    }

    if (listen(sfd, BACKLOG) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop	
        sin_size = sizeof client_addr;
        cfd = accept(sfd, (struct sockaddr *)&client_addr,
            &sin_size);
        if (cfd == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        inet_ntop(client_addr.ss_family,
            get_in_addr((struct sockaddr *)&client_addr),
            s, sizeof s);
        printf("Accepted connection from %s\n", s);
		
		// point the thread data struct the memory location of cfd
		td->client_socket = &cfd;
		td->server_socket = &sfd;

		int rc;
		rc = pthread_mutex_lock(&mutex);
		ret = pthread_create(&thread, NULL, handle_recv_data, td);
		if (rc && !ret) {
			perror("pthread_mutex_lock");
			exit(EXIT_FAILURE);
		}

		if (ret) {
			free(td);
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}

		rc = pthread_mutex_unlock(&mutex);
		if (rc && !ret) {
			perror("pthread_mutex_unlock");
			exit(EXIT_FAILURE);
		}

        close(cfd);  // parent doesn't need this
    }

    return 0;
}
