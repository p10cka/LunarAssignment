#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

 
#include <netinet/in.h>
#include <arpa/inet.h>
 
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ncurses.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
 
void getaddr(const char *node, const char *service, struct addrinfo **address);
int createSocket(void);
void sendCommand(int fd, struct addrinfo *address);
void getCondition(int fd, struct addrinfo *address);
void getUserInput(int fd, struct addrinfo *address);
void updateDashboard(int fd, struct addrinfo *address);
void* userInputThreadController(void *arg);
void* dashboardController(void *arg);

//global variables
char *host = "192.168.56.1"; 
char *dashPort = "65250";
char *landerPort = "65200";
const size_t buffsize = 4096;
int enginePower = 0;
int engineInc = 10;
float rcsInc = 0.1;
float rcsRoll = 0;
char *fuel;
char *altitude;
 
int main(int argc, const char **argv) {
    pthread_t dashThread;
    int dt = pthread_create(&dashThread, NULL, dashboardController, NULL);
 
    pthread_t userInputThread;
    int uit  = pthread_create(&userInputThread, NULL, userInputThreadController, NULL);
 
    if(dt != 0) {
        fprintf(stderr, "Could not create thread.\n");
        exit(-1);
    }
 
    if (uit != 0) {
        fprintf(stderr, "Could not create thread.\n");
        exit(-1);
    }
 	pthread_join(dashThread, NULL);
}
 
void* userInputThreadController(void *arg) {
    struct addrinfo *address;
    getaddr(host, landerPort, &address);
    int fd;
	fd = createSocket();
    getUserInput(fd, address);
    exit(0);
}
 
void* dashboardController(void *arg) {
    struct addrinfo *dashAddress, *landerAddress;
    int dashSocket, landerSocket;
 
    getaddr(host, dashPort, &dashAddress);
    getaddr(host, landerPort, &landerAddress);
 
    dashSocket = createSocket();
    landerSocket = createSocket();
 
    while (1) {
        getCondition(landerSocket, landerAddress);
        updateDashboard(dashSocket, dashAddress);
    }
}
 
void getUserInput(int fd, struct addrinfo *address) {
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
	printw("ESC - Quit The Game");
 
 //while the esc key has not been pressed
    while((input=getch()) != 27) { 
        //moves cursor to middle of the terminal window 
		move(6, 0); //10,0
        printw("\nAltitude: %sFuel Left: %s", altitude, fuel);

        switch(input) {
			case KEY_UP:
			if (enginePower <= 90)
			enginePower += engineInc;
            sendCommand(fd, address);
			break;
			
			case KEY_DOWN:
			if (enginePower >= 10)
			enginePower -= engineInc;
            sendCommand(fd, address);
			break;
			
			case KEY_LEFT:
			if (rcsRoll > -0.5)
			 rcsRoll -= rcsInc;
            sendCommand(fd, address);
			break;
			
			case KEY_RIGHT:
			if (rcsRoll <= 0.4)
			 rcsRoll += rcsInc;
            sendCommand(fd, address);
			break;
		default:
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
    snprintf(outgoing, sizeof(outgoing), "command:!\nmain-engine: %i\nrcs-roll: %f", enginePower, rcsRoll);
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);	
}
 
void updateDashboard(int fd, struct addrinfo *address) {
    char outgoing[buffsize];
    snprintf(outgoing, sizeof(outgoing), "Altitude: %s \nFuel Left: %s", altitude, fuel);
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
}
 
void getCondition(int fd, struct addrinfo *address) {
    char incoming[buffsize], outgoing[buffsize];
    size_t msgsize;
 
    strcpy(outgoing, "condition:?");
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
    msgsize = recvfrom(fd, incoming, buffsize, 0, NULL, 0);
    incoming[msgsize] = '\0';

    char *condition = strtok(incoming, ":");
    char *conditions[4]; 
    int i = 0;
 
    while(condition != NULL) {
        conditions[i++] = condition;
        condition = strtok(NULL, ":");
    }
 
    char *fuel_ = strtok(conditions[2], "%");
    fuel = fuel_;
    altitude = strtok(conditions[3], "contact");
}
 
void getaddr(const char *node, const char *service, struct addrinfo **address) {
    struct addrinfo hints = {
        .ai_flags = 0,
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM
    };
 
    if(node) {
        hints.ai_flags = AI_CANONNAME;
	}
    else {
        hints.ai_flags = AI_PASSIVE;
	}
 
    int err = getaddrinfo(node, service, &hints, address);
    if(err) {
        fprintf(stderr, "Error Getting Address: %s\n", gai_strerror(err));
        exit(1);
    }
}
 
int createSocket(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
 
    if(sock == -1) {
        fprintf(stderr, "Error making socket: %s\n", strerror(errno));
        exit(1);
        return 0;
    }
 
    return sock;
}