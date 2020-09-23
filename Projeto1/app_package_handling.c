
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "app_package_handling.h"

#define FALSE 0
#define TRUE 1

void stateMachineApp(enum appState *state)
{

    switch (*state)
    {
    case START:
        *state = DATA;
        break;
    case DATA:
        *state = END;
        break;
    case END:
        *state = STOP_;
        break;
    default:
        break;
    }
}

unsigned int writeTlv(uint8_t type, uint8_t length, uint8_t *value, uint8_t *dest)
{

    dest[0] = type;
    dest[1] = length;
    memcpy(dest + 2, value, length);
    return length + 2;
}

unsigned int buildControlPackage(uint8_t control_field, unsigned int file_size, uint8_t *file_name, int file_name_length, uint8_t *dest)
{

    dest[0] = control_field;

    unsigned int total_package_size = 1;

    //tlv file size
    //printf("SIZE OF FILE SIZE - %x\n", sizeof(file_size));
    total_package_size += writeTlv(T_SIZE, sizeof(file_size), (uint8_t *) &file_size, dest + total_package_size);
    //printf("FILE SIZE - %x\n", file_size);
    //tlv file name
    total_package_size += writeTlv(T_NAME, file_name_length + 1, file_name, dest + total_package_size);

    // for (int i = 0; i < total_package_size; i++){
    //     printf("%x ", dest[i]);
    // }
    // printf("\n");

    return total_package_size;
}

unsigned int buildDataPackage(unsigned int data_size, uint8_t *data, uint8_t *dest)
{
    static uint8_t sequence_number_N = 0;

    dest[0] = C_DATA;
    dest[1] = sequence_number_N;

    uint8_t l1 = data_size % 256;
    uint8_t l2 = (data_size - l1) / 256;

    dest[2] = l2;
    dest[3] = l1;

    

    memcpy(dest + PACKAGE_H_SIZE, data, data_size);


    sequence_number_N = (sequence_number_N + 1) % 256; //update sequence number

    return PACKAGE_H_SIZE + data_size;
}

int isEndPackage(uint8_t *package, uint8_t *start_package, unsigned int start_package_length)
{
    for (unsigned int i = 1; i < start_package_length; i++)
    {
        // printf("pack: %x vs start pack: %x\n", package[i], start_package[i]);
        if (package[i] != start_package[i])
            return FALSE;
    }
    return TRUE;
}


int readStartPackage(uint8_t *package, unsigned int package_size, uint8_t *file_size, uint8_t *file_name)
{
    // printf("C = %x = %x?\n", package[0], C_START);
    if (package[0] != C_START)
        return -1;
    
    //printf("package size - %d\n", package_size);

    unsigned int curr_pack_byte = 1;
    while (curr_pack_byte < package_size)
    {
        uint8_t type = package[curr_pack_byte++];
        //printf("%x | ", package[curr_pack_byte - 1]);
        uint8_t length = package[curr_pack_byte++];
        //printf("%x | ", package[curr_pack_byte - 1]);

        for (unsigned i = 0; i < length; i++)
        {
            if (type == T_SIZE)
                file_size[i] = package[curr_pack_byte++];
            else if (type == T_NAME)
                file_name[i] = package[curr_pack_byte++];
            //printf("%x | ", package[curr_pack_byte - 1]);
        }
        //printf("\n");
    }
    //printf("\n");
    return curr_pack_byte;
}

int readDataPackage(uint8_t *package, uint8_t *dest, uint8_t expected_N){

    uint8_t read_sequence_N = package[1];
    printf("Received Sequence N: %d\n", read_sequence_N);

    if (expected_N != read_sequence_N){
        return -1;
    }

    uint8_t l2 = package[2];
    uint8_t l1 = package[3];
    int data_length = 256 * l2 + l1;
    for (int i = 0; i < data_length; i++)
    {
        dest[i] = package[i + 4];
        // printf("%x", dest[i]);
    }
    // printf("\n");
    
    

    return data_length;
}

int readPackage(uint8_t *package, uint8_t *dest, uint8_t expected_N, uint8_t *start_package, unsigned int start_package_length)
{

    if (package[0] == C_DATA)
    {
        printf("Received Data Package\n");
        return readDataPackage(package, dest, expected_N);
    }
    else if (package[0] == C_END)
    {
        if (isEndPackage(package, start_package, start_package_length))
        {
            printf("Received Valid End Package\n");
            return VALID_END_PACKAGE;
        }
        else
        {
            printf("Received Invalid End Package\n");
            return INVALID_END_PACKAGE;
        }
    }
    printf("Received Invalid Package\n");
    return INVALID_PACKAGE;
}

void display_completion(unsigned int file_size, unsigned int curr_size){

    float file_in_packages = (file_size / ((unsigned ) PACKAGE_DATA_SIZE));
    int percentage = ((curr_size / file_in_packages) * 100);
    int squares = percentage / 5;
    printf("%d%% completed => [ ", percentage);
    for (int i = 0; i < 20; i++){
        if (i < squares)
           printf("+ ");
        else printf("- ");
    }
    printf("]\n");
}