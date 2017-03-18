#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>  
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define STATUS_OK "200 OK"
#define STATUS_NOT_FOUND "404 Not Found"

//Global Vars
int fileSize;
int packetSize = 1024;
int windowsize = 1024*5; 


//Function Headers
char* getFile(char*);
void handleTransmission(int,struct sockaddr*, socklen_t*,char*);
int sendPacket(int,struct sockaddr*, socklen_t*,char*,int);
int receiveACK(int,struct sockaddr*, socklen_t*,char*,int);
int createPacket(char **,char*,int);

typedef struct Frame Frame;

typedef struct Packet Packet; 
struct Packet{
  char* buffer;
  int sequenceNum;
  int received; 
  //int bufferSize; 
};

//typedef struct Packet* Packet; 
struct Packet* packetArray[1000]; //array of pointers to packet structs
int numPackets = 0;

struct Packet* newPacket(char* data, int sequenceNum, int dataSize){
  struct Packet* packet = malloc(sizeof (Packet));

  // setting buffer to be data
  char* bufferCopy = malloc(1024);
  memcpy(bufferCopy, data, dataSize);
  packet->buffer = bufferCopy;
  packet->sequenceNum = sequenceNum;
  packet->received = 0;
  //packet->bufferSize = dataSize;

  return packet;
}
void deletePacket(Packet* p){
  if(p!=NULL){
   // printf("inside deletepacket");
    free(p->buffer);
    printf("Deleting packet w/ seq. no: %d\n",p->sequenceNum);
    free(p);
   // printf("leaving deletepacket");
  }
}

void printPacket(Packet* p) {
  printf("Seq no: %d\n",p->sequenceNum);
}

int deleteACKedPacket(int ACKno){
  for(int i = 0; i<numPackets;i++){
    // printf("%d,", i);
    if(packetArray[i]!=NULL){
      printPacket(packetArray[i]);
      printf("ACKNO:%d\n",ACKno);
      if(packetArray[i]->sequenceNum==ACKno){
        //printf("packet seq == ackno");
        Packet* oldPacket = packetArray[i];
        memmove(&packetArray, &packetArray[1], sizeof(packetArray) - sizeof(oldPacket));
        deletePacket(oldPacket);
        numPackets--;
        
        // returns next ackno expected
        if (packetArray[i] != NULL) {
          return packetArray[i]->sequenceNum;
        }
        else {
          return -1;
        }
      }
    }
  }
  return -1; 
}

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
  char* file = NULL;
  int synReceived = 0; 
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
    //printf("Received %d bytes\n", nBytes);
    char* line = strtok(buffer, "\n");
    if(strcmp(line,"SYN")==0){ //start of three way handshake
        printf("Receiving Packet %s \n",line);
        strcpy(buffer,"SYNACK\0");
        nBytes = strlen(buffer);
        synReceived = 1;
    }
    if(strcmp(line,"REQUEST")==0 && synReceived == 1){  //start of File handling
        printf("Receiving Packet %s\n",line);
        char* filename = strtok(NULL,"\n");
        //printf("Size Request line: %lu\n",strlen(line));
        file = getFile(filename);
        if(strcmp(file,STATUS_NOT_FOUND)==0){  //FIN in case of no file found
            strcpy(buffer, "FIN\nFile Not Found\n");
            printf("Sending Packet %s\n", buffer);
            nBytes = strlen(buffer);
            sendto(udpSocket,buffer,nBytes,0,(struct sockaddr *)&serverStorage,addr_size);
            synReceived = 0; 
            break;
        }
        else{
          handleTransmission(udpSocket,(struct sockaddr *)&serverStorage,&addr_size,file);
          synReceived = 0; 
        }   //handleTransmission returns when file transmited completely

        break;
    }

    //send message back, address is serverStorage
    printf("Sending Packet %s \n", buffer);
    sendto(udpSocket,buffer,nBytes,0,(struct sockaddr *)&serverStorage,addr_size);
  }

  return 0;
}

// retrieve file from the system
char* getFile(char* filename) {
    char* buffer;
    FILE *fp;

    // open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("File not found\n");
        return STATUS_NOT_FOUND;
    }

    /* Get the number of bytes */
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    printf("fileSize is: %d\n", fileSize);

    /* reset the file position indicator to the beginning of the file */
    fseek(fp, 0, SEEK_SET);

    buffer = malloc(sizeof(char)*fileSize);

    // copy file into buffer
    fread(buffer, sizeof(char), fileSize, fp);
    fclose(fp);
    return buffer;
}

/*
Format of Packet: 
  Sequence\nxyz\nData\n...null terminated*/
void handleTransmission(int udpSocket, struct sockaddr* serverStorage,socklen_t* addr_size,char* file){
    char* buffer = malloc(1024);
    int nBytes,transmitting = 1;
    int windowStart = 0, windowEnd = 0;
    //sequence num is num used in datagram, sequenceNumSent includes data in the latest package
   
    int sequenceNum = 0, sequenceNumSent = 0, sequenceSentSize = 0;
    int sendingPacket = 1; //set to 0 when waiting on ACKs from client, set to 1 when packets in window ready to be sent
    while(transmitting){
      // sending packets, while it is within our window size and not end of file
      while(windowEnd < windowStart+windowsize && sequenceNumSent != -1) {
        printf("Sending packet %d %d\n", sequenceNumSent, windowsize);
        sequenceNumSent = sendPacket(udpSocket,serverStorage,addr_size,file,sequenceNumSent);
        windowEnd += packetSize;
        printf("windowEnd is now %d\n", windowEnd);
        if (sequenceSentSize == 0) {
          sequenceSentSize = sequenceNumSent;
        }
      }
      // we can recieve the next ackno from recieveack
      // compare with current sequenceNum, if it's more, then we will increment windowStart
      // so that 
      int newSequenceNum = receiveACK(udpSocket,serverStorage,addr_size,file,sequenceNum);
      
      // this is the new sequencenum expected
      // we're done
      if (newSequenceNum == -1) {
          printf("sequenceNum is -1, so we're done\n");
          printf("Sending FIN\n");
          strcpy(buffer,"FIN\n");
          nBytes = strlen(buffer);
          sendto(udpSocket,buffer,nBytes,0,serverStorage,*addr_size);
          transmitting = 0;
      }
      else if (newSequenceNum != sequenceNum) {
        sequenceNum = newSequenceNum;
        printf("sequenceNum is now %d\n", sequenceNum);
        windowStart += packetSize;
        printf("increasing window start to %d\n", windowStart);
      }
    }
}
 /*sends a packet to the client and returns the sequence number of the packet*/
int sendPacket(int udpSocket, struct sockaddr* serverStorage,socklen_t* addr_size,char* file,int sequenceNum){
      char* buffer = malloc(1024);
      int sequenceNumSent = createPacket(&buffer,file,sequenceNum); //returns seq no. that has been sent, will be next seq. no. Set to -1 when entire file has been sent
      int nBytes = strlen(buffer);
      // printf("buffer\n%s\n", buffer);
      sendto(udpSocket,buffer,1024,0,serverStorage,*addr_size);
      return sequenceNumSent;

}
 /*sends a packet to the client and returns the sequence number of the packet*/
int receiveACK(int udpSocket, struct sockaddr* serverStorage,socklen_t* addr_size,char* file,int sequenceNum){
      char* buffer = malloc(1024);
      int nBytes = recvfrom(udpSocket,buffer,1024,0,serverStorage, &(*addr_size));
      buffer[nBytes] = 0;
      // printf("Buffer recieved: %i\n%s\n\n", nBytes, buffer);
      char* line = strtok(buffer, "\n");
      if(strcmp(line,"ACK")==0){
        char* ackNumString = strtok(NULL,"\n");
        int ackNum = atoi(ackNumString); 

        printf("Receiving Packet %d\n",ackNum);

        // this is an ack we expect
        if (ackNum == sequenceNum) {
          return deleteACKedPacket(ackNum);
        }  
        // we got an ack we didnt expect (meaning a packet dropped) 
        else {
          // need to handle retransmission
        }
      }

      return sequenceNum; 
}
int createPacket(char** buffer,char* file,int sequenceNum){
      int nDigits = floor(log10(abs(sequenceNum))) + 1;
      int nFileSizeDigits = floor(log10(abs(fileSize)))+1;
      if (nDigits < 0) {
        nDigits = 1;
      }
      //printf("\n\n%i\n\n", nDigits);
      char sequenceNumString[nDigits+1];
      char fileSizeString[nFileSizeDigits+1];
      strcpy(*buffer,"Sequence\n");
      sprintf(sequenceNumString,"%d",sequenceNum);
      strcat(*buffer,sequenceNumString);
      strcat(*buffer,"\n");
      sprintf(fileSizeString,"%d",fileSize);
      strcat(*buffer,fileSizeString);
      strcat(*buffer,"\n");
      strcat(*buffer,"Data\n");
      int nBytes = strlen(*buffer);
      int freespace = 1024-nBytes;  //bytes left for data in 1024 byte packet
      int futuresum = sequenceNum+freespace;
      if(futuresum>fileSize){
        freespace = fileSize-sequenceNum;
      }
      char* currentData = malloc(1024);
      //printf("FILE:%s\n",file);
      memcpy(currentData,*buffer,nBytes);
      currentData+=nBytes;
      memcpy(currentData,file+sequenceNum,freespace);
      currentData-= nBytes;
      memcpy(*buffer,currentData,freespace+nBytes);
      //freespace is always 1004. IDK if we should hardcode it thoo
      packetArray[numPackets] = newPacket(*buffer,sequenceNum,freespace+nBytes);
      printPacket(packetArray[numPackets]);
      sequenceNum+=freespace;     //increment sequence number by amount of new data sent
      numPackets++;
      if (numPackets >= 1000) {
        numPackets = 0;
      }
      printf("We're at %d packets.\n", numPackets);
      if(futuresum>fileSize) 
        return -1;
      else
        return sequenceNum;
}