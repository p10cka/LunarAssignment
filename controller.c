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

int main() {
	char *port = "65200";
	char *host = "localhost";
	struct addrinfo *address;
	
	int fd, addr;
	
	addr = getaddr(host, port, &address);
	if (!addr)
		exit(1);
	
	fd = makeSocket();
	if(fd == 0)
			exit(1);
		
	const size_t buffsize = 4096;
	char incoming[buffsize], outgoing[buffsize];
	size_t msgsize;
	struct sockaddr clientaddr;
	socklen_t addrlen = sizeof(clientaddr);
	
	strcpy(outgoing, "condition:?");
	sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
	
	msgsize = recvfrom(fd, incoming, buffsize, 0, 0, 0);
	
	incoming[msgsize] = '\0';
	
	printf("reply: %s \n", incoming);
}