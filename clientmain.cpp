#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#define SA struct sockaddr

// Uncomment the following line to enable debugging output
//#define DEBUG

// Includes the support library for calculations
#include <calcLib.h>

// Receives a message from the server and stores it in the provided buffer
void receiveMessage(int &sock_desc, char* server_response, unsigned int buffer_size) {
    memset(server_response, 0, buffer_size);
    if(recv(sock_desc, server_response, buffer_size, 0) < 0) {
        #ifdef DEBUG
        printf("Error occurred while receiving the message\n");
        #endif
        exit(-1);
    } else {
        #ifdef DEBUG
        printf("Received: %s", server_response);
        #endif
    }
}

// Sends a message to the server
void sendMessage(int &sock_desc, const char* client_request) {
    if(send(sock_desc, client_request, strlen(client_request), 0) < 0) {
        #ifdef DEBUG
        printf("Error occurred while sending the message\n");
        #endif
        exit(-1);
    } else {
        #ifdef DEBUG
        printf("Sent: %s", client_request);
        #endif
    }
}

// Computes the result based on the server's request and sends it back
void computeAndSendResult(const char* server_response, int &sock_desc) {
    int int1, int2, int_result;
    double float1, float2, float_result;
    char operation[10];

    sscanf(server_response, "%s %lf %lf", operation, &float1, &float2);

    if(strcmp(operation, "fadd") == 0) {
        float_result = float1 + float2;
    } else if(strcmp(operation, "fsub") == 0) {
        float_result = float1 - float2;
    } else if(strcmp(operation, "fmul") == 0) {
        float_result = float1 * float2;
    } else if(strcmp(operation, "fdiv") == 0) {
        float_result = float1 / float2;
    } else {
        int1 = static_cast<int>(float1);
        int2 = static_cast<int>(float2);

        if(strcmp(operation, "add") == 0) {
            int_result = int1 + int2;
        } else if(strcmp(operation, "sub") == 0) {
            int_result = int1 - int2;
        } else if(strcmp(operation, "mul") == 0) {
            int_result = int1 * int2;
        } else if(strcmp(operation, "div") == 0) {
            int_result = int1 / int2;
        }
    }

    char response[50];
    if(operation[0] == 'f') {
        sprintf(response, "%8.8g\n", float_result);
    } else {
        sprintf(response, "%d\n", int_result);
    }

    sendMessage(sock_desc, response);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <host:port>\n", argv[0]);
        return -1;
    }

    char* host_port = strdup(argv[1]);
    char* colon = strrchr(host_port, ':');
    if (!colon) {
        printf("Incorrect format. Use <host:port>\n");
        free(host_port); // Changed part: Ensure to free allocated memory
        return -1;
    }

    *colon = '\0';
    char* hostname = host_port;
    int port_number = atoi(colon + 1);

    printf("Hostname: %s, Port: %d.\n", hostname, port_number);

    struct addrinfo hints, *server_info;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, colon + 1, &hints, &server_info) < 0) {
        printf("Error in getaddrinfo: %s\n", strerror(errno));
        free(host_port); // Changed part: Ensure to free allocated memory
        return -1;
    }

    int sock_desc = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (sock_desc < 0) {
        #ifdef DEBUG
        printf("Socket creation failed\n");
        #endif
        freeaddrinfo(server_info); // Changed part: Ensure to free allocated memory
        free(host_port); // Changed part: Ensure to free allocated memory
        return -1;
    }
    #ifdef DEBUG
    printf("Socket successfully created\n");
    #endif

    if (connect(sock_desc, server_info->ai_addr, server_info->ai_addrlen) < 0) {
        #ifdef DEBUG
        printf("Connection to server failed\n");
        #endif
        freeaddrinfo(server_info); // Changed part: Ensure to free allocated memory
        close(sock_desc); // Changed part: Close socket if connection fails
        free(host_port); // Changed part: Ensure to free allocated memory
        return -1;
    }
    #ifdef DEBUG
    printf("Successfully connected to server\n");
    #endif

    freeaddrinfo(server_info);
    free(host_port); // Changed part: Ensure to free allocated memory

    char server_response[2000];
    receiveMessage(sock_desc, server_response, sizeof(server_response));

    char* supported_protocols[10];
    int protocol_count = 0;
    char* line = strtok(server_response, "\n");
    while (line != NULL) {
        if (strcmp(line, "") == 0) break;
        supported_protocols[protocol_count++] = line;
        line = strtok(NULL, "\n");
    }

    int protocol_supported = 0;
    for (int i = 0; i < protocol_count; i++) {
        if (strcmp(supported_protocols[i], "TEXT TCP 1.0") == 0) {
            protocol_supported = 1;
            break;
        }
    }

    if (protocol_supported) {
        sendMessage(sock_desc, "OK\n");
    } else {
        close(sock_desc);
        return -1;
    }

    receiveMessage(sock_desc, server_response, sizeof(server_response));

    computeAndSendResult(server_response, sock_desc);

    receiveMessage(sock_desc, server_response, sizeof(server_response));
    printf("%s", server_response);

    close(sock_desc);
    return 0;
}
