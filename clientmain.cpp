#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#define SA struct sockaddr
/* You will to add includes here */

// Enable if you want debugging to be printed, see example below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
//#define DEBUG


// Included to get the support library
#include <calcLib.h>

// Function to receive a message from the server
void receiveMessage(int &socket_desc, char* server_message, unsigned int msg_size){
  memset(server_message, 0, msg_size);
  if(recv(socket_desc, server_message, msg_size, 0) < 0){
    #ifdef DEBUG
    printf("Error receiving message\n");
    #endif
    exit(-1);
  } else {
    #ifdef DEBUG
    printf("Received: %s", server_message);
    #endif
  }
}

// Function to send a message to the server
void sendMessage(int &socket_desc, const char* client_message){
  if(send(socket_desc, client_message, strlen(client_message), 0) < 0){
    #ifdef DEBUG
    printf("Unable to send message\n");
    #endif
    exit(-1);
  } else {
    #ifdef DEBUG
    printf("Sent: %s", client_message);
    #endif
  }
}

// Function to calculate and send the result back to the server
void calculateMessage(char* server_message, int &socket_desc){
  int i1, i2, iresult;
  double f1, f2, fresult;
  char operation[10];

  sscanf(server_message, "%s %lf %lf", operation, &f1, &f2);

  if(strcmp(operation, "fadd") == 0){
    fresult = f1 + f2;
  } else if(strcmp(operation, "fsub") == 0){
    fresult = f1 - f2;
  } else if(strcmp(operation, "fmul") == 0){
    fresult = f1 * f2;
  } else if(strcmp(operation, "fdiv") == 0){
    fresult = f1 / f2;
  } else {
    i1 = (int)f1;
    i2 = (int)f2;

    if(strcmp(operation, "add") == 0){
      iresult = i1 + i2;
    } else if(strcmp(operation, "sub") == 0){
      iresult = i1 - i2;
    } else if(strcmp(operation, "mul") == 0){
      iresult = i1 * i2;
    } else if(strcmp(operation, "div") == 0){
      iresult = i1 / i2;
    }
  }

  char response[50];
  if(server_message[0] == 'f'){
    sprintf(response, "%8.8g\n", fresult);
  } else {
    sprintf(response, "%d\n", iresult);
  }

  sendMessage(socket_desc, response);
}

int main(int argc, char *argv[]){
  if (argc != 2) {
    printf("Usage: %s <host:port>\n", argv[0]);
    return -1;
  }

  // Parse host and port from argv[1]
  char* host_port = strdup(argv[1]);
  char* colon = strrchr(host_port, ':');
  if (!colon) {
    printf("Invalid format. Use <host:port>\n");
    return -1;
  }

  *colon = '\0';
  char* host = host_port;
  int port = atoi(colon + 1);

  printf("Host %s, and port %d.\n", host, port);

  // Get address info
  struct addrinfo hints, *serverinfo;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host, colon + 1, &hints, &serverinfo) < 0) {
    printf("Getaddrinfo error: %s\n", strerror(errno));
    return -1;
  }

  // Create socket
  int socket_desc = socket(serverinfo->ai_family, serverinfo->ai_socktype, serverinfo->ai_protocol);
  if (socket_desc < 0) {
    #ifdef DEBUG
    printf("Unable to create socket\n");
    #endif
    return -1;
  }
  #ifdef DEBUG
  printf("Socket created\n");
  #endif

  // Connect to server
  if (connect(socket_desc, serverinfo->ai_addr, serverinfo->ai_addrlen) < 0) {
    #ifdef DEBUG
    printf("Unable to connect\n");
    #endif
    return -1;
  }
  #ifdef DEBUG
  printf("Connected\n");
  #endif

  // Free address info
  freeaddrinfo(serverinfo);

  // Receive protocol versions
  char server_message[2000];
  receiveMessage(socket_desc, server_message, sizeof(server_message));

  // Check supported protocols
  char* supported_protocols[10];
  int protocol_count = 0;
  char* line = strtok(server_message, "\n");
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
    sendMessage(socket_desc, "OK\n");
  } else {
    close(socket_desc);
    return -1;
  }

  // Receive the assignment
  receiveMessage(socket_desc, server_message, sizeof(server_message));

  // Calculate and send the result
  calculateMessage(server_message, socket_desc);

  // Receive and print the final response
  receiveMessage(socket_desc, server_message, sizeof(server_message));
  printf("%s", server_message);

  // Close socket and exit
  close(socket_desc);
  return 0;
}
