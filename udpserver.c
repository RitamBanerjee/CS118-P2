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
//Function Headers
int windowsize = 1024*1; 
char* getFile(char*);
void handleTransmission(int,struct sockaddr*, socklen_t*,char*);
int sendPacket(int,struct sockaddr*, socklen_t*,char*,int);
int receiveACK(int,struct sockaddr*, socklen_t*,char*,int);
int createPacket(char **,char*,int);
typedef struct Packet Packet; 
struct Packet{
  char* buffer;
  int sequenceNum;
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
  //packet->bufferSize = dataSize;

  return packet;
}
void deletePacket(Packet* p){
  if(p!=NULL){
   // printf("inside deletepacket");
    free(p->buffer);
    printf("Deleting packwet w/ seq. no: %d\n",p->sequenceNum);
    free(p);
   // printf("leaving deletepacket");
  }
}

void printPacket(Packet* p) {
  printf("Seq no: %d\n",p->sequenceNum);
}

int deleteACKedPacket(int ACKno){
  for(int i = 0; i<1000;i++){
    // printf("%d,", i);
    if(packetArray[i]!=NULL){
      printPacket(packetArray[i]);
      printf("ACKNO:%d\n",ACKno);
      if(packetArray[i]->sequenceNum==ACKno){
        //printf("packet seq == ackno");
        deletePacket(packetArray[i]);
        return ACKno;
      }
    }
  }
  return 1; 
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
    int windowCount = 0; 
    //sequence num is num used in datagram, sequenceNumSent includes data in the latest package
    int sequenceNum = 0, sequenceNumSent = 0;  
    int sendingPacket = 1; //set to 0 when waiting on ACKs from client, set to 1 when packets in window ready to be sent
    while(transmitting){
      if(sendingPacket){
        while((windowCount*1024)<windowsize){
        sequenceNum = sequenceNumSent;
        sequenceNumSent = sendPacket(udpSocket,serverStorage,addr_size,file,sequenceNumSent);
        printf("Sending packet %d %d\n", sequenceNum, windowsize);
        windowCount++;
        if(sequenceNumSent==-1){
          sendingPacket = 0; 
          break;
        }
      }
        sendingPacket = 0; 
      }
      if(receiveACK(udpSocket,serverStorage,addr_size,file,sequenceNum) && windowCount!=0){
        printf("decrementing window count");
        windowCount--;
        sequenceNum = sequenceNumSent; //once packet acked, we can update sequenceNum
        sendingPacket = 1; 
      }

      printf("windowcount: %d", windowCount);
      if(windowCount==0){
        if(sequenceNumSent==-1)
        {
            printf("Sending FIN\n");
            strcpy(buffer,"FIN\n");
            nBytes = strlen(buffer);
            sendto(udpSocket,buffer,nBytes,0,serverStorage,*addr_size);
            transmitting = 0;
        }
        sendingPacket = 1; 
      }
      // if(sequenceNumSent==-1){  //this indicates file has been transmitted and ACKed completely

      // }
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
      //printf("Buffer recieved: %i\n%s\n\n", nBytes, buffer);
      char* line = strtok(buffer, "\n");
      if(strcmp(line,"ACK")==0){
        char* ackNumString = strtok(NULL,"\n");
        int ackNum = atoi(ackNumString);
        if(ackNum==sequenceNum){  //checks if packet sent has been acked{
          printf("Receiving Packet %d\n",ackNum);
          if(deleteACKedPacket(ackNum)>=0){
            printf("DELETE RETURN 1");
            return 1; 
          }
          else {
            printf("delete return 0");
            return 0; 
          }
        }
      }
      return 0; 
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
      if(futuresum>fileSize) 
        return -1;
      else
        return sequenceNum;
}