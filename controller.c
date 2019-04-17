#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h> 
#include <semaphore.h>

//Function Declarations
void *serverConn();
int makeSocket(void);

//Global Variables
char *serverPort = "65200";
char *dashboardPort = "65250";
char *host = "127.0.1.1";
int g_argc;
char **g_argv;
sem_t sem;

//Main Method
int main(int argc, char **argv) {
	g_argc = argc;
	g_argv = argv;
	sem_init(&sem,0,1);
	pthread_t server_thread;
	pthread_create(&server_thread, NULL, serverConn, NULL);
	pthread_join(server_thread, NULL);
	sem_destroy(&sem);
	return 0;
}

int makeSocket(void) {
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
 
    if(sock == -1) {
        fprintf(stderr, "error making socket: %s\n", strerror(errno));
        exit(1);
        return 0;
    }
 
    return sock;
}

//Server Connection
void *serverConn() {
	struct addrinfo *address;
	const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };
	int fd, err;
	
	sem_wait(&sem);
    err = getaddrinfo(host, serverPort, &hints, &address);
    if (err) {
        fprintf(stderr, "Error getting address: %s\n", gai_strerror(err));
        exit(1);
    }

	//make the socket
	fd = makeSocket();
		
	const size_t buffsize = 4096;
	char incoming[buffsize], outgoing[buffsize];
	size_t msgsize;
	struct sockaddr clientaddr;
	socklen_t addrlen = sizeof(clientaddr);
	
	strcpy(outgoing, g_argv[1]);
	sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
	
	msgsize = recvfrom(fd, incoming, buffsize, 0, 0, 0);
	
	incoming[msgsize] = '\0';
	
	printf("reply: %s \n", incoming);
    sem_post(&sem); 
}

void *dashboardConn() {
	struct addrinfo *address;
	const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };
	int fd, err;
	
	sem_wait(&sem);
    err = getaddrinfo(host, dashboardPort, &hints, &address);
    if (err) {
        fprintf(stderr, "Error getting address: %s\n", gai_strerror(err));
        exit(1);
    }

	//make the socket
	fd = makeSocket();
		
	const size_t buffsize = 4096;
	char incoming[buffsize], outgoing[buffsize];
	size_t msgsize;
	struct sockaddr clientaddr;
	socklen_t addrlen = sizeof(clientaddr);
	
	strcpy(outgoing, g_argv[1]);
	sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
	
	msgsize = recvfrom(fd, incoming, buffsize, 0, 0, 0);
	
	incoming[msgsize] = '\0';
	
	printf("reply: %s \n", incoming);
    sem_post(&sem); 
}