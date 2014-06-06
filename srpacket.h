/*
 * This is the packet structure for receiver and sender
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#define PACKETSIZE 1024
typedef enum
{
    REQUEST,
    DATA,
    ACK,
    FIN
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
    int total_length; // Total length of the file this DATA packet belongs to.
    // char data[PACKETSIZE+1]; // +1 for the zero byte \0
    char data[PACKETSIZE];
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
        case 3:
            return "FIN";
    }
    return "UNKNOWN";
}

int play_the_odds(const double p_corr, int *counter)
{
    double legit = drand48(); // Generates a random double between 0.0 and 1.0, inclusive. 
    if (legit >= p_corr)
    {
        // printf("Legit had a value of %f, which is GREATER than %f\n", legit, p_corr);
        return 0;
    }
    else
    {
        // printf("Legit had a value of %f, which is LESS than %f\n", legit, p_corr);
        *counter = *counter + 1;
        return 1;
    }
}

void print_packet_info_server(const struct srpacket *pac, sender_t stype)
{
    // UNCOMMENT this for full packet info
    /*
    printf("==========================================\n");
    printf("PACKET ");
    if (stype == SERVER)
        printf("outbound to CLIENT\n");
    else
        printf("incoming from CLIENT\n");
    printf("Header------------------------------------\n");
    printf("------------------------------------------\n");
    printf("    Type: %s\n", get_packet_type(pac->type));
    // printf("    Sequence Number: %d\n", pac->sequence*(int)PACKETSIZE);
    printf("    Sequence Number: %d\n", pac->sequence);
    printf("    Corrupt? %s\n", pac->corrupt ? "YES" : "NO");
    printf("    Length: %d bytes\n", pac->length);
    printf("Data--------------------------------------\n");
    printf("------------------------------------------\n");
    printf("%s\n",pac->data);
    printf("\n==========================================\n");
    */
    if (stype == SERVER)
    {
        printf("\nSENDING ==============> SEQ %d", pac->sequence);
        if (pac->length < PACKETSIZE && pac->length > 0)
            printf(" (size %d bytes)", pac->length); // Only print byte size if it's not a full/emtpy packet
        if (pac->corrupt)
            printf(" (CORRUPT)");

        printf("\n\n");
    }
    else
    {
        printf("\nRECEIVING <============ ACK %d", pac->sequence);
        // if (pac->length < PACKETSIZE && pac->length > 0)
        //     printf(" (size %d bytes)"); // Only print byte size if it's not a full/emtpy packet
        if (pac->corrupt)
            printf(" (CORRUPT)");
        
        printf("\n\n");
    }

    return;
}
void print_packet_info_client(const struct srpacket *pac, sender_t stype)
{
    // UNCOMMENT this for full packet info
    /*
    printf("==========================================\n");
    printf("PACKET ");
    if (stype == SERVER)
        printf("incoming from SERVER\n");
    else
        printf("outbound to SERVER\n");
    printf("Header------------------------------------\n");
    printf("------------------------------------------\n");
    printf("    Type: %s\n", get_packet_type(pac->type));
    // printf("    Sequence Number: %d\n", pac->sequence*(int)PACKETSIZE);
    printf("    Sequence Number: %d\n", pac->sequence);
    printf("    Corrupt? %s\n", pac->corrupt ? "YES" : "NO");
    printf("    Length: %d bytes\n", pac->length);
    printf("Data--------------------------------------\n");
    printf("------------------------------------------\n");
    printf("%s\n",pac->data);
    printf("\n==========================================\n");
    */
    if (stype == SERVER)
    {
        printf("\nRECEIVING <============ SEQ %d", pac->sequence);
        if (pac->length < PACKETSIZE && pac->length > 0)
            printf(" (size %d bytes)", pac->length); // Only print byte size if it's not a full/emtpy packet
        if (pac->corrupt)
            printf(" (corrupt)");
        
        printf("\n\n");
    }
    else
    { 
        printf("\nSENDING ==============> ACK %d", pac->sequence);
        // if (pac->length < PACKETSIZE && pac->length > 0)
        //     printf(" (size %d bytes)"); // Only print byte size if it's not a full/emtpy packet
        if (pac->corrupt)
            printf(" (corrupt)");
        
        printf("\n\n");
    }

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



