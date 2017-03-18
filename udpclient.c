#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define fileBufferLength 10000  //Increment by this amount everytime it overflows

int send_to(int fd, char *buffer, int len, int to, struct sockaddr* serverStorage, socklen_t addr_size) {

  fd_set rfds;
  struct timeval tv;
  int retval;

  /* Watch stdin (fd 0) to see when it has input. */

  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);

  /* Wait up to five seconds. */
  sendto(fd, buffer, len, 0, serverStorage, addr_size);
  // printf("Buffer is %s", buffer);

  tv.tv_sec = 0;
  tv.tv_usec = 500;

  retval = select(fd+1, &rfds, NULL, NULL, &tv);
  /* Don't rely on the value of tv now! */

  if (retval == -1)
      perror("select()");
  else if (retval) {
      // printf("Data is available now.\n");
      /* FD_ISSET(0, &rfds) will be true. */
      printf("Is this sending?\n");
      return 0;
  }
  else {
      printf("Time out\n");
      return -2;
  }
    return 0;
}

int sendSYN(int udpSocket, struct sockaddr* serverStorage,socklen_t* addr_size) {
  char buffer[5];
  strncpy(buffer, "SYN\n", 5);
  printf("Sending packet %s",buffer);
  int response = send_to(udpSocket,buffer,5,0,serverStorage,*addr_size);
  while (response == -2) {
    response = send_to(udpSocket,buffer,5,1000,serverStorage,*addr_size);
  }
  return 0;
}

int main(int argc, char *argv[]){
  char* inOrderFileBuffer; 
  int clientSocket, portNum, nBytes;
  int bufferMultiplier = 1;   //indicates size of file buffer
  char *buffer = malloc(1024);
  char *fileBuffer = malloc(fileBufferLength);
  char fileName[500];
  int fileSize;
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
      sendSYN(clientSocket,(struct sockaddr *)&serverAddr,&addr_size);
      nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);
      char* line = strtok(buffer,"\n");

      // Got SYNACK, so send request for the file
      if(strcmp(line,"SYNACK")==0){
        // printf("Receiving Packet %s \n",line);
        printf("Receiving packet %s\n", buffer);
        strcpy(buffer,"REQUEST\n");
        strcat(buffer,fileName);
        strcat(buffer,"\n");
        printf("Sending packet request %s\n",fileName);
        nBytes = strlen(buffer);
        int response = send_to(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
        while (response == -2) {
          response = send_to(clientSocket,buffer,nBytes,1000,(struct sockaddr *)&serverAddr,addr_size);
        }
        handShook=1;
      }
    }
    if(handShook){  //handles receiving packets, sending acks, and terminating on FIN
      int recievedPacketSize = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);
      printf("recievepacket");
      nBytes = 1024;
      printf("buffer is \n%s\n", buffer);
      printf("nBytes is %i\n", nBytes);

      // allocating newBuffer to store copy of buffer
      char* newBuffer = malloc(100);
      memcpy(newBuffer, buffer, 100);
      char* line = strtok(newBuffer,"\n");
      // substracting bytes from the text before the deliminator
      nBytes -= (strlen(line)+1);
      if(strcmp(line,"FIN")==0){
        char* finMessage = strtok(NULL,"\n");
        printf("Receiving packet %s FIN\n",finMessage);  //change according to spec
        break;
      }
      // process the packet recieved
      else if(strcmp(line,"Sequence")==0) {
        //printf("Recieving sequence...\n");
        char* sequenceNumString = strtok(NULL,"\n");
        // substracting bytes from the text before the deliminator
        // getting sequenceNum
        nBytes -= (strlen(sequenceNumString)+1);
        int sequenceNum = atoi(sequenceNumString);

        // getting file size
        char* fileSizeString = strtok(NULL, "\n");
        fileSize = atoi(fileSizeString);
        if(sequenceNum==0){
          //printf("seq = 0");
          inOrderFileBuffer = malloc(fileSize);
        }
        // subtract filesize from nBytes
        nBytes -= (strlen(fileSizeString)+1);
        
        // getting data
        char* data = strtok(NULL,"\n");
        nBytes -= (strlen(data)+1);

        int headerSize = 1024-nBytes;
        if (sequenceNum+nBytes > fileSize) {
          //printf("sequencenum+nbytes > filesize\n");
          nBytes = fileSize-sequenceNum;
          //printf("nBytes is %i\n", nBytes);
        }

        char* receivedData = malloc(1024);
        memcpy(receivedData, buffer+headerSize, nBytes);
        //printf("sequenceNum is %i\nnBytes is %i\nreceivedData is %s\n\n",sequenceNum, nBytes, receivedData);
        if((sequenceNum + nBytes) > (bufferMultiplier * fileBufferLength)){  //check for file buffer overflow
          bufferMultiplier++;
          fileBuffer = realloc(fileBuffer,bufferMultiplier*fileBufferLength);
          // printf("Buffer has been allocated to %i", bufferMultiplier*fileBufferLength);
        }
        //save data to buffer
        if(sequenceNum==0){
          memcpy(fileBuffer,receivedData, nBytes);
          //printf("filebuffer: %s\n",fileBuffer);
        }
        else {
          // printf("\n\ndataSize is: %i\n%s\n--------------------\n", nBytes, dataSize);
          memcpy(fileBuffer+sequenceNum,receivedData,nBytes);
        }

        memcpy(inOrderFileBuffer+sequenceNum,receivedData,nBytes);  

          
        strcpy(buffer,"ACK\n");
        strcat(buffer,sequenceNumString);  //ack packet that was received and stored
        strcat(buffer,"\n");
        nBytes = strlen(buffer);
        int response = send_to(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
        while (response == -2) {
          response = send_to(clientSocket,buffer,nBytes,1000,(struct sockaddr *)&serverAddr,addr_size);
        }
        printf("Sending Packet %s \n",sequenceNumString);
      }

    }
  } 
  //printf("Filebuffer:\n%s\n",fileBuffer);  //check file has transfered correctly

  // saving to file recieved.data
  FILE *fp = fopen("received.data", "wb+");
  // printf("sizeof buffer is: %lu\n", sizeof(fileBuffer));
  // printf("strlen buffer is: %lu\n", strlen(fileSize));
  //printf("Len of file buf: %d\n",strlen(fileBuffer));
  fwrite(fileBuffer,1,fileSize, fp);
  fclose(fp);
  return 0;
}