/* State Machine for messages communications */

#include "msg_state_machine.h"
#include <stdio.h>
#include <stdlib.h>

#define FALSE 0
#define TRUE 1

int hasError(int error_chance)
{
	int has_error = 0;
	if (error_chance > 0)
	{
		int random_number = (rand() % 100) + 1;
		printf("RANDOM NUMBER - %d\n", random_number);
		has_error = (error_chance >= random_number);
		printf("ERRORI - %d\n", has_error);
	}
	return has_error;
}

uint8_t get_bcc(uint8_t A, uint8_t C, int headerI)
{
	if (headerI)
	{
		if (hasError(BCC1ERR))
			return A & C;
	}
	return A ^ C;
}

void stateMachineOpen(enum state *state, uint8_t *received_char, uint8_t c, uint8_t a, int headerI){

	switch (*state){
		//flag
		case START_S:
			if (*received_char == F_FLAG)
				*state = FLAG_RCV;
			//printf("START - RCV = %x\n", *received_char);
			break;
		case FLAG_RCV:
			if (*received_char == a)
				*state = A_RCV;
			else if (*received_char != F_FLAG)
				*state = START_S;
			//printf("FLAG - RCV = %x\n", *received_char);
			break;
		case A_RCV:
			if (*received_char == c)
				*state = C_RCV;
			else if (*received_char == F_FLAG)
				*state = 1;
			else *state = START_S;
			//printf("A - RCV = %x\n", *received_char);
			break;
		case C_RCV:
			if (*received_char == get_bcc(a, c, headerI))
			{
				if (headerI != TRUE)
					*state = BCC_OK; //if it is a control frame, the BCC is not the last octet
				else
					*state = STOP_S; //if it is an I header, the BCC is the last octet
			}
			else if (*received_char == F_FLAG)
				*state = FLAG_RCV;
			else
				*state = START_S;
			//printf("C - RCV = %x\n", *received_char);
			break;
		case BCC_OK:
			if (*received_char == F_FLAG)
				*state = STOP_S;
			else *state = START_S;
			//printf("BCC - RCV = %x\n", *received_char);
			break;
		default:
			break;

	}

}
