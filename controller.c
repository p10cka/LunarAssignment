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
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

/* Method Declarations */
void *userInput(void *arg);
void *dashboardCommunication(void *arg);
void *serverCommunication(void *arg);
void userControls(int fd, struct addrinfo *address);
void updateDashboard(int fd, struct addrinfo *address);
void sendCommand(int fd, struct addrinfo *address);
void clientMessage(int fd, struct addrinfo *address);
void dataLog(int fd);
int createSocket(void);
int getaddr(const char *node, const char *service, struct addrinfo **address);

/* Global Variables*/
static sem_t sem; //check if static
FILE *fp;
char *host = "192.168.56.1"; //localhost?
char *dashboardPort = "65250";
char *serverPort = "65200";
bool escPressed = false;
const size_t buffsize = 4096;
float fuel;
float altitude;
int mainEngine = 0;
float rcsRoll = 0;

/* Main Method */
int main(int argc, const char *argv)
{ 
    pthread_t dashboardCommunicationThread;
    pthread_t userInputThread;
    pthread_t serverCommunicationThread;
    //pthread_t dataLogThread;
    fp = fopen("DataLog.txt", "w");
    int dc, ui, sc, dl, rc;

    //Dashboard Communication Thread
    dc = pthread_create(&dashboardCommunicationThread, NULL, dashboardCommunication, NULL);
    assert(dc == 0);

    //User Input Thread
    ui = pthread_create(&userInputThread, NULL, userInput, NULL);
    assert(ui == 0);

    //Server Communication Thread
    sc = pthread_create(&serverCommunicationThread, NULL, serverCommunication, NULL);
    assert(sc == 0);

    //Data Log Thread
    //dl = pthread_create(&dataLogThread, NULL, dataLog, NULL);
    //assert(dl == 0);

    rc = sem_init(&sem, 0, 1);
    assert(rc == 0);
    pthread_join(dashboardCommunicationThread, NULL);
    exit(0);
}

/* Sets up User Input Communication */
void *userInput(void *arg)
{
    struct addrinfo *address;
    int fd;
    getaddr(host, serverPort, &address);
    fd = createSocket();
    userControls(fd, address);
    exit(0);
}

/* Sets up communication with the Lander Dashboard */
void *dashboardCommunication(void *arg)
{
    struct addrinfo *dashboardAddress;
    getaddr(host, dashboardPort, &dashboardAddress);
    int dashboardSocket = createSocket();
    while (1)
    {
        updateDashboard(dashboardSocket, dashboardAddress);
        sleep(1); //quick hack for now
    }
}

/* Sets up communication with the Lander Server */
void *serverCommunication(void *arg)
{
    struct addrinfo *serverAddress;
    getaddr(host, serverPort, &serverAddress);
    int serverSocket = createSocket();
    while (1)
    {
        clientMessage(serverSocket, serverAddress);
    }
}

/* User Controls to Control the Lander */
void userControls(int fd, struct addrinfo *address)
{
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
    while ((input = getch()) != 27)
    {
        //moves cursor to middle of the terminal window
        move(8, 0);
        printw("\nFuel: %.2f", fuel);
        printw("\nAltitude: %.2f", altitude);

        switch (input)
        {
        case KEY_UP:
            if (mainEngine <= 90)
            {
                mainEngine += 5;
                sendCommand(fd, address);
                dataLog(input);
            }
            break;

        case KEY_DOWN:
            if (mainEngine >= 10)
            {
                mainEngine -= 5;
                sendCommand(fd, address);
                dataLog(input);
            }
            break;

        case KEY_LEFT:
            if (rcsRoll > -0.5)
            {
                rcsRoll -= 0.1;
                sendCommand(fd, address);
                dataLog(input);
            }
            break;

        case KEY_RIGHT:
            if (rcsRoll <= 0.4)
            {
                rcsRoll += 0.1;
                sendCommand(fd, address);
                dataLog(input);
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
    escPressed = true;
    dataLog(input);
    endwin();
    exit(1);
}

/* Updates the Lander Dashboard */
void updateDashboard(int fd, struct addrinfo *address)
{
    int rc;
    char outgoing[buffsize];

    rc = sem_wait(&sem);
    assert(rc == 0);

    //Crititcal Section
    snprintf(outgoing, sizeof(outgoing), "fuel: %.2f \naltitude: %.2f", fuel, altitude);
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);

    rc = sem_post(&sem);
    assert(rc == 0);
}

/* Sends Commands to the Lander Server */
void sendCommand(int fd, struct addrinfo *address)
{
    int rc;
    char outgoing[buffsize];

    rc = sem_wait(&sem);
    assert(rc == 0);

    //Critical Section
    snprintf(outgoing, sizeof(outgoing), "command:!\nmain-engine: %i\nrcs-roll: %f", mainEngine, rcsRoll);
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);

    rc = sem_post(&sem);
    assert(rc == 0);
}

/* Sends and Receives Messages to the Client */
void clientMessage(int fd, struct addrinfo *address)
{
    char incoming[buffsize], outgoing[buffsize];
    size_t msgsize;
    int i = 0;
    int rc;
    //struct sockaddr clientaddr;
    //socklen_t addrlen = sizeof(clientaddr);

    rc = sem_wait(&sem);
    assert(rc == 0);

    //Critical Section
    strcpy(outgoing, "condition:?");
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);

    msgsize = recvfrom(fd, incoming, buffsize, 0, NULL, 0); /* Don't need the senders address */
    incoming[msgsize] = '\0';

    char *landerCondition = strtok(incoming, ":"); //split into key:value pair
    char *landerConditions[4];

    while (landerCondition != NULL)
    {
        landerConditions[i++] = landerCondition;
        landerCondition = strtok(NULL, ":");
    }

    char *fuel1 = strtok(landerConditions[2], "%");
    fuel = strtof(fuel1, NULL);

    char *altitude1 = strtok(landerConditions[3], "contact");
    altitude = strtof(altitude1, NULL);

    //Semaphore Post
    rc = sem_post(&sem);
    assert(rc == 0);
}

/* Logs User Data to a Text File */
void dataLog(int fd)
{
    int rc;
    rc = sem_wait(&sem);
    assert(rc == 0);

    fprintf(fp, "Key Pressed: %i\n", fd);
    fprintf(fp, "Lander Altitude: %.2f\n", altitude);
    fprintf(fp, "Lander Fuel: %.2f\n\n", fuel);

    if (escPressed)
    {
        fclose(fp);
    }

    rc = sem_post(&sem);
    assert(rc == 0);
}

/* Create Socket Utility Function */
int createSocket(void)
{
    int sock;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        fprintf(stderr, "error making socket: %s\n", strerror(errno));
        exit(1);
        return 0;
    }
    return sock;
}

/* Get Address Utility Function */
int getaddr(const char *hostname, const char *service, struct addrinfo **address)
{
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM
    };

    int err = getaddrinfo(hostname, service, &hints, address);

    if (err)
    {
        fprintf(stderr, "error getting address: %s\n", gai_strerror(err));
        exit(1);
        return false;
    }
    return true;
}