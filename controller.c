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
void *serverCommunicationHandler(void *arg);
void *dataLogHandler(void *arg);
void userControls(int fd, struct addrinfo *address);
void updateDashboard(int fd, struct addrinfo *address);
void serverCommunication(int fd, struct addrinfo *address);
void clientMessage(int fd, struct addrinfo *address);
void dataLog(void);
int createSocket(void);
int getAddress(const char *hostname, const char *service, struct addrinfo **address);
void getData(int fd, struct addrinfo *address);

/* Global Variables*/
FILE *fp;
static sem_t sem; //check if static
char *host = "192.168.56.1";
//char *host = "127.0.1.1";
char *dashboardPort = "65250";
char *serverPort = "65200";
bool escPressed = false;
const size_t buffsize = 4096;
float fuel;
float altitude;
int points;
int mainEngine = 0;
float rcsRoll = 0;

/* Main Method */
int main(int argc, const char *argv)
{
    pthread_t dashboardCommunicationThread;
    pthread_t userInputThread;
    pthread_t serverCommunicationHandlerThread;
    pthread_t dataLogThread;
    int dc, ui, sc, dl, rc;

    //Dashboard Communication Thread
    dc = pthread_create(&dashboardCommunicationThread, NULL, dashboardCommunication, NULL);
    assert(dc == 0);

    //User Input Thread
    ui = pthread_create(&userInputThread, NULL, userInput, NULL);
    assert(ui == 0);

    //Server Communication Thread
    sc = pthread_create(&serverCommunicationHandlerThread, NULL, serverCommunicationHandler, NULL);
    assert(sc == 0);

    //Data Log Thread
    dl = pthread_create(&dataLogThread, NULL, dataLogHandler, NULL);
    assert(dl == 0);

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
    getAddress(host, serverPort, &address);
    fd = createSocket();
    userControls(fd, address);
    exit(0);
}

/* Sets up communication with the Lander Dashboard */
void *dashboardCommunication(void *arg)
{
    struct addrinfo *dashboardAddress;
    getAddress(host, dashboardPort, &dashboardAddress);
    int dashboardSocket = createSocket();
    
    while (1)
    {
        updateDashboard(dashboardSocket, dashboardAddress);
        sleep(1);
    }
}

/* Sets up communication with the Lander Server */
void *serverCommunicationHandler(void *arg)
{
    struct addrinfo *serverAddress;
    getAddress(host, serverPort, &serverAddress);
    int serverSocket = createSocket();
    
    while (1)
    {
        clientMessage(serverSocket, serverAddress);
    }
}

void *dataLogHandler(void *arg)
{
    fp = fopen("LanderLog.txt", "w");
    struct addrinfo *serverAddress;
    getAddress(host, serverPort, &serverAddress);
    int dataSocket = createSocket();

    while (1)
    {
        getData(dataSocket, serverAddress);
        dataLog();
        sleep(1);
    }
    fclose(fp);
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
                serverCommunication(fd, address);
            }
            break;

        case KEY_DOWN:
            if (mainEngine >= 10)
            {
                mainEngine -= 5;
                serverCommunication(fd, address);
            }
            break;

        case KEY_LEFT:
            if (rcsRoll > -0.5)
            {
                rcsRoll -= 0.1;
                serverCommunication(fd, address);
            }
            break;

        case KEY_RIGHT:
            if (rcsRoll <= 0.4)
            {
                rcsRoll += 0.1;
                serverCommunication(fd, address);
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
    endwin();
    exit(0);
}

/* Updates the Lander Dashboard */
void updateDashboard(int fd, struct addrinfo *address)
{
    char outgoing[buffsize];
    snprintf(outgoing, sizeof(outgoing), "fuel: %.2f \naltitude: %.2f", fuel, altitude);
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
}

/* Sends Commands to the Lander Server */
void serverCommunication(int fd, struct addrinfo *address)
{
    char outgoing[buffsize];
    //Critical Section
    snprintf(outgoing, sizeof(outgoing), "command:!\nmain-engine: %i\nrcs-roll: %f", mainEngine, rcsRoll);
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
}

/* Sends and Receives Messages to the Client */
void getData(int fd, struct addrinfo *address)
{
    char incoming[buffsize], outgoing[buffsize];
    size_t msgsize;
    int i = 0;
    int rc;

    strcpy(outgoing, "terrain:?");
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);

    msgsize = recvfrom(fd, incoming, buffsize, 0, NULL, 0); /* Don't need the senders address */
    incoming[msgsize] = '\0';

    char *terrain = strtok(incoming, ":"); //split into key:value pair
    char *terrainConditions[4];

    while (terrain != NULL)
    {
        terrainConditions[i++] = terrain;
        terrain = strtok(NULL, ":");
    }
    rc = sem_wait(&sem);
    assert(rc == 0);

    char *points1 = strtok(terrainConditions[2], "data-x");
    points = atoi(points1);

    //char *data-x1 = strtok(landerConditions[3], ",");
    //data-x = strtof(altitude1, NULL);

    //Semaphore Post
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

    //Semaphore wait
    rc = sem_wait(&sem);
    assert(rc == 0);

    //Critical Section
    char *fuel1 = strtok(landerConditions[2], "%");
    fuel = strtof(fuel1, NULL);

    char *altitude1 = strtok(landerConditions[3], "contact");
    altitude = strtof(altitude1, NULL);

    //Semaphore Post
    rc = sem_post(&sem);
    assert(rc == 0);
}

/* Logs User Data to a Text File */
void dataLog(void)
{
    int rc;
    rc = sem_wait(&sem);
    assert(rc == 0);

    fprintf(fp, "----------------------------------------------\n");
    //fprintf(fp, "Key Pressed: %i\n", fd);
    fprintf(fp, "Lander Altitude: %.2f\n", altitude);
    fprintf(fp, "Lander Fuel: %.2f\n", fuel);
    fprintf(fp, "Data-Points: %i\n", points);

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
int getAddress(const char *hostname, const char *service, struct addrinfo **address)
{
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM};

    int err = getaddrinfo(hostname, service, &hints, address);

    if (err)
    {
        fprintf(stderr, "error getting address: %s\n", gai_strerror(err));
        exit(1);
        return false;
    }
    return true;
}