/*
 * This is the packet structure for receiver and sender
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#define PACKETSIZE 512
typedef enum
{
    REQUEST,
    DATA,
    ACK
} packet_t;

typedef enum
{
    SERVER,
    CLIENT
} sender_t;

struct srpacket
{
    packet_t type; // Request? ACK? NAK?
    int sequence; // Packet sequence number
    int corrupt;
    int length;
    char data[PACKETSIZE+1]; // +1 for the zero byte \0
};

char* get_packet_type(packet_t ptype)
{
    switch (ptype)
    {
        case 0:
            return "REQUEST";
        case 1:
            return "DATA";
        case 2:
            return "ACK";
    }
    return "UNKNOWN";
}
void print_with_indent(const int indent, const char* string)
{
    printf("%*s" "%s", indent, " ", string);
    return;
}

int set_packet_corruption(const double p_corr)
{
    time_t t;
    srand((unsigned) time(&t)); // Seed the random number generator based on the current time.
    double legit = drand48(); // Generates a random double between 0.0 and 1.0, inclusive. 
    if (legit >= p_corr)
    {
        printf("Legit had a value of %f, which is GREATER than %f\n", legit, p_corr);
        return 0;
    }
    else
    {
        printf("Legit had a value of %f, which is LESS than %f\n", legit, p_corr);
        return 1;
    }
}

void print_packet_info_server(const struct srpacket *pac, sender_t stype)
{
    // TO-DO: Fill this out
    printf("==========================================\n");
    printf("PACKET ");
    if (stype == SERVER)
        printf("outbound to CLIENT\n");
    else
        printf("incoming from CLIENT\n");
    printf("Header------------------------------------\n");
    printf("------------------------------------------\n");
    printf("    Type: %s\n", get_packet_type(pac->type));
    printf("    Sequence Number: %d\n", pac->sequence);
    printf("    Corrupt? %s\n", pac->corrupt ? "YES" : "NO");
    printf("    Length: %d bytes\n", pac->length);
    printf("Data--------------------------------------\n");
    printf("------------------------------------------\n");
    printf("%s\n",pac->data);
    printf("\n==========================================\n");
    return;
}
void print_packet_info_client(const struct srpacket *pac, sender_t stype)
{
    // TO-DO: Fill this out
    printf("==========================================\n");
    printf("PACKET ");
    if (stype == SERVER)
        printf("incoming from SERVER\n");
    else
        printf("outbound to SERVER\n");
    printf("Header------------------------------------\n");
    printf("------------------------------------------\n");
    printf("    Type: %s\n", get_packet_type(pac->type));
    printf("    Sequence Number: %d\n", pac->sequence);
    printf("    Corrupt? %s\n", pac->corrupt ? "YES" : "NO");
    printf("    Length: %d bytes\n", pac->length);
    printf("Data--------------------------------------\n");
    printf("------------------------------------------\n");
    printf("%s\n",pac->data);
    printf("\n==========================================\n");
    return;
}

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



