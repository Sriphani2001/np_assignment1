#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "calcLib.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <iostream>
//#define DEBUG

using namespace std;

int main(int argc, char *argv[])
{
	char delim[] = ":";
	char *Desthost = strtok(argv[1], delim);
	char *Destport = strtok(NULL, delim);
	//**Read ip addresses
	int port = atoi(Destport);
	int rc, soc, valread, client_fd;
	char buffer[2000];

	struct in6_addr serveraddr;
	struct addrinfo hints, *res = NULL;

	memset(&hints, 0x00, sizeof(hints));
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* Check if we were provided the address of the server using
	 inet_pton() to convert the text form of the address to binary    */

	rc = inet_pton(AF_INET, Desthost, &serveraddr);
	if (rc == 1) /* valid IPv4 text address? */
	{
		hints.ai_family = AF_INET;
		hints.ai_flags |= AI_NUMERICHOST;
	}
	else
	{
		rc = inet_pton(AF_INET6, Desthost, &serveraddr);
		if (rc == 1) /* valid IPv6 text address? */
		{

			hints.ai_family = AF_INET6;
			hints.ai_flags |= AI_NUMERICHOST;
		}
	}

	// Get the address information for the server using getaddrinfo().

	rc = getaddrinfo(Desthost, Destport, &hints, &res);
	if (rc != 0)
	{
		printf("Host not found --> %s\n", gai_strerror(rc));
		if (rc == EAI_SYSTEM)
			perror("getaddrinfo() failed");
		return -1;
	}

	// *Create socket:*
	soc = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (soc < 0)
	{
		perror("socket() failed");
		return -1;
	}

	// Use the connect() function to establish a connection to the server.

	client_fd = connect(soc, res->ai_addr, res->ai_addrlen);
	if (client_fd < 0)
	{
		perror("connect() failed");
		return -1;
	}
	// Get my ip address and port
	char myIP[16];
	unsigned int myPort;
	struct sockaddr_in my_addr;
	bzero(&my_addr, sizeof(my_addr));
	socklen_t len = sizeof(my_addr);
	getsockname(soc, (struct sockaddr *)&my_addr, &len);
	inet_ntop(AF_INET, &my_addr.sin_addr, myIP, sizeof(myIP));
	myPort = ntohs(my_addr.sin_port);
#ifdef DEBUG
	printf("Local ip address: %s\n", myIP);
	printf("Local port : %u\n", myPort);
#endif
	printf("Host %s and port %d.\n\n", Desthost, port);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++
	//  info transfer between server and client

double f1,f2,fresult;
  int i1,i2,iresult;
  char *ptr;
  char *lineBuffer=NULL;
  char *linBuf=NULL;
  char *lineBuf;
  size_t lenBuffer=0;
  ssize_t nread=0;    

char buf[4096];
string userInput;
int tempt=0;

 do{
    memset(buf, 0, sizeof(buf));

    //bytes Recived will recive data from server based on tempt condition
    int bytesR = recv(soc, buf, 4096, 0);
    //cout << "> " << string(buf, bytesReceived) << "\r\n";
    if (bytesR == -1)
    {
      cout << "message is not comming from server!!\r\n";
      return 1;
    }

    // the fist condition recives  MESSAGE TCP_1.0 and replies with message ok\n which procides the condition to assignmnt conditon

    else if(tempt==0)
    {
      tempt++;
    string(buf, bytesR);
          
    #ifdef DEBUG 
    cout << "> " << string(buf, bytesR) << "\r\n";
    cout<< "OK\n";

    #endif
    string k = "OK\n";
    int sendres = send(soc, k.c_str(), k.length(), 0);
   // cout<< k.length();

   //int sendres = send(soc,"ok\n", 4, 0);
  }
  //
  
  else if(tempt==1)
  {
  tempt=tempt+1;
  string r ="";
  cout << "\nAssignment from Server>"<< string(buf, bytesR) << "\r";
  std::string b = string(buf, bytesR);
  char *lineBuffer=  strdup(b.c_str());

  b= nread;
  if (nread == -1 ) {
    printf("getline failed.\n");
    exit(1);
    }

  int rv;
  char command[10];
  rv=0;
  rv=sscanf(lineBuffer,"%s",command);
  if (rv == EOF ) {
    printf("Sscanf failed.\n");
    free(lineBuffer); 
      exit(1);
    }
    
  if(command[0]=='f'){
      //printf("Float\t");
   rv=sscanf(lineBuffer,"%s %lg %lg",command,&f1,&f2);
    if (rv == EOF ) 
      {
        printf("Sscanf failed.\n");
        free(lineBuffer); 
        exit(1);
      }


      if(strcmp(command,"fadd")==0){
      fresult=f1+f2;
    } else if (strcmp(command, "fsub")==0){
      fresult=f1-f2;
    } else if (strcmp(command, "fmul")==0){
      fresult=f1*f2;
    } else if (strcmp(command, "fdiv")==0){
      fresult=f1/f2;
    }
    //printf("my result:%8.8g\n",fresult);
    r = to_string(fresult);
    #ifdef DEBUG 
     cout<<"obtained float result :" << fresult <<"\n";
    #endif

    
  } 
  else 
  {
    //printf("Int\t");
    rv=sscanf(lineBuffer,"%s %d %d",command,&i1,&i2);
    if (rv == EOF ) {
      printf("Sscanf failed.\n");
      free(lineBuffer); 
      // This is needed for the getline() as it will allocate memory (if the provided buffer is NUL).
      exit(1);
    }
    if(strcmp(command,"add")==0){
      iresult=i1+i2;
    } else if (strcmp(command, "sub")==0){
      iresult=i1-i2;
    } else if (strcmp(command, "mul")==0){
      iresult=i1*i2;
    } else if (strcmp(command, "div")==0){     
      iresult=i1/i2;
    } 
    //printf("my result: %d \n",iresult);
    r = to_string(iresult);
    #ifdef DEBUG 
      cout<<"obtained inter result :" << iresult<<"\n";
    #endif
    
    }
    string result2 = r;
    r += '\n';

    int sendres = send(soc,r.c_str(), r.length(), 0);
    if (sendres <0){
      cout<<"error in sending";
     }
    cout << "Result sent to server: " << r;

  free(lineBuffer);

  }
  
  else
  {
  
    cout << "> " << string(buf, bytesR) << "\r\n";
    
    break;

  }
  
  } while(true);
  return 0;

	// *Close the socket:*
	close(soc);
}
