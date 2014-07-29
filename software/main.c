/*-------------------------------------------------------------------------------------------------
/ Computer side software for Atmel Xmega32E5 UART bootloader.
/--------------------------------------------------------------------------------------------------
/ “THE COFFEEWARE LICENSE” (Revision 1):
/ <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
/ can do whatever you want with this stuff. If we meet some day, and you think
/ this stuff is worth it, you can buy me a coffee in return.
/--------------------------------------------------------------------------------------------------
/ v0.1 - August 2014
/------------------------------------------------------------------------------------------------*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "serial_lib.h"
/*-----------------------------------------------------------------------------------------------*/
const float version = 0.1;
/*-----------------------------------------------------------------------------------------------*/
int verbose = 0;
int immediateExit = 0;
uint8_t dataBuffer[65536];
/*-----------------------------------------------------------------------------------------------*/
#define PAGE_SIZE 128
/*-----------------------------------------------------------------------------------------------*/
const char filePath[256]; 
const char portPath[256];
/*-----------------------------------------------------------------------------------------------*/
int readACK(fd);
int getVersion(fd);
int sendPing(int fd);
int connectDevice(char* path);
int setRTS(int fd, int level);
int parseUntilColon(FILE *file_pointer);
int parseHex(FILE *file_pointer, int num_digits);
int parseIntelHex(char *hexfile, uint8_t* buffer, int *startAddr, int *endAddr);
/*-----------------------------------------------------------------------------------------------*/
int main(int argc, char *argv[]) 
{    
    int i;
    int c;
    int fd;
    int offset;
    uint8_t t8;    
    int err = 0;
    int pageNumber;
    int gotFile = 0;
    int gotPort = 0; 
    int endAddress = 0;
    int startAddress = 1;

    while ((c = getopt(argc, argv, "f:p:vi")) != -1)
    {
        switch (c) 
        {
            case 'v':
            {
                verbose = 1;
                break;
            }
            case 'f':
            {
                gotFile = 1;     
                sprintf(filePath,"%s",optarg);           
                break;
            }
            case 'p':
            {
                gotPort = 1;                
                sprintf(portPath,"%s",optarg);
                break;
            }
            case 'i':
            {
                immediateExit = 1;
                break;
            }
            default:
            {
                err = 1;
                printf(" [%c]: Unrecognized option!\n",c);
                break;
            }
        }   
    }      
    
    if(verbose)
    {
        printf("-----------------------------------------------------------------------\n");
        printf(" - teaLoader - Atmel XMega32E5 serial bootloader\n");    
        printf("               Copyright (c) 2014 - <ihsan@kehribar.me>\n");
        printf("               Released under Coffeware License\n");
        printf("               https://github.com/kehribar/tealoader\n");
        printf("               Software version: %2.1f\n",version);    
        printf("-----------------------------------------------------------------------\n");
    }

    if((err==1) || (gotFile==0) || (gotPort==0))
    {
        printf("Argument parsing error!\n");                
        printf("Usage: %s [-f <fileName>] [-p <portPath>] [-v] [-i]\n",argv[0]);
        printf("       -v: verbose output\n");
        printf("       -i: immediate exit\n");                    
        
        if(!immediateExit)
        {
            printf("> Press enter key to exit ...\n");
            getchar();
        }        
        return 0;
    }

    fd = connectDevice(portPath);

    if(fd < 0)
    {
        return 0;
    }

    /* Auto reset the board */
    setRTS(fd,1); setRTS(fd,0); setRTS(fd,1);

    if(sendPing(fd) > 0)
    {
        printf("> Ping OK\n");
    }
    else
    {
        if(verbose)
            printf("[err]: Ping problem\n");
        return 0;
    }

    printf("> Firmware version: %d\n",getVersion(fd));    

    memset(dataBuffer, 0xFF, sizeof(dataBuffer));

    if(parseIntelHex(filePath, dataBuffer, &startAddress, &endAddress) == 0);
    {
        return 0;
    }

    if(startAddress != 0)
    {
        printf("> You should change the startAddress = 0 assumption\n");
        return 0;
    }

    if(endAddress > (32768))
    {
        printf("Program size is too big!\n");
        return 0;
    }

    printf("> Erasing the memory ...\n");
    serialport_writebyte(fd,'d');
    if (readACK(fd) > 0)
    {
        if(verbose)
            printf("[dbg]: ACK OK\n");
    }
    else
    {
        if(verbose)
            printf("[dbg]: ACK problem\n");
        return 0;
    }

    i = 0;    
    offset = 0;
    pageNumber = 0;

    if(!verbose)
        setvbuf(stdout, NULL, _IONBF, 0);

    while(offset<endAddress)
    {        
        if(verbose)
        {
            printf("\n");
            printf("[dbg]: Page number: %d\n",pageNumber);
            printf("[dbg]: Page base address: %d\n",offset);
        }
        
        /* Fill the page buffer command */
        serialport_writebyte(fd,'b');

        if(verbose)
            printf("[dbg]: Uploading: %c%d\n",'%',((100 * offset) / endAddress));
        else
            printf("> Uploading: %c%d\r",'%',((100 * offset) / endAddress));

        if (readACK(fd) > 0)
        {
            if(verbose)
                printf("[dbg]: ACK OK\n");
        }
        else
        {
            if(verbose)
                printf("[dbg]: ACK problem\n");
            return 0;
        }
         
        if(verbose)      
            printf("    ");
        for(i=0;i<PAGE_SIZE;i++)
        {   
            if(verbose)                     
            {
                printf("[%3d] %2X   ",i,dataBuffer[i+offset]);     
                if((i%8)==7)
                {
                    printf("\n    ");
                }
            }                   
            serialport_writebyte(fd,dataBuffer[i+offset]);
        }
        if(verbose)    
            printf("\n");

        /* Write the page command */
        serialport_writebyte(fd,'c');  

        if (readACK(fd) > 0)
        {
            if(verbose)
                printf("[dbg]: ACK OK\n");
        }
        else
        {
            if(verbose)
               printf("[dbg]: ACK problem\n");
            return 0;
        }

        t8 = (offset >> 0) & 0xFF;
        serialport_writebyte(fd,t8);

        t8 = (offset >> 8) & 0xFF;
        serialport_writebyte(fd,t8);

        t8 = (offset >> 16) & 0xFF;
        serialport_writebyte(fd,t8);

        t8 = (offset >> 24) & 0xFF;
        serialport_writebyte(fd,t8);

        if (readACK(fd) > 0)
        {
            if(verbose)
                printf("[dbg]: ACK OK\n");
        }
        else
        {
            if(verbose)
                printf("[dbg]: ACK problem\n");
            return 0;
        }

        offset += PAGE_SIZE;
        pageNumber++;
    }

     if(verbose)
            printf("[dbg]: Uploading: %c%d\n",'%',100);
        else
            printf("> Uploading: %c%d\n",'%',100);
        
    printf("> Jumping to the user application\n");

    /* Jump to the user app */
    serialport_writebyte(fd,'x');

    serialport_close(fd);

    if(!immediateExit)
    {
        printf("> Press enter key to exit ...\n");
        getchar();
    }        

    return 0;
}
/*-----------------------------------------------------------------------------------------------*/
int parseIntelHex(char *hexfile, uint8_t* buffer, int *startAddr, int *endAddr) 
{
  int address, base, d, segment, i, lineLen, sum;
  FILE *input;
  
  input = strcmp(hexfile, "-") == 0 ? stdin : fopen(hexfile, "r");
  if (input == NULL) {
    printf("> Error opening %s: %s\n", hexfile, strerror(errno));
    return 0;
  }
  
  while (parseUntilColon(input) == ':') {
    sum = 0;
    sum += lineLen = parseHex(input, 2);
    base = address = parseHex(input, 4);
    sum += address >> 8;
    sum += address;
    sum += segment = parseHex(input, 2);  /* segment value? */
    if (segment != 0) {   /* ignore lines where this byte is not 0 */
      continue;
    }
    
    for (i = 0; i < lineLen; i++) {
      d = parseHex(input, 2);
      buffer[address++] = d;
      sum += d;
    }
    
    sum += parseHex(input, 2);
    if ((sum & 0xff) != 0) {
      printf("> Error: Checksum error between address 0x%x and 0x%x\n", base, address);
      return 0;
    }
    
    if(*startAddr > base) {
      *startAddr = base;
    }
    if(*endAddr < address) {
      *endAddr = address;
    }
  }
  
  fclose(input);
  return 1;
}
/*-----------------------------------------------------------------------------------------------*/
int parseUntilColon(FILE *file_pointer) 
{
  int character;
  
  do {
    character = getc(file_pointer);
  } while(character != ':' && character != EOF);
  
  return character;
}
/*-----------------------------------------------------------------------------------------------*/
int parseHex(FILE *file_pointer, int num_digits) 
{
  int iter;
  char temp[9];

  for(iter = 0; iter < num_digits; iter++) {
    temp[iter] = getc(file_pointer);
  }
  temp[iter] = 0;
  
  return strtol(temp, NULL, 16);
}
/*-----------------------------------------------------------------------------------------------*/
/* Taken from: http://www.linuxquestions.org/questions/programming-9/manually-controlling-rts-cts-326590/#post1658463 */
int setRTS(int fd, int level)
{
    int status;

    if (ioctl(fd, TIOCMGET, &status) == -1) {
        perror("setRTS(): TIOCMGET");
        return 0;
    }
    if (level)
        status |= TIOCM_RTS;
    else
        status &= ~TIOCM_RTS;
    if (ioctl(fd, TIOCMSET, &status) == -1) {
        perror("setRTS(): TIOCMSET");
        return 0;
    }
    return 1;
}
/*-----------------------------------------------------------------------------------------------*/
int connectDevice(char* path)
{
    int fd = -1;    

    fd = serialport_init(path,115200,'n');

    if(fd < 0)
    {
        printf("[err]: Connection error.\n");
        return -1;
    }
    else
    {
        
        printf("> Conection OK\n");
        return fd;
    }       
}
/*-----------------------------------------------------------------------------------------------*/
int getVersion(fd)
{
    char msg;

    serialport_writebyte(fd,'v');

    /* Read the response */
    if(readRawBytes(fd,&msg,1,10000) < 0)
    {
        /* Timeout or read problem */
        return -1;
    }

    return msg;
}
/*-----------------------------------------------------------------------------------------------*/
int readACK(fd)
{
    char msg;

    /* Read the response */
    if(readRawBytes(fd,&msg,1,10000) < 0)
    {
        /* Timeout or read problem */
        return -1;
    }

    if(msg == 'Y')
    {
        /* Return OK */
        return 1;
    }
    else
    {
        /* Wrong response ... */
        return -1;
    }
}
/*-----------------------------------------------------------------------------------------------*/
int sendPing(int fd)
{    
    /* Send ping message */
    serialport_writebyte(fd,'a');    

    return readACK(fd);
}
/*-----------------------------------------------------------------------------------------------*/
