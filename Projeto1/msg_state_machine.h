/* State Machine for messages communications */

#ifndef MSG_STATE_MACHINE_H
#define MSG_STATE_MACHINE_H

#include <stdint.h>

/**/

#define F_FLAG 0x7E

/* 00000011 (0x03) em Comandos enviados pelo Emissor e Respostas enviadas pelo Receptor */

#define A1_ 0X03

/* 00000001 (0x01) em Comandos enviados pelo Receptor e Respostas enviadas pelo Emissor */

#define A2_ 0X01

//comandos, n = 0/1
#define C_SET 0x03
#define C_I(n)   ((n) << (6))
#define C_DISC 0x0B

//respostas, n = 0/1
#define C_UA 0x06
#define C_RR(n)    (((n) << (7)) | 0x05)
#define C_REJ(n)   (((n) << (7)) | 0x01)


//stuffing
#define ESC_OCT     0x7D
#define FLAG_STUFF  0x5E
#define ESC_STUFF   0x5D
/**/

//
#define BCC1ERR 0
#define BCC2ERR 0




enum state {START_S, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP_S};

void stateMachineOpen(enum state *state, uint8_t *received_char, uint8_t c, uint8_t a, int headerI);

int hasError(int error_chance);


#endif //MSG_STATE_MACHINE_H
