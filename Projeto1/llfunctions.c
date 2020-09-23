
#include "llfunctions.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>

int numAlarms = 0;
int alarmFlag = FALSE;
struct termios oldtio, newtio;
int curr_ns = 0;

void alarmHandler(int sig)
{
    if (sig == SIGALRM)
    {
        numAlarms++;
        alarmFlag = TRUE;
        printf("alarm nº %d called\n", numAlarms);
    }
}

void sendControlMessage(int fd, uint8_t C, uint8_t A)
{
    uint8_t msg[5];
    msg[0] = F_FLAG;
    msg[1] = A;
    msg[2] = C;
    msg[3] = A ^ C;
    msg[4] = F_FLAG;
    write(fd, msg, 5);
}

int sendICommand(int fd, int c_num, uint8_t *info, unsigned int info_length)
{
    uint8_t msg[F_STUFFED_SIZE];
    uint8_t unstuffed_bcc;
    msg[0] = F_FLAG;
    msg[1] = A1_;
    msg[2] = C_I(c_num);   //C
    msg[3] = A1_ ^ msg[2]; //BCC 1

    // for (unsigned int i = 0; i < info_length; i++)
    // {
    //     printf("%x", info[i]);
    // }
    // printf("\n");

    unsigned int header_size = 4;

    unsigned int stuffed_msg_size = byteStuffing(info, info_length, msg + header_size); //stuffs the message

    unstuffed_bcc = dataBCC(info, info_length); //BCC 2, XOR of all the data octets

    unsigned int stuffed_bcc_size = byteStuffing(&unstuffed_bcc, 1, msg + header_size + stuffed_msg_size); //stuffs the bcc

    msg[stuffed_msg_size + header_size + stuffed_bcc_size] = F_FLAG;

    unsigned int tail_size = stuffed_bcc_size + 1;

    unsigned int frame_size = header_size + stuffed_msg_size + tail_size;

    // for (unsigned int i = 0; i < frame_size; i++)
    // {
    //     printf("%x", msg[i]);
    // }
    // printf("\n");

    return write(fd, msg, frame_size);
}

int readIFrame(int fd, uint8_t *buffer)
{

    uint8_t stuffed_msg_w_tail[D_STUFFED_MAX_SIZE + F_STUFFED_TAIL_SIZE];
    uint8_t unstuffed_msg_w_tail[D_MAX_SIZE + F_TAIL_SIZE];
    int stuffed_msg_size = getFrame(fd, stuffed_msg_w_tail);

    // printf("STUFFED\n");
    // for (unsigned int i = 0; i < stuffed_msg_size; i++)
    // {
    //     printf("%x", stuffed_msg_w_tail[i]);
    // }
    // printf("\n");
    if (stuffed_msg_size <= 0)
    {
        return DATA_ERROR;
    }

    int unstuffed_msg_size = destuffing(stuffed_msg_w_tail, stuffed_msg_size, unstuffed_msg_w_tail) - F_TAIL_SIZE; //destuffs message

    if (unstuffed_msg_size <= 0)
    {
        return DATA_ERROR;
    }

    uint8_t bcc = unstuffed_msg_w_tail[unstuffed_msg_size]; //retrieves BCC from tail

    // printf("UNSTUFFED\n");
    // for (unsigned int i = 0; i < unstuffed_msg_size + F_TAIL_SIZE; i++)
    // {
    //     printf("%x", unstuffed_msg_w_tail[i]);
    // }
    // printf("\n");

    int is_bcc_valid = checkBCC(unstuffed_msg_w_tail, unstuffed_msg_size, bcc); //compares the received BCC and the BCC calculated with the received data
    //printf("is bcc valid? %d\n", is_bcc_valid);

    //error simulation
    
    if (hasError(BCC2ERR))
        is_bcc_valid = 0;

    if (is_bcc_valid)
    {
        memcpy(buffer, unstuffed_msg_w_tail, unstuffed_msg_size);
        return unstuffed_msg_size;
    }
    else
    {
        return DATA_ERROR;
    }
}

uint8_t dataBCC(uint8_t *info, unsigned int info_length)
{

    uint8_t bcc = info[0];
    for (unsigned i = 1; i < info_length; i++)
    {
        bcc = bcc ^ info[i];
    }
    return bcc;
}

//builds stuffed msg, returns size of msg after stuffing
int byteStuffing(uint8_t *msg, unsigned int msg_length, uint8_t *stuffed_msg)
{

    unsigned j = 0;
    for (unsigned i = 0; i < msg_length; i++)
    {
        uint8_t curr_char = msg[i];
        //printf("%x\n", curr_char);
        if (curr_char == F_FLAG)
        {
            stuffed_msg[j++] = ESC_OCT;
            stuffed_msg[j++] = FLAG_STUFF;
        }
        else if (curr_char == ESC_OCT)
        {
            stuffed_msg[j++] = ESC_OCT;
            stuffed_msg[j++] = ESC_STUFF;
        }
        else
        {
            stuffed_msg[j++] = curr_char;
        }
        //printf("%d - size\n", j);
    }
    return j;
}

//builds a destuffed message. returns size of destuffed msg
int destuffing(uint8_t *stuffed_msg, unsigned int msg_length, uint8_t *destuffed_msg)
{
    unsigned j = 0;
    // printf("DESTUFFING\n");
    for (unsigned i = 0; i < msg_length; i++)
    {
        uint8_t curr_char = stuffed_msg[i];
        // printf("%x", curr_char);
        if (curr_char == ESC_OCT)
        {
            i++;
            uint8_t next_char = stuffed_msg[i];
            // printf("%x", next_char);
            if (next_char == FLAG_STUFF)
            {
                destuffed_msg[j] = F_FLAG;
            }
            else if (next_char == ESC_STUFF)
            {
                destuffed_msg[j] = ESC_OCT;
            }
        }
        else
        {
            destuffed_msg[j] = curr_char;
        }
        j++;
    }
    // printf("\n");
    // printf("DESTUFFING ENDED\n");
    return j;
}

int getFrame(int fd, uint8_t *buffer)
{
    int res = 0;
    uint8_t curr_char;
    while (!alarmFlag)
    { /* loop for input */
        if (res >= D_STUFFED_MAX_SIZE + F_STUFFED_TAIL_SIZE)
            return -2;
        if (read(fd, &curr_char, 1))
        {
            buffer[res] = curr_char;
            // printf("%x", buffer[res]);
            res++;
        }
        else
        {
            return -1;
        }
        if (curr_char == F_FLAG && res > 0)
            break;
    }

    // printf("\n");
    // printf("RES - %d\n\n", res);
    return res;
}

int checkBCC(uint8_t *msg, unsigned length, uint8_t bcc)
{
    uint8_t data_bcc = msg[0];
    for (unsigned i = 1; i < length; i++)
    {
        data_bcc = data_bcc ^ msg[i];
    }
    return (data_bcc == bcc);
}

int llopen(char *port, int type)
{

    int fd;
    fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0)
        return 0;

    initPort(fd);

    signal(SIGALRM, alarmHandler);
    siginterrupt(SIGALRM, 1);

    if (type == TRANSMITTER)
    {
        //sprintf(buf, "%lx", frame);

        do
        {
            alarmFlag = FALSE;
            printf("Sending SET command - attempt nº %d\n", numAlarms + 1);
            sendControlMessage(fd, C_SET, A1_);
            alarm(TIMEOUT);
            if (serialReadControl1(fd, C_UA, A1_) > 0)
            {
                alarm(0);
                printf("Received UA reply\n");
                break;
            }
        } while (alarmFlag && numAlarms < MAX_RETR);
    }
    else
    {
        if (serialReadControl1(fd, C_SET, A1_) > 0)
        {
            printf("Received SET command\n");
            sendControlMessage(fd, C_UA, A1_);
            printf("Sent UA reply\n");
        }
    }

    alarmFlag = FALSE;

    if (numAlarms == MAX_RETR)
    {
        numAlarms = 0;
        return TIMEOUT_RET;
    }
    else
    {
        numAlarms = 0;
        return fd;
    }
}

int llwrite(int fd, uint8_t *buffer, int length)
{
    //used to synch the transmitter and receiver
    int curr_nr = (curr_ns + 1) % 2;
    unsigned bytes_written = 0;
    int reply_type = 0;

    printf("%d - CURR NS\n", curr_ns);

    do
    {
        alarmFlag = FALSE;
        printf("Sending I frame - attempt nº %d\n", numAlarms + 1);
        bytes_written = sendICommand(fd, curr_ns, buffer, length);
        alarm(TIMEOUT);
        reply_type = serialReadControl2(fd, C_RR(curr_nr), A1_, C_REJ(curr_nr), FALSE);
        if (reply_type > 0)
        {
            //Received a valid reply
            alarm(0);
            break;
        }
    } while (alarmFlag && numAlarms < MAX_RETR);

    alarmFlag = FALSE;
    numAlarms = 0;
    if (reply_type == RR)
    {
        //The I frame was sucessfully received
        printf("Received RR reply\n");
        curr_ns = (curr_ns + 1) % 2;
        return bytes_written;
    }
    else if (reply_type == REJ)
    {
        //The I frame was either received with errors or a duplicate
        printf("Received REJ reply\n");
        return RESEND_RET;
    }
    else
    {
        printf("Timed out while sending I frame\n");
        return TIMEOUT_RET;
    }
}

int llread(int fd, uint8_t *buffer)
{
    int curr_nr = (curr_ns + 1) % 2;

    printf("%d - CURR NS\n", curr_ns);

    int command_type = serialReadControl2(fd, C_I(curr_ns), A1_, C_I(curr_nr), TRUE);
    printf("COMMAND_TYPE HEADER RESULT = %d\n", command_type);

    int msg_size = DISCARD;
    // sleep(1);
    if (command_type > 0)
    {
        msg_size = readIFrame(fd, buffer);
        printf("msg size = %d\n", msg_size);
        if (command_type == 1)
        {
            if (msg_size >= 0)
            {
                printf("Data is ok, sent RR reply\n");
                sendControlMessage(fd, C_RR(curr_nr), A1_);
                curr_ns = (curr_ns + 1) % 2;
            }
            else
            {
                printf("Error in data, Sent REJ reply\n");
                msg_size = DISCARD;
                sendControlMessage(fd, C_REJ(curr_nr), A1_);
            }
        }
        else if (command_type == 2)
        {
            printf("Frame is duplicate, sent RR reply\n");
            msg_size = DISCARD;
            //changed curr_nr to curr_ns TODO: CHECK THIS
            sendControlMessage(fd, C_RR(curr_ns), A1_);
        }
    }

    return msg_size;
}

int llclose(int fd, int type)
{
    if (type == TRANSMITTER)
    {
        do
        {
            alarmFlag = FALSE;
            printf("Sending DISC command - attempt nº %d\n", numAlarms + 1);
            sendControlMessage(fd, C_DISC, A1_);
            alarm(TIMEOUT);
            if (serialReadControl1(fd, C_DISC, A2_) > 0)
            {
                alarm(0);
                printf("Received DISC command\n");
                sendControlMessage(fd, C_UA, A2_);
                printf("Sent UA reply\n");
                break;
            }
        } while (alarmFlag && numAlarms < MAX_RETR);
    }
    else
    {
        if (serialReadControl1(fd, C_DISC, A1_) > 0)
        {
            printf("Received DISC command\n");
            do
            {
                alarmFlag = FALSE;
                printf("Sending DISC command - attempt nº %d\n", numAlarms + 1);
                sendControlMessage(fd, C_DISC, A2_);
                alarm(TIMEOUT);
                if (serialReadControl1(fd, C_UA, A2_) > 0)
                {
                    alarm(0);
                    printf("Received UA reply\n");
                    break;
                }
            } while (alarmFlag && numAlarms < MAX_RETR);
        }
    }

    alarmFlag = FALSE;

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
    close(fd);

    if (numAlarms == MAX_RETR)
    {
        numAlarms = 0;
        return TIMEOUT_RET;
    }
    else
    {
        numAlarms = 0;
        return 1;
    }
}

int serialReadControl2(int fd, uint8_t c, uint8_t a, uint8_t c2, int headerI)
{
    enum state curr_state = START_S;
    enum state curr_state2 = START_S;

    uint8_t curr_byte;
    while (curr_state != STOP_S && curr_state2 != STOP_S && !alarmFlag)
    { /* loop for input */
        // if (headerI)
        //     printf("curr_byte = %x, curr_state1 = %d, curr_state2 = %d\n", curr_byte, curr_state, curr_state2);
        if (read(fd, &curr_byte, 1) < 0)
        {
            if (errno != EINTR)
                return -1;
        }
        stateMachineOpen(&curr_state, &curr_byte, c, a, headerI);
        if (c2 != 0x0F)
        {
            stateMachineOpen(&curr_state2, &curr_byte, c2, a, headerI);
        }
    }
    if (curr_state == STOP_S)
        return 1;
    else if (curr_state2 == STOP_S)
        return 2;
    else
        return FALSE;
}

int serialReadControl1(int fd, uint8_t c, uint8_t a)
{
    return serialReadControl2(fd, c, a, 0x0F, FALSE);
}

void initPort(int fd)
{

    if (tcgetattr(fd, &oldtio) == -1)
    { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
    newtio.c_cc[VMIN] = 1;  /* blocking read until 5 chars received */

    /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) pr�ximo(s) caracter(es)
  */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");
}
