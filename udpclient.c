#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>

#define fileBufferLength 120

int main(int argc, char *argv[]){
  int clientSocket, portNum, nBytes;
  char *buffer = malloc(1024);
  char *fileBuffer = malloc(fileBufferLength);
  char fileName[500];
  struct sockaddr_in serverAddr;
  socklen_t addr_size;
  if (argc < 2) {
         fprintf(stderr,"ERROR: no port provided\n");
         exit(1);
  }
  if (argc < 3) {
         fprintf(stderr,"ERROR: no file provided\n");
         exit(1);
  }

  clientSocket = socket(PF_INET, SOCK_DGRAM, 0);
  strcpy(fileName,argv[2]);
  portNum = atoi(argv[1]);
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(portNum);
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  

  /*Initialize size variable to be used later on*/
  addr_size = sizeof serverAddr;
  int handShook = 0; 
  strcpy(buffer,"SYN:"); //initiate handshake
  while(1){
    // printf("Type a sentence to send to server:\n");
    // fgets(buffer,1024,stdin );
    // printf("You typed: %s",buffer);
    nBytes = strlen(buffer) + 1;
    if(!handShook){   //perform three way handshake if not already done
      strcpy(buffer,"SYN:");
      nBytes = strlen(buffer)+1;
      sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
      printf("Sending Packet %s \n",buffer);
      nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);
      char* line = strtok(buffer,":");
      if(strcmp(line,"SYNACK")==0){
        printf("Receiving Packet %s \n", buffer);
        strcpy(buffer,"REQUEST:");
        strcat(buffer,fileName);
        strcat(buffer,":\n");
        printf("Sending Packet Request %s \n",fileName);
        nBytes = strlen(buffer);
        sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
        handShook=1;
      }
    }
    if(handShook){
      nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);
      char * line = strtok(buffer,":");
      if(strcmp(line,"FYN")==0){
        char* fynMessage = strtok(NULL,":");
        printf("Receiving Packet FYN %s \n",fynMessage);
        break;
      }
      else if(strcmp(line,"Sequence")==0){
        int bufferMultiplier = 1; 
        char* sequenceNumString = strtok(NULL,":");
        int sequenceNum = atoi(sequenceNumString);
        char* data = strtok(NULL,":");
        data = strtok(NULL,":");
        if(sequenceNum>(bufferMultiplier*fileBufferLength)){
          printf("reallocing\n");
          fileBuffer = realloc(fileBuffer,bufferMultiplier*fileBufferLength);
        }
        // printf("Contents: %s\n",data);  //replace with "Received SeqNo"
        if(sequenceNum==0)
          strcpy(fileBuffer,data);
        else
          strcat(fileBuffer,data);
        strcpy(buffer,"ACK:");
        strcat(buffer,sequenceNumString);
        nBytes = strlen(buffer);
        sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
      }

    }
    /*Send message to server*/
    //sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);

    /*Receive message from server*/
    //nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);

    

  }
  printf("Filebuf:%s\n",fileBuffer);
  return 0;
}
