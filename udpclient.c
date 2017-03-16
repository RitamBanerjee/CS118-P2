#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>

#define fileBufferLength 10000  //Increment by this amount everytime it overflows

int main(int argc, char *argv[]){
  int clientSocket, portNum, nBytes;
   int bufferMultiplier = 1;   //indicates size of file buffer
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
      strcpy(buffer,"SYN\n\n");
      nBytes = strlen(buffer)+1;
      sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
      printf("Sending Packet %s",buffer);
      nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);
      char* line = strtok(buffer,"\n");
      if(strcmp(line,"SYNACK")==0){
        // printf("Receiving Packet %s \n",line);
        printf("Receiving Packet %s", buffer);
        strcpy(buffer,"REQUEST\n");
        strcat(buffer,fileName);
        strcat(buffer,"\n");
        printf("Sending Packet Request %s",fileName);
        nBytes = strlen(buffer);
        sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
        handShook=1;
      }
    }
    if(handShook){  //handles receiving packets, sending acks, and terminating on FIN
      nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);
      printf("\n\nBuffer is:\n%s\n\n", buffer);
      char* newBuffer = malloc(strlen(buffer));
      strcpy(newBuffer, buffer);
      char * line = strtok(newBuffer,"\n");
      nBytes -= (strlen(line)+1);
      if(strcmp(line,"FIN")==0){
        char* finMessage = strtok(NULL,"\n");
        printf("Receiving Packet FIN %s\n",finMessage);  //change according to spec
        break;
      }
      else if(strcmp(line,"Sequence")==0){
        // printf("Recieving sequence...\n");
        char* sequenceNumString = strtok(NULL,"\n");

        // substracting the initial blocks from nbytes 
        nBytes -= (strlen(sequenceNumString)+1);
        int sequenceNum = atoi(sequenceNumString);
        char* data = strtok(NULL,"\n");
        nBytes -= (strlen(data)+1);
        // printf("\n\n%d\n\n", nBytes);
        data = strtok(NULL,"\0");
        if(sequenceNum>(bufferMultiplier*fileBufferLength)){  //check for file buffer overflow
          bufferMultiplier++;
          fileBuffer = realloc(fileBuffer,bufferMultiplier*fileBufferLength);
          printf("Buffer has been allocated to %i", bufferMultiplier*fileBufferLength);
        }
        // printf("Contents: %s\n",data);  //replace with "Received SeqNo"
        // getchar(); // pause
        //save data to buffer
        if(sequenceNum==0)
          strcpy(fileBuffer,data);
        else {
          char dataSize[nBytes+1];
          strncpy(dataSize, data, nBytes);
          // printf("\n\ndata is: %s\n----------\n", data);
          dataSize[nBytes] = '\0';
          printf("\n\ndataSize is: %s\n--------------------\n", dataSize);
          strcat(fileBuffer,dataSize);
          // printf("\n\nfileBuffer is %s\n------------\n", fileBuffer);
        }
          
        strcpy(buffer,"ACK\n");
        strcat(buffer,sequenceNumString);  //ack packet that was received and stored
        nBytes = strlen(buffer);
        sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
      }

    }
    /*Send message to server*/
    //sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);

    /*Receive message from server*/
    //nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);

    

  }
  printf("Filebuf:\n%s",fileBuffer);  //check file has transfered correctly
  return 0;
}