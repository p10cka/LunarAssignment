/*
 * Author: Ryan Pickering
 * Student Number: 17013352
 * Version: 1
 * Date: 16/05/2019
 * 
 * A controller class that communicates with a Lunar Lander Server and 
 * Dashboard to display information, control the lander and log data.
 */

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
void getTerrain(int fd, struct addrinfo *address);
void getState(int fd, struct addrinfo *address);
void getCondition(int fd, struct addrinfo *address);
void dataLog(void);
int createSocket(void);
int getAddress(const char *hostname, const char *service, struct addrinfo **address);

/* Global Variables*/
FILE *fp;
static sem_t sem;
char *host = "192.168.56.1";
//char *host = "127.0.1.1";
char *dashboardPort = "65250";
char *serverPort = "65200";
bool escPressed = false;
const size_t buffsize = 4096;
float fuel;
float altitude;
int points;
int dataX;
int dataY;
float xPosition;
float yPosition;
int orientation;
float hVelocity;
float vVelocity;
float rotationRate;
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

    //Initialise the Semaphore
    rc = sem_init(&sem, 0, 1);
    assert(rc == 0);

    //Wait for the Dashboard Communication Thread to Terminate
    pthread_join(dashboardCommunicationThread, NULL);
    exit(0);
}

/* Sets up User Input Communication */
void *userInput(void *arg)
{
    struct addrinfo *address;
    getAddress(host, serverPort, &address);
    int userSocket = createSocket();

    userControls(userSocket, address);
    exit(0);
}

/* Sets up communication with the Lander Dashboard */
void *dashboardCommunication(void *arg)
{
    struct addrinfo *dashboardAddress;
    getAddress(host, dashboardPort, &dashboardAddress);
    int dashboardSocket = createSocket();
    
    //Update the dashboard every 1 second
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
    
    //Continuously get the condition of the lander
    while (1)
    {
        getCondition(serverSocket, serverAddress);
    }
}

/* Opens the LanderLog.txt file to log data, connects to the server
and logs data related to the status of the lander */
void *dataLogHandler(void *arg)
{
    //Opens the LanderLog.txt file for writing
    fp = fopen("LanderLog.txt", "w");
    struct addrinfo *serverAddress;
    getAddress(host, serverPort, &serverAddress);
    int dataSocket = createSocket();

    //While 1, get terrain data and log the data.
    while (1)
    {
        getTerrain(dataSocket, serverAddress);
        getState(dataSocket, serverAddress);
        dataLog();
        sleep(1); //Sleep for 1 second 
    }
    //Close the file
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
        //Increase engine power by 5 
        case KEY_UP:
            if (mainEngine <= 95)
            {
                mainEngine += 5;
                serverCommunication(fd, address);
            }
            break;

        //Decrease engine power by 5
        case KEY_DOWN:
            if (mainEngine >= 5)
            {
                mainEngine -= 5;
                serverCommunication(fd, address);
            }
            break;

        //Tilt the lander left by 0.1
        case KEY_LEFT:
            if (rcsRoll > -0.5)
            {
                rcsRoll -= 0.1;
                serverCommunication(fd, address);
            }
            break;

        //Tilt the lander right by 0.1
        case KEY_RIGHT:
            if (rcsRoll <= 0.4)
            {
                rcsRoll += 0.1;
                serverCommunication(fd, address);
            }
            break;

        //If any other key is pressed, instruct the user what to do.    
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

/* Updates the Lander Dashboard with Fuel and Altitude */
void updateDashboard(int fd, struct addrinfo *address)
{
    char outgoing[buffsize];
    snprintf(outgoing, sizeof(outgoing), "fuel: %.2f \naltitude: %.2f", fuel, altitude);
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
}

/* Sends Commands to the Lander Server to Control the Lander */
void serverCommunication(int fd, struct addrinfo *address)
{
    char outgoing[buffsize];
    snprintf(outgoing, sizeof(outgoing), "command:!\nmain-engine: %i\nrcs-roll: %f", mainEngine, rcsRoll);
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
}

/* Requests Terrain Information from the Lander Server */
void getTerrain(int fd, struct addrinfo *address)
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
    //Semaphore Wait
    rc = sem_wait(&sem);
    assert(rc == 0);

    //Critical Section
    char *points1 = strtok(terrainConditions[2], "data-x");
    points = atoi(points1);

    char *dataX1 = strtok(terrainConditions[3], "data-y");
    dataX = atoi(dataX1);

    char *dataY1 = strtok(terrainConditions[4], "...");
    dataY = atoi(dataY1);
   
    //Semaphore Post
    rc = sem_post(&sem);
    assert(rc == 0);
}

/* Requests the State of the Lander from the Lander Server*/
void getState(int fd, struct addrinfo *address)
{
    char incoming[buffsize], outgoing[buffsize];
    size_t msgsize;
    int i = 0;
    int rc;

    strcpy(outgoing, "state:?");
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);

    msgsize = recvfrom(fd, incoming, buffsize, 0, NULL, 0); /* Don't need the senders address */
    incoming[msgsize] = '\0';

    char *state = strtok(incoming, ":"); //split into key:value pair
    char *stateConditions[15];

    while (state != NULL)
    {
        stateConditions[i++] = state;
        state = strtok(NULL, ":");
    }

    //Semaphore Wait
    rc = sem_wait(&sem);
    assert(rc == 0);

    //Critical Section
    char *xPosition1 = strtok(stateConditions[2], "y");
    xPosition = strtof(xPosition1, NULL);

    char *yPosition1 = strtok(stateConditions[3], "O");
    yPosition = strtof(yPosition1, NULL);

    char *orientation1 = strtok(stateConditions[4], "x'");//
    orientation = atoi(orientation1);

    char *hVelocity1 = strtok(stateConditions[5], "y'");
    hVeloctiy = strtof(hVelocity1, NULL);

    char *vVelocity1 = strtok(stateConditions[6], "O'");
    vVelocity = strtof(vVelocity1);

    char *rotiationRate1 = strtok(stateConditions[7], "");
    rotationRate = strtof(rotationRate1, NULL);

    //Semaphore Post
    rc = sem_post(&sem);
    assert(rc == 0);
}

/* Requests Condition Information from the Lander Server */
void getCondition(int fd, struct addrinfo *address)
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
    //Semaphore Wait
    rc = sem_wait(&sem);
    assert(rc == 0);

    fprintf(fp, "----------------------------------------------\n");
    fprintf(fp, "Lander Altitude: %.2f\n", altitude);
    fprintf(fp, "Lander Fuel: %.2f\n", fuel);
    fprintf(fp, "Data-Points: %i\n", points);
    fprintf(fp, "X Ground Coordinates: %i\n", dataX);
    fprintf(fp, "Y Ground Coordinates: %i\n", dataY);
    fprintf(fp, "X Position: %.2f\n", xPosition);
    fprintf(fp, "Y Position: %.2f\n", yPosition);

    fprintf(fp, "Orientation %i\n", orientation);
    fprintf(fp, "Horizontal Velocity: %.2f\n", hVelocity);
    fprintf(fp, "Vertical Velocity: %.2f\n", vVelocity);
    fprintf(fp, "Rotation Rate: %.2f\n", rotationRate);

    //Semaphore Post
    rc = sem_post(&sem);
    assert(rc == 0);
}

/* Create Socket Utility Function 
    Creates a socket or returns an error if unsuccessful. */
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

/* Get Address Utility Function 
    Gets the requested address or returns an error if unsuccessful. */
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