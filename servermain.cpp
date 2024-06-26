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
#include <pthread.h>
#include <math.h>
#include <sys/select.h>
#include <sys/time.h>
#include "calcLib.h"

#define MAX_CLIENTS 5
#define TIME_OUT 5
#define PROTOCOL_MSG "TEXT TCP 1.0\n"
#define MAX_BUF_SIZE 1024

static int activeClients = 0;
static int clientCounter = 0;
pthread_mutex_t clientLock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int sockfd;
    int clientID;
} ClientInfo;

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

bool validateFloatResult(const char* operation, double op1, double op2, double result) {
    double calculatedResult;
    if (strcmp(operation, "fadd") == 0) calculatedResult = op1 + op2;
    else if (strcmp(operation, "fsub") == 0) calculatedResult = op1 - op2;
    else if (strcmp(operation, "fmul") == 0) calculatedResult = op1 * op2;
    else if (strcmp(operation, "fdiv") == 0) calculatedResult = op1 / op2;

    return fabs(calculatedResult - result) < 0.0001;
}

bool validateIntResult(const char* operation, int op1, int op2, int result) {
    int calculatedResult;
    if (strcmp(operation, "add") == 0) calculatedResult = op1 + op2;
    else if (strcmp(operation, "sub") == 0) calculatedResult = op1 - op2;
    else if (strcmp(operation, "mul") == 0) calculatedResult = op1 * op2;
    else if (strcmp(operation, "div") == 0) calculatedResult = op1 / op2;

    return calculatedResult == result;
}

void* clientHandler(void* clientData) {
    ClientInfo* info = (ClientInfo*) clientData;
    char buffer[MAX_BUF_SIZE];
    double floatVal1, floatVal2, floatRes;
    int intVal1, intVal2, intRes;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(info->sockfd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = TIME_OUT;
    timeout.tv_usec = 0;

    int activity = select(info->sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (activity <= 0) {
        printf("Client[%d] timeout or error.\n", info->clientID);
        send(info->sockfd, "ERROR TO\n", 9, 0);
        close(info->sockfd);
        pthread_mutex_lock(&clientLock);
        activeClients--;
        pthread_mutex_unlock(&clientLock);
        free(info);
        return NULL;
    }

    recv(info->sockfd, &buffer, MAX_BUF_SIZE, 0);
    if (strcmp(buffer, "OK\n") != 0) {
        printf("Client[%d] sent an invalid message.\n", info->clientID);
        close(info->sockfd);
        pthread_mutex_lock(&clientLock);
        activeClients--;
        pthread_mutex_unlock(&clientLock);
        free(info);
        return NULL;
    }

    char* operation = randomType();
    if (operation[0] == 'f') {
        floatVal1 = randomFloat();
        floatVal2 = randomFloat();
        sprintf(buffer, "%s %8.8g %8.8g\n", operation, floatVal1, floatVal2);
    } else {
        intVal1 = randomInt();
        intVal2 = randomInt();
        sprintf(buffer, "%s %d %d\n", operation, intVal1, intVal2);
    }

    send(info->sockfd, buffer, strlen(buffer), 0);
    printf("Client[%d] operation: %s", info->clientID, buffer);
    memset(buffer, 0, sizeof(buffer));

    timeout.tv_sec = TIME_OUT;
    setsockopt(info->sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int bytesReceived = recv(info->sockfd, &buffer, MAX_BUF_SIZE, 0);
    if (bytesReceived <= 0) {
        printf("Client[%d] timeout or error receiving result.\n", info->clientID);
        send(info->sockfd, "ERROR TO\n", 9, 0);
        close(info->sockfd);
        pthread_mutex_lock(&clientLock);
        activeClients--;
        pthread_mutex_unlock(&clientLock);
        free(info);
        return NULL;
    }

    if (operation[0] == 'f') {
        sscanf(buffer, "%lf", &floatRes);
        if (validateFloatResult(operation, floatVal1, floatVal2, floatRes)) {
            send(info->sockfd, "OK\n", 3, 0);
            printf("Client[%d] result: Correct.\n", info->clientID);
        } else {
            send(info->sockfd, "ERROR\n", 6, 0);
            printf("Client[%d] result: Incorrect.\n", info->clientID);
        }
    } else {
        sscanf(buffer, "%d", &intRes);
        if (validateIntResult(operation, intVal1, intVal2, intRes)) {
            send(info->sockfd, "OK\n", 3, 0);
            printf("Client[%d] result: Correct.\n", info->clientID);
        } else {
            send(info->sockfd, "ERROR\n", 6, 0);
            printf("Client[%d] result: Incorrect.\n", info->clientID);
        }
    }

    close(info->sockfd);
    pthread_mutex_lock(&clientLock);
    activeClients--;
    pthread_mutex_unlock(&clientLock);
    free(info);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s hostname:port\n", argv[0]);
        exit(1);
    }

    initCalcLib();
    
    char *hostname = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");

    int serverSock;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage clientAddr;
    socklen_t addrSize;
    char clientIP[INET6_ADDRSTRLEN];
    int yes = 1;

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

    fd_set read_fds;
    int maxFd;
    struct timeval timeout;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(serverSock, &read_fds);
        maxFd = serverSock;

        timeout.tv_sec = TIME_OUT;
        timeout.tv_usec = 0;

        int activity = select(maxFd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity <= 0) {
            continue;
        }

        if (FD_ISSET(serverSock, &read_fds)) {
            addrSize = sizeof clientAddr;
            int newSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrSize);
            if (newSock == -1) {
                perror("accept");
                continue;
            }

            pthread_mutex_lock(&clientLock);
            if (activeClients < MAX_CLIENTS) {
                activeClients++;
                clientCounter++;
                inet_ntop(clientAddr.ss_family, get_in_addr((struct sockaddr *)&clientAddr), clientIP, sizeof clientIP);
                printf("Client[%d] from [%s] connected.\n", clientCounter, clientIP);

                send(newSock, PROTOCOL_MSG, strlen(PROTOCOL_MSG), 0);

                ClientInfo* clientInfo = (ClientInfo*)malloc(sizeof(ClientInfo));
                clientInfo->sockfd = newSock;
                clientInfo->clientID = clientCounter;

                pthread_t clientThread;
                pthread_create(&clientThread, NULL, clientHandler, clientInfo);
                pthread_detach(clientThread);
            } else {
                send(newSock, "ERROR: Server busy, rejecting connection.\n", 42, 0);
                close(newSock);
                printf("Server busy, rejected connection.\n");
            }
            pthread_mutex_unlock(&clientLock);
        }
    }

    pthread_mutex_destroy(&clientLock);
    return 0;
}
