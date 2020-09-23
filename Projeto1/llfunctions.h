#ifndef LLFUNCTIONS_H
#define LLFUNCTIONS_H

#include "msg_state_machine.h"
#include <termios.h>

#define BIT(n) (0x01 << (n))

#define TRANSMITTER 0
#define RECEIVER 1

#define COMMAND 0
#define REPLY 1

#define FALSE 0
#define TRUE 1

#define MAX_RETR 3
#define TIMEOUT 3
#define BAUDRATE B38400

//ll return values
#define TIMEOUT_RET -1
#define RESEND_RET -2

//I FRAME data max size
#define D_MAX_SIZE         255
#define D_STUFFED_MAX_SIZE D_MAX_SIZE * 2

//I frame max size
#define HEAD_SIZE                4
#define F_TAIL_SIZE              2
#define F_STUFFED_TAIL_SIZE      3
#define F_STUFFED_SIZE           HEAD_SIZE + D_STUFFED_MAX_SIZE + F_STUFFED_TAIL_SIZE

//I frame return values
#define DATA_ERROR -1

//reply types
#define RR  1
#define REJ 2

//discard frame
#define DISCARD -1



/**
 * @brief Indicates the alarm call through a global flag and increments the alarm counter
 * @param sig signal identifier.
 */
void alarmHandler(int sig);

/**
 * @brief Assembles the control frame and sends it to the given file descriptor
 * 
 * @param fd     The file descriptor.
 * @param C      Frame control field.
 * @param A      Frame adress field.
 */
void sendControlMessage(int fd, uint8_t C, uint8_t A);

/**
 * @brief Assembles the I frame and sends it to the given file descriptor. Handles stuffing, head and tail building.
 * 
 * @param fd     The file descriptor.
 * @param c_num  I Frame control field. Can either be C_I(0) or C_I(1).
 * @param info   Information to be transferred, not yet stuffed.
 * @param info_length   Length of information (number of bytes/octets)
 * @return int   Number of bytes written (after stuffing).
 */
int sendICommand(int fd, int c_num, uint8_t *info, unsigned int info_length);

/**
 * @brief Reads the I frame from the file descriptor and retrieves the message it carries. Handles destuffing, tail reception and validation.
 * 
 * @param fd     The file descriptor.
 * @param buffer Where the message will be stored (unstuffed).
 * @return int   Number of bytes read (after destuffing), or DATA_ERROR if the data is not valid.
 */
int readIFrame(int fd, uint8_t *buffer);


/**
 * @brief Assembles I frame's Data BCC, obtained through the exclusive OR of all the data's octets.
 * 
 * @param info         The Data.
 * @param info_length  Data's length, in bytes.
 * @return uint8_t     Octet obtained through the XOR of all data's octets
 */
uint8_t dataBCC(uint8_t *info, unsigned int info_length);

/**
 * @brief Receives a message and builds the corresponding stuffed message
 * 
 * @param msg         The Data.
 * @param msg_length  Data's length, in bytes.
 * @param stuffed_msg Where the stuffed message will be stored.
 * @return int        Size of stuffed message.
 */
int byteStuffing(uint8_t *msg, unsigned int msg_length, uint8_t *stuffed_msg);

/**
 * @brief Receives a stuffed message and rebuilds the original message
 * 
 * @param stuffed_msg Stuffed message.
 * @param msg_length  Data's length, in bytes. 
 * @param destuff     Where the original message will be stored
 * @return int        Size of original message.
 */
int destuffing(uint8_t *stuffed_msg, unsigned int msg_length, uint8_t *destuff);


/**
 * @brief Reads from the file descriptor until it receives the ending flag or exceeds the message max size. Handles the 
 * retrieval of the I frame's half after the tail (data + head) for ulterior processing.
 * 
 * @param fd          The file descriptor.
 * @param buffer      Where the received data will be stored.
 * @return int        Number of bytes read, a negative value if it fails or exceeds the max size.
 */
int getFrame(int fd, uint8_t *buffer);

/**
 * @brief Compares the received Data BCC with a BCC calculated with the received data to check if there were transmission errors.
 * 
 * @param msg     The Data.
 * @param length  Data's length, in bytes.
 * @param BCC     BCC read from the serial port, used in error checking.
 * @return int    0 if the BCCs differ (due to a transmission error), other value if they're the same.
 */
int checkBCC(uint8_t *msg, unsigned length, uint8_t BCC);

/**
 * @brief Opens the serial port and sets the signal handler, then attempts to establish the connection.
 * a) type == TRANSMITTER
 * 
 * Sends the SET and waits for the UA reply. When SET is sent, an alarm is called, and if the reply is not received
 * within TIMEOUT seconds it resends the SET command. It keeps re-sending until it receives the reply or has attempted 
 * MAX_RETR times.
 * 
 * b) type == RECEIVER
 * Awaits the SET command and sends the UA reply when it receives it. 
 * 
 * @param port    The Port to be opened.
 * @param type    App type indentifier (TRANSMITTER or RECEIVER).
 * @return int    Returns a file descriptor if it sucessfully establishes the connection, 0 if there's an error when acessing the serial
 * port.
 * a) returns TIMEOUT_RET if it times out while sending the SET command
 */
int llopen(char *port, int type);

/**
 * @brief Sends a message through the serial port. Uses Ns to synchronize with the receiver.
 * Sends the I frame and waits for the reply. When SET is sent, an alarm is called, and if the reply is not received
 * within TIMEOUT seconds it resends the SET command. It keeps re-sending until it receives the reply or has attempted 
 * MAX_RETR times.
 * @param fd      File descriptor.
 * @param buffer  Message to be sent through the serial port.
 * @param length  Message's size.
 * @return int    Returns the number of bytes written on a sucessful transmittion, RESEND_RET if the same data has to be sent again
 * or TIMEOUT_RET if it times out.
 */
int llwrite(int fd, uint8_t *buffer, int length);

/**
 * @brief Receives a message from the serial port. Uses Nr to synchronize with the transmitter.
 * Receives the I frame. If the data is ok and carries the expected Ns then replies with an RR reply;
 * if the data is a duplicate (the Ns is different than expected) it also sends the RR reply.
 * If the data has errors it sends the REJ reply.
 * @param fd      File descriptor.
 * @param buffer  Where the received message will be stored.
 * @param length  Message's size.
 * @return int    Returns the number of bytes written on a sucessful transmittion, 
 * DISCARD if the data has errors or is a duplicate.
 */
int llread(int fd, uint8_t *buffer);

/**
 * @brief Attempts to terminate the connection.
 * a) type == TRANSMITTER
 * 
 * Sends the DISC command and waits for the DISC reply. When DISC is sent, an alarm is called, and if the reply is not received
 * within TIMEOUT seconds it resends the DISC command. It keeps re-sending until it receives the reply or has attempted 
 * MAX_RETR times. If it receives the DISC reply, it sends an UA reply to the receiver.
 * 
 * b) type == RECEIVER
 * Awaits the DISC command and sends the DISC reply when it receives it. 
 * It then waits for the UA reply. When DISC is sent, an alarm is called, and if the reply is not received
 * within TIMEOUT seconds it resends DISC. It keeps re-sending until it receives the reply or has attempted 
 * MAX_RETR times.
 * @param port    The Port to be opened.
 * @param type    App type indentifier (TRANSMITTER or RECEIVER).
 * @return int    Returns 1 if sucessful, TIMEOUT_RET if it times out.
 */
int llclose(int fd, int type);

/**
 * @brief Reads the serial until either a control frame or I frame's header with the specified A and C fields has been found.
 * It uses a state machine to process the received bytes.
 * It can search for two C's at the same time, testing each one in a different state machine.
 * @param fd      File descriptor.
 * @param c       Desired control field.
 * @param a       Desired adress field.
 * @param c2      Another desired control field, used when there are two possible values for C.
 * @param headerI Determines if it's looking for a control frame or an I frame's header. If TRUE, it stops at the BCC,
 * otherwise looks for the end flag.
 * @return int    Returns 1 if it found the desired sequence, while using c; 2 if it did so while using c2;
 * FALSE if it didn't find anything when the alarm was called, -1 if there was an error reading.
 */
int serialReadControl2(int fd, uint8_t c, uint8_t a, uint8_t c2, int headerI);

/** 
 * @brief serialReadControl version which can only search for a control frame and test a single C octet.
 */
int serialReadControl1(int fd, uint8_t c, uint8_t a);

/** 
 * @brief initializes the port
 * */
void initPort(int fd);

#endif 
