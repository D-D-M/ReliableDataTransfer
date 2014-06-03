/*
 * This is the packet structure for receiver and sender
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define PACKETSIZE 512
typedef enum
{
    REQUEST,
    DATA,
    ACK
} packet_t;

struct srpacket
{
    packet_t type; // Request? ACK? NAK?
    int sequence; // Packet sequence number
    int length;
    char data[PACKETSIZE+1]; // +1 for the zero byte \0
};

int p_header_size()
{
    return sizeof(struct srpacket) - PACKETSIZE; // Minus the 512 bytes of data
}

int p_size()
{
    return sizeof(struct srpacket);
}


/*
 * Print out a system error message
 */
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

void usage_err()
{
    fprintf(stdout, "USAGE\n");
    fprintf(stdout, "---------------------------------------------------\n");
    fprintf(stdout, "Sender:   ./sender portnumber CWnd P_l P_c\n");
    fprintf(stdout, "Receiver: ./receiver sender_hostname sender_portnumber filename P_l P _c\n");
    fprintf(stdout, "---------------------------------------------------\n");
    exit(1);
}



