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
#include <sys/select.h>
#include <sys/time.h>
#include <math.h>
#include "calcLib.h"

#define MAX_CLIENTS 5          // Maximum number of clients in the queue
#define TIME_OUT 5             // Timeout in seconds for client responses
#define PROTOCOL_MSG "TEXT TCP 1.0\n\n"  // Initial protocol message
#define MAX_BUF_SIZE 1024      // Maximum buffer size for messages

// Function to get the IPv4 or IPv6 address from a sockaddr structure
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Function to validate the result of a floating-point operation
bool validateFloatResult(const char* operation, double op1, double op2, double result) {
    double calculatedResult;
    if (strcmp(operation, "fadd") == 0) calculatedResult = op1 + op2;
    else if (strcmp(operation, "fsub") == 0) calculatedResult = op1 - op2;
    else if (strcmp(operation, "fmul") == 0) calculatedResult = op1 * op2;
    else if (strcmp(operation, "fdiv") == 0) calculatedResult = op1 / op2;

    return fabs(calculatedResult - result) < 0.0001;
}

// Function to validate the result of an integer operation
bool validateIntResult(const char* operation, int op1, int op2, int result) {
    int calculatedResult;
    if (strcmp(operation, "add") == 0) calculatedResult = op1 + op2;
    else if (strcmp(operation, "sub") == 0) calculatedResult = op1 - op2;
    else if (strcmp(operation, "mul") == 0) calculatedResult = op1 * op2;
    else if (strcmp(operation, "div") == 0) calculatedResult = op1 / op2;

    return calculatedResult == result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s hostname:port\n", argv[0]);
        exit(1);
    }

    initCalcLib();

    // Extract hostname and port from the command line argument
    char *hostname = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");

    int serverSock;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage clientAddr;
    socklen_t addrSize;
    char clientIP[INET6_ADDRSTRLEN];
    int yes = 1;
    
    fd_set read_fds;
    struct timeval timeout;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(hostname, port, &hints, &servinfo) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((serverSock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(serverSock, p->ai_addr, p->ai_addrlen) == -1) {
            close(serverSock);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(serverSock, MAX_CLIENTS) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Server is waiting for connections...\n");

    while (1) {
        addrSize = sizeof clientAddr;
        int clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrSize);
        if (clientSock == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(clientAddr.ss_family, get_in_addr((struct sockaddr *)&clientAddr), clientIP, sizeof clientIP);
        printf("Connected to client at %s\n", clientIP);

        // Send protocol message to client
        send(clientSock, PROTOCOL_MSG, strlen(PROTOCOL_MSG), 0);

        // Set up timeout for client response
        FD_ZERO(&read_fds);
        FD_SET(clientSock, &read_fds);
        timeout.tv_sec = TIME_OUT;
        timeout.tv_usec = 0;

        // Wait for "OK\n" from client
        int activity = select(clientSock + 1, &read_fds, NULL, NULL, &timeout);
        if (activity <= 0) {
            printf("Client timeout or error.\n");
            send(clientSock, "ERROR TO\n", 9, 0);
            close(clientSock);
            continue;
        }

        char buffer[MAX_BUF_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSock, buffer, MAX_BUF_SIZE - 1, 0);
        if (bytesReceived < 0 || strcmp(buffer, "OK\n") != 0) {
            printf("Invalid client response: %s\n", buffer);
            close(clientSock);
            continue;
        }

        // Generate random operation
        char* operation = randomType();
        double fv1, fv2, fresult;
        int iv1, iv2, iresult;
        memset(buffer, 0, sizeof(buffer));

        if (operation[0] == 'f') {
            // Floating-point operation
            fv1 = randomFloat();
            fv2 = randomFloat();
            sprintf(buffer, "%s %8.8g %8.8g\n", operation, fv1, fv2);
        } else {
            // Integer operation
            iv1 = randomInt();
            iv2 = randomInt();
            sprintf(buffer, "%s %d %d\n", operation, iv1, iv2);
        }

        // Send the operation and operands to the client
        send(clientSock, buffer, strlen(buffer), 0);
        printf("Sent to client: %s", buffer);

        // Receive client's result
        memset(buffer, 0, sizeof(buffer));
        activity = select(clientSock + 1, &read_fds, NULL, NULL, &timeout);
        if (activity <= 0) {
            printf("Client timeout or error receiving result.\n");
            send(clientSock, "ERROR TO\n", 9, 0);
            close(clientSock);
            continue;
        }

        bytesReceived = recv(clientSock, buffer, MAX_BUF_SIZE - 1, 0);
        if (bytesReceived <= 0) {
            printf("Client disconnected or error.\n");
            close(clientSock);
            continue;
        }

        // Validate the client's result
        if (operation[0] == 'f') {
            sscanf(buffer, "%lf", &fresult);
            if (validateFloatResult(operation, fv1, fv2, fresult)) {
                send(clientSock, "OK\n", 3, 0);
                printf("Correct result from client.\n");
            } else {
                send(clientSock, "ERROR\n", 6, 0);
                printf("Incorrect result from client.\n");
            }
        } else {
            sscanf(buffer, "%d", &iresult);
            if (validateIntResult(operation, iv1, iv2, iresult)) {
                send(clientSock, "OK\n", 3, 0);
                printf("Correct result from client.\n");
            } else {
                send(clientSock, "ERROR\n", 6, 0);
                printf("Incorrect result from client.\n");
            }
        }

        // Close the connection with the client
        close(clientSock);
    }

    close(serverSock);
    return 0;
}
