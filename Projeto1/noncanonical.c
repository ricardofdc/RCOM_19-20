/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "llfunctions.h"
#include "app_package_handling.h"

#include <sys/time.h>
#include <time.h>



#define _POSIX_SOURCE 1 /* POSIX compliant source */

volatile int STOP = FALSE;

int isFileSizeExpected(int fd, unsigned int file_size){

  struct stat st;
  fstat(fd, &st); //retrieves size of file
  return (st.st_size == file_size);
}

int getLastCharPos(char *str, char character){

  unsigned int str_size = strlen(str);
  for (int i = str_size - 1; i >= 0; i--){
    if (str[i] == character)
      return i;
  }
  return -1;
}

//creates a file with the requested name, if it doesn't already exist. Creates a copy otherwise. Returns file descriptor
int createReceivedFile(char *file_name){

  int file;
  int i = 1;
  char extension[EXTENSION_MAX_SIZE];
  char file_name_no_extension[FILE_NAME_MAX_SIZE]; 

  int last_dot_pos = getLastCharPos(file_name, '.');//position of last '.'
  if (last_dot_pos > 0)//if it has an extension
  {
    strcpy(extension, file_name + last_dot_pos); //get extension
    memcpy(file_name_no_extension, file_name, last_dot_pos); //get file name without extension
    file_name_no_extension[last_dot_pos] = '\0';
  }
  else strcpy(file_name_no_extension, file_name);

  while ((file = open(file_name, O_WRONLY | O_CREAT | O_EXCL | O_APPEND, 0777)) < 0)
  {
    if (errno == EEXIST)
    {
      if (last_dot_pos > 0) //if it has extension
        sprintf(file_name, "%s(%d)%s", file_name_no_extension, i, extension);
      else
        sprintf(file_name, "%s(%d)", file_name_no_extension, i);
      if (strlen(file_name) < FILE_NAME_MAX_SIZE){
        //if the size does not exceed the maximum file
        i++;
      }
      else {
        perror("file name too big");
        exit(-1);
      }
    }
    else
    {
      perror("error on creating file");
      exit(-1);
    }
  }
  return file;
}

int main(int argc, char **argv)
{
  int fd; //, c, res = 0;
  unsigned char buf[D_MAX_SIZE];

  /*unsigned char bufo[14] = {FLAG_STUFF, 0x02, 0x20, 0x40, ESC_OCT, F_FLAG, F_FLAG, ESC_OCT, 0x20, 0x30, 0x10, ESC_OCT, 0x20, 0x30};
  unsigned char bufon[3] = {0x0, 0x0E, 0x70};

  sendICommand(1, bufon, 3);*/

  if ((argc < 2) ||
      ((strcmp("/dev/ttyS0", argv[1]) != 0) &&
       (strcmp("/dev/ttyS1", argv[1]) != 0) &&
       (strcmp("/dev/ttyS2", argv[1]) != 0) &&
       (strcmp("/dev/ttyS4", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = llopen(argv[1], RECEIVER);
  if (fd == 0)
  {
    perror(argv[1]);
    exit(-1);
  }

  //serialRead(fd, buf);
  // while (1)
  // {
  //   llread(fd, buf);
  // }

  //clock test

  struct timeval start, stop;
  gettimeofday(&start, NULL); // get initial time-stamp
  //random erros generation
  srand(time(NULL));

  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no gui�o 
  */

  int bytes_read;
  enum appState state = START;
  uint8_t data_package[D_MAX_SIZE];
  uint8_t start_package[D_MAX_SIZE];
  int s_pack_size;
  char file_name[FILE_NAME_MAX_SIZE];
  unsigned int file_size;
  int file = -1;

  uint8_t expected_N = 0;
  int number_of_N_groups = 0; //incremented each time N reaches 255 (N is mod 256) (each group corresponds to 256 packets)

  while (STOP == FALSE)
  {
    bytes_read = llread(fd,data_package); //get message from serial port
    if (bytes_read != DISCARD) //if the data is new and has no errors
    {
      //read the serial port message and store it in data_package
      if (state == START)
      {
        s_pack_size = readStartPackage(data_package, bytes_read, (uint8_t *) &file_size, (uint8_t *) file_name);
        if (s_pack_size > 0)
        {
          file = createReceivedFile(file_name); //create the file to be received
          memcpy(start_package, data_package, s_pack_size); //store the start package for future validation
          stateMachineApp(&state); //start reading data
        }
      }
      else if (state == DATA)
      {
        int read_result = readPackage(data_package, buf, expected_N, start_package, s_pack_size);
        if (read_result > 0){
          //is valid data package
          if (write(file, buf, read_result) < 0)
          {
            perror("writing to file");
            exit(2);
          }
          //
          display_completion(file_size, expected_N + 256 * number_of_N_groups);
          if (expected_N == 255)
            number_of_N_groups++;
          expected_N = (expected_N + 1) % 256;
          //
        }
        else if (read_result == VALID_END_PACKAGE)
          stateMachineApp(&state); //move on to END phase
        else {
          printf("UNEXPECTED/INVALID PACKAGE\n");
        }
      }
      
      if (state == END)
      {
        stateMachineApp(&state);
      }
    }

    if (state == STOP_)
      STOP = TRUE;
  }

  // ... do stuff ... //

  gettimeofday(&stop, NULL); // get final time-stamp

  //

  if (llclose(fd, RECEIVER) < 0)
  {
    printf("Timed out while closing port\n");
  }

  if (!isFileSizeExpected(file, file_size))
  {
    printf("Received file size is different than expected!!!\n");
  }

  close(file);


  

  double seconds = (double)(stop.tv_sec - start.tv_sec);

  double useconds = (seconds * 1000000) + stop.tv_usec - (start.tv_usec);

  double t_s = useconds / 1000000;
  // subtract time-stamps and
  // multiply to get elapsed
  // time in ns
  printf("%f, time elapsed\n", t_s);


  return 0;
}
