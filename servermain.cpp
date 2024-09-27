#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>    // Socket functions
#include <netinet/in.h>    // Internet address family
#include <netdb.h>         // Network database operations
#include <arpa/inet.h>     // Functions for IP addresses
#include <pthread.h>       // POSIX threads
#include <math.h>          // Math functions
#include <sys/select.h>    // I/O multiplexing
#include <sys/time.h>      // Time functions
#include "calcLib.h"       // Custom calculation library

#define MAX_CLIENTS 5          // Maximum number of concurrent clients
#define TIME_OUT 5             // Timeout in seconds for client responses
#define PROTOCOL_MSG "TEXT TCP 1.0\n"  // Initial protocol message
#define MAX_BUF_SIZE 1024      // Maximum buffer size for messages

// Global variables to track active clients
static int activeClients = 0;       // Current number of active clients
static int clientCounter = 0;       // Total number of clients connected
pthread_mutex_t clientLock = PTHREAD_MUTEX_INITIALIZER;  // Mutex for synchronizing access to client counts

// Structure to hold client information
typedef struct {
    int sockfd;     // Socket file descriptor for the client
    int clientID;   // Unique identifier for the client
} ClientInfo;

// Function to get the IPv4 or IPv6 address from a sockaddr structure
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        // Return IPv4 address
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    // Return IPv6 address
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Function to validate the result of a floating-point operation
bool validateFloatResult(const char* operation, double op1, double op2, double result) {
    double calculatedResult;
    // Perform the operation based on the operation type
    if (strcmp(operation, "fadd") == 0) calculatedResult = op1 + op2;
    else if (strcmp(operation, "fsub") == 0) calculatedResult = op1 - op2;
    else if (strcmp(operation, "fmul") == 0) calculatedResult = op1 * op2;
    else if (strcmp(operation, "fdiv") == 0) calculatedResult = op1 / op2;

    // Check if the result is within a small epsilon due to floating-point precision
    return fabs(calculatedResult - result) < 0.0001;
}

// Function to validate the result of an integer operation
bool validateIntResult(const char* operation, int op1, int op2, int result) {
    int calculatedResult;
    // Perform the operation based on the operation type
    if (strcmp(operation, "add") == 0) calculatedResult = op1 + op2;
    else if (strcmp(operation, "sub") == 0) calculatedResult = op1 - op2;
    else if (strcmp(operation, "mul") == 0) calculatedResult = op1 * op2;
    else if (strcmp(operation, "div") == 0) calculatedResult = op1 / op2;

    // Check if the calculated result matches the client's result
    return calculatedResult == result;
}

// Thread function to handle communication with a client
void* clientHandler(void* clientData) {
    ClientInfo* info = (ClientInfo*) clientData;
    char buffer[MAX_BUF_SIZE];   // Buffer for sending and receiving messages
    double floatVal1, floatVal2, floatRes;  // Variables for floating-point operations
    int intVal1, intVal2, intRes;           // Variables for integer operations

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(info->sockfd, &read_fds);

    // Set the timeout for client response
    struct timeval timeout;
    timeout.tv_sec = TIME_OUT;
    timeout.tv_usec = 0;

    // Wait for the client to respond using select()
    int activity = select(info->sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (activity <= 0) {
        // If select() returns 0 or negative, it's a timeout or error
        printf("Client[%d] timeout or error.\n", info->clientID);
        send(info->sockfd, "ERROR TO\n", 9, 0);
        close(info->sockfd);
        // Decrement the number of active clients
        pthread_mutex_lock(&clientLock);
        activeClients--;
        pthread_mutex_unlock(&clientLock);
        free(info);
        return NULL;
    }

    // Clear the buffer before receiving
    memset(buffer, 0, sizeof(buffer));

    // Receive the initial response from the client
    int int_value = recv(info->sockfd, &buffer, MAX_BUF_SIZE, 0);
    if (int_value < 0) {
        perror("recv error");
    }

    // Check if the client sent "OK\n" as expected
    if (strcmp(buffer, "OK\n") != 0) {
        printf("buffer:%s\n", buffer);
        printf("Client[%d] sent an invalid message.\n", info->clientID);
        close(info->sockfd);
        // Decrement the number of active clients
        pthread_mutex_lock(&clientLock);
        activeClients--;
        pthread_mutex_unlock(&clientLock);
        free(info);
        return NULL;
    }

    // Generate a random operation and operands
    char* operation = randomType();
    if (operation[0] == 'f') {
        // Floating-point operation
        floatVal1 = randomFloat();
        floatVal2 = randomFloat();
        sprintf(buffer, "%s %8.8g %8.8g\n", operation, floatVal1, floatVal2);
    } else {
        // Integer operation
        intVal1 = randomInt();
        intVal2 = randomInt();
        sprintf(buffer, "%s %d %d\n", operation, intVal1, intVal2);
    }

    // Send the operation and operands to the client
    send(info->sockfd, buffer, strlen(buffer), 0);
    printf("Client[%d] operation: %s", info->clientID, buffer);

    // Clear the buffer to receive the client's result
    memset(buffer, 0, sizeof(buffer));

    // Set the timeout for receiving the result
    timeout.tv_sec = TIME_OUT;
    setsockopt(info->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Receive the result from the client
    int bytesReceived = recv(info->sockfd, &buffer, MAX_BUF_SIZE, 0);
    if (bytesReceived <= 0) {
        // If no data is received, it's a timeout or error
        printf("Client[%d] timeout or error receiving result.\n", info->clientID);
        send(info->sockfd, "ERROR TO\n", 9, 0);
        close(info->sockfd);
        // Decrement the number of active clients
        pthread_mutex_lock(&clientLock);
        activeClients--;
        pthread_mutex_unlock(&clientLock);
        free(info);
        return NULL;
    }

    // Validate the client's result
    if (operation[0] == 'f') {
        // Floating-point result
        sscanf(buffer, "%lf", &floatRes);
        if (validateFloatResult(operation, floatVal1, floatVal2, floatRes)) {
            // If correct, send "OK\n"
            send(info->sockfd, "OK\n", 3, 0);
            printf("Client[%d] result: Correct.\n", info->clientID);
        } else {
            // If incorrect, send "ERROR\n"
            send(info->sockfd, "ERROR\n", 6, 0);
            printf("Client[%d] result: Incorrect.\n", info->clientID);
        }
    } else {
        // Integer result
        sscanf(buffer, "%d", &intRes);
        if (validateIntResult(operation, intVal1, intVal2, intRes)) {
            // If correct, send "OK\n"
            send(info->sockfd, "OK\n", 3, 0);
            printf("Client[%d] result: Correct.\n", info->clientID);
        } else {
            // If incorrect, send "ERROR\n"
            send(info->sockfd, "ERROR\n", 6, 0);
            printf("Client[%d] result: Incorrect.\n", info->clientID);
        }
    }

    // Close the client's socket and clean up
    close(info->sockfd);
    pthread_mutex_lock(&clientLock);
    activeClients--;
    pthread_mutex_unlock(&clientLock);
    free(info);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        // Ensure the program is called with the correct arguments
        fprintf(stderr, "usage: %s hostname:port\n", argv[0]);
        exit(1);
    }

    // Initialize the calculation library (assumed to be provided)
    initCalcLib();

    // Extract the hostname and port from the command-line argument
    char *hostname = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");

    int serverSock;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage clientAddr;  // Client's address information
    socklen_t addrSize;
    char clientIP[INET6_ADDRSTRLEN];     // Buffer for client's IP address
    int yes = 1;                         // For setsockopt()

    // Prepare the hints structure for getaddrinfo()
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;        // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;        // Use my IP

    // Get server address information
    if (getaddrinfo(hostname, port, &hints, &servinfo) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    // Loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        // Create a socket
        if ((serverSock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        // Set socket options to allow reuse of address
        if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        // Bind the socket to the address
        if (bind(serverSock, p->ai_addr, p->ai_addrlen) == -1) {
            close(serverSock);
            perror("server: bind");
            continue;
        }

        // Successfully bound
        break;
    }

    // Free the address information
    freeaddrinfo(servinfo);

    if (p == NULL) {
        // If we couldn't bind to any address
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // Start listening for incoming connections
    if (listen(serverSock, MAX_CLIENTS) == -1) {
        perror("listen");
        exit(1);
    }

    fd_set read_fds;
    int maxFd;
    struct timeval timeout;

    // Main loop to accept and handle clients
    while (1) {
        // Clear the set and add the server socket
        FD_ZERO(&read_fds);
        FD_SET(serverSock, &read_fds);
        maxFd = serverSock;

        // Set the timeout for select()
        timeout.tv_sec = TIME_OUT;
        timeout.tv_usec = 0;

        // Use select() to wait for incoming connections
        int activity = select(maxFd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity <= 0) {
            // If select() times out or encounters an error, continue
            continue;
        }

        if (FD_ISSET(serverSock, &read_fds)) {
            // Accept a new connection
            addrSize = sizeof clientAddr;
            int newSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrSize);
            if (newSock == -1) {
                perror("accept");
                continue;
            }

            // Lock the mutex to modify shared variables
            pthread_mutex_lock(&clientLock);
            if (activeClients < MAX_CLIENTS) {
                // Increment active client counts
                activeClients++;
                clientCounter++;
                // Get the client's IP address
                inet_ntop(clientAddr.ss_family, get_in_addr((struct sockaddr *)&clientAddr), clientIP, sizeof clientIP);
                printf("Client[%d] from [%s] connected.\n", clientCounter, clientIP);

                // Send the initial protocol message to the client
                send(newSock, PROTOCOL_MSG, strlen(PROTOCOL_MSG), 0);

                // Allocate memory for client information
                ClientInfo* clientInfo = (ClientInfo*)malloc(sizeof(ClientInfo));
                clientInfo->sockfd = newSock;
                clientInfo->clientID = clientCounter;

                // Create a new thread to handle the client
                pthread_t clientThread;
                pthread_create(&clientThread, NULL, clientHandler, clientInfo);
                pthread_detach(clientThread);  // Detach the thread to allow independent execution
            } else {
                // If the server is busy, reject the connection
                send(newSock, "ERROR: Server busy, rejecting connection.\n", 42, 0);
                close(newSock);
                printf("Server busy, rejected connection.\n");
            }
            // Unlock the mutex after modifying shared variables
            pthread_mutex_unlock(&clientLock);
        }
    }

    // Clean up the mutex before exiting (not reachable in this code)
    pthread_mutex_destroy(&clientLock);
    return 0;
}
