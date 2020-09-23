#ifndef APP_PACKAGE_HANDLING_H
#define APP_PACKAGE_HANDLING_H

#include <stdint.h>

//C
#define C_DATA     0x01
#define C_START    0x02
#define C_END      0x03

//control TLV
#define T_SIZE     0x00
#define T_NAME     0x01


//TODO: IR BUSCAR A LLFUNCTION.H
#define PACK_MAX_SIZE  255
#define PACKAGE_H_SIZE 4
#define PACKAGE_DATA_SIZE PACK_MAX_SIZE - PACKAGE_H_SIZE

//end package returns
#define INVALID_END_PACKAGE -1
#define VALID_END_PACKAGE   -2
#define INVALID_PACKAGE     -3

//file
#define FILE_NAME_MAX_SIZE     255
#define EXTENSION_MAX_SIZE     16
#define ORIGINAL_FILE_MAX_SIZE PACK_MAX_SIZE - 10

//START: start of file data sending, DATA: sending file data, END: Ending Program, STOP: Program has ended.
enum appState {START, DATA, END, STOP_};

/**
 * @brief Changes the state to the next
 */
void stateMachineApp(enum appState *state);

/**
 * @brief Builds a TLV format message 
 * @param type     T.
 * @param length   L.
 * @param value    V.
 * @param dest     Where the TLV message will be stored.
 * @return int     Size of tlv message (in bytes).
 */
unsigned int writeTlv(uint8_t type, uint8_t length, uint8_t *value, uint8_t *dest);

/**
 * @brief Builds a Control Package, which is made of a Control Field and
 * two TLV messages, containing the file size and file name
 * @param dest     Where the Control Package will be stored.
 * @return int     Size of Control Package (in bytes).
 */
unsigned int buildControlPackage(uint8_t control_field, unsigned int file_size, uint8_t *file_name, int file_name_length, uint8_t *dest);

/**
 * @brief Builds a Data Package, which is made of a Control Field, a sequence number (N),
 * two octets which contain the data's size, (L1 and L2), and the data.
 * @param dest     Where the Data Package will be stored.
 * @return int     Size of Data Package (in bytes).
 */
unsigned int buildDataPackage(unsigned int data_size, uint8_t *data, uint8_t *dest);

/**
 * @brief Checks if the received end_package is valid (has the same TLV data as the start_package)
 * @return int     TRUE if is valid, FALSE if it isn't.
 */
int isEndPackage(uint8_t *package, uint8_t *start_package, unsigned int start_package_length);

/**
 * @brief Reads the Start Package, checking the control field and retrieving the values sent in TLV format
 * @param package       Received Package.
 * @param package_size  Received Package's size (in bytes).
 * @param file_size     Where the file size will be stored.
 * @param file_name     Where the file name will be stored.
 * @return int          Positive value if it's a start package, negative otherwise
 */
int readStartPackage(uint8_t *package, unsigned int package_size, uint8_t *file_size, uint8_t *file_name);

int readDataPackage(uint8_t *package, uint8_t *dest, uint8_t expected_N);

int readPackage(uint8_t *package, uint8_t *dest, uint8_t expected_N, uint8_t *start_package, unsigned int start_package_length);

void display_completion(unsigned int file_size, unsigned int curr_size);


#endif