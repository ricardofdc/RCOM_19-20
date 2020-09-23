/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "llfunctions.h"
#include "app_package_handling.h"

#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */

volatile int STOP = FALSE;

int sendMessage(int bytes_written, int fd, uint8_t *data_package)
{

  int resend = FALSE;
  if (bytes_written > 0)
  {
    int write_result = llwrite(fd, data_package, bytes_written);
    if (write_result == RESEND_RET)
      resend = TRUE;
    else if (write_result == TIMEOUT_RET)
    {
      printf("TIMED OUT\n");
      exit(2);
    }
    // printf("message sent - %d bytes\n", write_result);
  }
  return resend;
}

int fileDataReading(int fd, uint8_t *data_package, enum appState *state)
{

  uint8_t buf[PACKAGE_DATA_SIZE];
  int bytes_read = read(fd, buf, PACKAGE_DATA_SIZE);

  // for (unsigned int i = 0; i < bytes_read; i++)
  // {
  //   printf("%x", buf[i]);
  // }
  // printf("\n");

  if (bytes_read < 0)
  {
    perror("file reading");
    exit(1);
  }
  else if (bytes_read == 0)
  {
    //if it reaches the end of the file, go to the END state
    stateMachineApp(state);
  }
  else
  {
    //TODO: use sequence number
    bytes_read = buildDataPackage(bytes_read, buf, data_package);
  }
  return bytes_read;
}

int setupControlPackage(int fd, uint8_t *data_package, uint8_t c, enum appState *state, char *file_name, unsigned int name_size)
{

  struct stat st;
  fstat(fd, &st); //retrieves size of file
  int bytes_written = buildControlPackage(c, st.st_size, (uint8_t *) file_name, name_size, data_package);
  stateMachineApp(state); //if Start, goes to DATA state; if END, goes to stop state
  return bytes_written;
}

int main(int argc, char **argv)
{
  int fd; //, c, res;
  unsigned char buf[D_MAX_SIZE];
  //int i, sum = 0, speed = 0;

  if ((argc < 3) ||
      ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
       (strcmp("/dev/ttyS1", argv[1]) != 0) &&
       (strcmp("/dev/ttyS2", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort FileName\n\tex: nserial /dev/ttyS1 pinguim.gif\n");
    exit(1);
  }

  //file name and respective size
  unsigned file_name_length = strlen(argv[2]);
  if (file_name_length > ORIGINAL_FILE_MAX_SIZE ){
    printf("File name is too big\n");
    exit(4);
  }
  
  //open file to be read
  int file = open(argv[2], O_RDONLY, 0777);
  if (file < 0)
  {
    perror("error on opening requested file");
    exit(-1);
  }
  //

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = llopen(argv[1], TRANSMITTER);
  if (fd == 0)
  {
    perror(argv[1]);
    exit(-1);
  }
  else if (fd == TIMEOUT_RET)
  {
    printf("Timed out while opening port\n");
    exit(2);
  }

  /* 
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar 
    o indicado no gui�o 
  */
  
  int resend = FALSE;
  int bytes_written;
  enum appState state = START;
  uint8_t data_package[D_MAX_SIZE];

  while (STOP == FALSE)
  {
    if (!resend)
    {
      //if the previous message was received sucessfully, build the next one
      if (state == START)
      {
        bytes_written = setupControlPackage(file, data_package, C_START, &state, argv[2], file_name_length);
      }
      else if (state == DATA)
      {
        bytes_written = fileDataReading(file, data_package, &state);
        //printf("%d - BYTES WRITTEN\n", bytes_written);
      }
      else if (state == END)
      {
        bytes_written = setupControlPackage(file, data_package, C_END, &state, argv[2], file_name_length);
      }
    }
    resend = sendMessage(bytes_written, fd, data_package);
    if (!resend && state == STOP_) //if it reached the end and the END Package has been received sucessfully
      STOP = TRUE;
  }

  //close
  if (llclose(fd, TRANSMITTER) < 0)
  {
    printf("Timed out while closing port\n");
  }

  close(file);

  return 0;
}
