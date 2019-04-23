#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
 
/* Method Declarations */
int getaddr(const char *node, const char *service, struct addrinfo **address);
void sendCommand(int fd, struct addrinfo *address);
void clientMessage(int fd, struct addrinfo *address);
void userControls(int fd, struct addrinfo *address);
void updateDashboard(int fd, struct addrinfo *address);
void* userInputThreadController(void *arg);
void* dashThreadController(void *arg);
int createSocket(void);
int printFile(int fd);
 
/* Global Variables*/
char *host = "192.168.56.1"; //localhost?
char *dashPort = "65250";
char *landerPort = "65200";
const size_t buffsize = 4096;

int speed = 0;
int changeSpeed = 5;
float rcsInc = 0.1;
float rcsRoll = 0;
char *fuel;
char *altitude;
 
int main(int argc, const char **argv) { //try with *argv
    pthread_t dashboard;
    pthread_t userInputThread;
    int rc;
    int tr;

    rc = pthread_create(&dashboard, NULL, dashThreadController, NULL);
    assert(rc == 0);

    tr = pthread_create(&userInputThread, NULL, userInputThreadController, NULL);
    assert(tr == 0);
    
    pthread_join(dashboard, NULL);
    exit(0);
}
 
void* userInputThreadController(void *arg) {
    struct addrinfo *address;
 
    int fd;
 
    getaddr(host, landerPort, &address);
    fd = createSocket();
 
    userControls(fd, address);
    exit(0);
}
 
void* dashThreadController(void *arg) {
    struct addrinfo *dashAddress, *landerAddress;
 
    int dashSocket, landerSocket;
 
    getaddr(host, dashPort, &dashAddress);
    getaddr(host, landerPort, &landerAddress);
 
    dashSocket = createSocket();
    landerSocket = createSocket();
 
    while (1) {
        clientMessage(landerSocket, landerAddress);
        updateDashboard(dashSocket, dashAddress);
    }
}
 
//need to change cases
void userControls(int fd, struct addrinfo *address) {
    //initialises curses data structures
    initscr();
    //disables character printing to the screen
    noecho();
    //allows arrow keys
    keypad(stdscr, TRUE); 
 
    int input;
    printw("Controls: \n");
	printw("Left Arrow Key - Rotate Left \n");
	printw("Right Arrow Key - Rotate Right \n");
	printw("Up Arrow Key - Increase Power \n");
	printw("Down Arrow Key - Reduce Power \n");
	printw("ESC - Quit The Game \n");
 
    //while the esc key has not been pressed
    while ((input=getch()) != 27) { 
        //moves cursor to middle of the terminal window 
		move(10, 0);
        printw("\nFuel: %s \nAltitude: %s", fuel, altitude);

        switch(input) {
			case KEY_UP:
			if (speed <= 90) {
			speed += changeSpeed;
            sendCommand(fd, address);
            }
			break;
			
			case KEY_DOWN:
			if (speed >= 10) {
			speed -= changeSpeed;
            sendCommand(fd, address);
            }
			break;
			
			case KEY_LEFT:
			if (rcsRoll > -0.5) {
			 rcsRoll -= rcsInc;
            sendCommand(fd, address);
            }
			break;
			
			case KEY_RIGHT:
			if (rcsRoll <= 0.4) {
			 rcsRoll += rcsInc;
            sendCommand(fd, address);
            }
			break;
		default:
        //printw("Key pressed has value = %d\n", input);
		printw("\nUse an arrow key to control the lander.");
		break;
		}
        move(0, 0);
        refresh();
    }
    endwin();
    exit(1);
}
 
void sendCommand(int fd, struct addrinfo *address) {
    char outgoing[buffsize];
 
    snprintf(outgoing, sizeof(outgoing), "command:!\nmain-engine: %i\nrcs-roll: %f", speed, rcsRoll);
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
}
 
void updateDashboard(int fd, struct addrinfo *address) {
    char outgoing[buffsize];
    snprintf(outgoing, sizeof(outgoing), "fuel: %s \naltitude: %s", fuel, altitude);
 
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
}
 
//kinda done 
void clientMessage(int fd, struct addrinfo *address) {
    char incoming[buffsize], outgoing[buffsize];
    size_t msgsize;
	int i = 0;
	//struct sockaddr clientaddr;
	//socklen_t addrlen = sizeof(clientaddr);
 
		strcpy(outgoing, "condition:?");
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
	
    msgsize = recvfrom(fd, incoming, buffsize, 0, NULL, 0);		/* Don't need the senders address */
	incoming[msgsize] = '\0';	
 
    char *landerCondition = strtok(incoming, ":"); //split into key:value pair
    char *landerConditions[4]; //try changing to 3
    
    while(landerCondition != NULL) { 
        landerConditions[i++] = landerCondition;
        landerCondition = strtok(NULL, ":");
    }
 
    fuel = strtok(landerConditions[2], "%");
    altitude = strtok(landerConditions[3], "contact");
}
 
//todo 
int getaddr(const char *hostname, const char *service, struct addrinfo **address) {
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM
    };
 
    if(hostname) {
        hints.ai_flags = AI_CANONNAME;
    }
    else {
        hints.ai_flags = AI_PASSIVE;
    }

    int err = getaddrinfo(hostname, service, &hints, address);
 
    if (err) {
        fprintf(stderr, "Error getting address: %s\n", gai_strerror(err));
        exit(1);
		return false;
    }
    return true;
	
}
 
//done 
int createSocket(void) {
    int sock;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        fprintf(stderr, "error making socket: %s\n", strerror(errno));
        exit(1);
		return 0;
    }
	return sock;
}
 
//done 
int printFile(int fd) {
    FILE *fp;
    fp = fopen("DataLog.txt", "w");
    fprintf(fp, "Key: %i\n", fd);
    fclose(fp);
} 