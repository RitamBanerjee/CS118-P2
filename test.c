#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    char fileName[50];
    strcpy(fileName,argv[1]);
    FILE * filp = fopen(fileName, "rb"); 
    fseek(filp, 0, SEEK_END);
    long fsize = ftell(filp);
    fseek(filp, 0, SEEK_SET);  //same as rewind(f);
    char *buffer = malloc(fsize + 1);
    
    FILE* output = fopen("output", "wb+");
    if (!filp) { printf("Error: could not open file\n"); return -1; }

    fread(buffer, fsize, 1, filp);
    printf("%lu\n", fsize);
    printf("%lu\n", strlen(buffer));
    printf("%lu\n", sizeof(buffer));
    printf("%s\n", buffer);

    long sequenceNum = 0;
    int bytesToRead = 1024;
    while (1) {
        printf("sequenceNum is %lu\n", sequenceNum);
        if (sequenceNum+bytesToRead > fsize) {
            bytesToRead = fsize-sequenceNum;
        }
        int byteRead = fwrite(buffer+sequenceNum, 1, bytesToRead, output);
        printf("%i bytesRead\n", byteRead);
        sequenceNum += bytesToRead;

        if (sequenceNum >= fsize) {
            printf("sequenceNum is >= fsize %lu, breaking %lu\n", fsize, sequenceNum);
            break;
        }
    }
    // fwrite(buffer,1,fsize, output);

    // Done and close.
    fclose(filp);
    fclose(output);
    return 0;
}