#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]){
  int udpSocket, nBytes, portno;
  char buffer[1024];
  struct sockaddr_in serverAddr, clientAddr;
  struct sockaddr_storage serverStorage;
  socklen_t addr_size, client_addr_size;
  int i;
  if (argc < 2) {
    fprintf(stderr,"ERROR: no port provided\n");
    exit(1);
  }
  /*Create UDP socket*/
  udpSocket = socket(PF_INET, SOCK_DGRAM, 0);
  if (udpSocket < 0)
    error("ERROR: opening socket");
  else
    printf("Socket Opened \n");
  portno = atoi(argv[1]);
  /*Configure address struct*/
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(portno);
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  

  /*Bind socket with address struct*/
  if (bind(udpSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
        error("ERROR on binding");
  addr_size = sizeof serverStorage;

  while(1){
    //receive incoming data
    nBytes = recvfrom(udpSocket,buffer,1024,0,(struct sockaddr *)&serverStorage, &addr_size);
    printf("filename:%s\n",buffer);
    char* line = strtok(buffer, "\n");
    if(strcmp(line,"SYN")==0){
        strcpy(buffer,"SYNACK\0");
        nBytes = strlen(buffer);
    }
    if(strcmp(line,"REQUEST")==0){
        char* fileName = strtok(line, ":");
        printf("filename:%s\n",fileName);
        //strcpy(buffer,"RECEIVED\0");
    }

    //send message back, address is serverStorage
    sendto(udpSocket,buffer,nBytes,0,(struct sockaddr *)&serverStorage,addr_size);
  }

  return 0;
}
