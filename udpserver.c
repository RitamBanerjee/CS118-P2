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
char* getFile(char*);
void handleTransmission(int,struct sockaddr*, socklen_t*,char*);
int createPacket(char **,char*,int);

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
        printf("Receiving Packet %s ",line);
        char* filename = strtok(NULL,"\n");
        printf("Size Request line: %d\n",strlen(line));
        file = getFile(filename);
        if(strcmp(file,STATUS_NOT_FOUND)==0){  //FYN in case of no file found
            strcpy(buffer, "FYN\n File Not Found\n");
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
  Sequence:xyz:Data:null terminated*/
void handleTransmission(int udpSocket, struct sockaddr* serverStorage,socklen_t* addr_size,char* file){
    char* buffer = malloc(1024);
    int nBytes,transmitting = 1;
    //sequence num is num used in datagram, sequenceNumSent includes data in the latest package
    int sequenceNum = 0, sequenceNumSent = 0;  
    int windowSize = 0; 
    while(transmitting){
      sequenceNumSent = createPacket(&buffer,file,sequenceNum);//record seq no. that has been sent, will be next seq. no. 
      nBytes = strlen(buffer);
      printf("sending packet: %i\n%s\n",nBytes, buffer);
      sendto(udpSocket,buffer,nBytes,0,serverStorage,*addr_size);
      // if(sequenceNumSent==-1)
      //   transmitting=0;
      nBytes = recvfrom(udpSocket,buffer,1024,0,serverStorage, &(*addr_size));
      buffer[nBytes] = 0;
      printf("Buffer recieved: %i\n%s\n\n", nBytes, buffer);
      char* line = strtok(buffer, "\n");
      char* ackNumString = strtok(NULL,"\n");
      int ackNum = atoi(ackNumString);
      if(ackNum==sequenceNum){  //checks if packet sent has been acked{
        printf("ackno:%d\n",ackNum);
        if(sequenceNumSent==-1){  //this plus ack means file is transmitted completely
          printf("sending fyn\n");
          strcpy(buffer,"FYN\n");
          nBytes = strlen(buffer);
          sendto(udpSocket,buffer,nBytes,0,serverStorage,*addr_size);
          transmitting = 0;
        }
        sequenceNum = sequenceNumSent; //update seq. number
      }
      else{//handle retransmission

      }

    }
}

int createPacket(char** buffer,char* file,int sequenceNum){
      int nDigits = floor(log10(abs(sequenceNum))) + 1;
      if (nDigits < 0) {
        nDigits = 1;
      }
      printf("\n\n%i\n\n", nDigits);
      char sequenceNumString[nDigits+1];
      strcpy(*buffer,"Sequence\n");
      sprintf(sequenceNumString,"%d",sequenceNum);
      strcat(*buffer,sequenceNumString);
      strcat(*buffer,"\n");
      strcat(*buffer,"Data\n");
      int nBytes = strlen(*buffer);
      int freespace = 1024-nBytes-1;  //bytes left for data in 1024 byte packet
      char currentData[1024];
      strncpy(currentData,&file[sequenceNum],freespace);
      sequenceNum+=freespace;     //increment sequence number by amount of new data sent
      int nullpos = strlen(currentData);
      currentData[nullpos] = '\0';   //null terminate data
      strcat(*buffer,currentData);
      if(sequenceNum>fileSize)   //indicate file transmitted
        return -1;
      else
        return sequenceNum;
}
