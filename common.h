#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>
#include <pthread.h>

int REG_REQ = 0;
int REG_INFO = 1;
int REG_ACK = 2;
int INFO_ACK = 3;
int REG_NACK = 4;
int INFO_NACK = 5;
int REG_REJ = 6;
int ALIVE = 16;
int ALIVE_REJ = 17; 

int DISCONNECTED = 160;
int NOT_REGISTERED = 161;
int WAIT_ACK_REG = 162;
int WAIT_INFO = 163;
int WAIT_ACK_INFO = 164;
int REGISTERED = 165;
int SEND_ALIVE = 166;

int generate_random(){
    return (rand() % (99999999 - 10000000 + 1)) + 10000000;
}

int generate_UDP_port(){
    return (rand() % (65000 - 1024 + 1)) + 1024;
}

struct client{
    int status;
    char id[13];
    char dispositius[16][8];
    struct sockaddr_in addr_UDP;
    int random;
    int TCP_port;
    int new_udp_port;
    int alive_recved;
    int alives_no_answer;
};

struct PDU_UDP{
    unsigned char tipus;
    char id[13];
    char aleatori[9];
    char dades[61];
};

struct PDU_TCP{
	unsigned char tipus;
	char id[13];
	char aleatori[9];
	char element[8];
	char valor[16];
	char info[80];
};
