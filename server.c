#include <stdio.h> // Printing
#include <stdlib.h> // atoi
#include <string.h>
#include <sys/socket.h> // C sockets
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>

#include "srpacket.h"

long fill_buffer_from_file(const char *filename, char* buffer)
{
    FILE *file;
    long file_length;
    if ( !(file = fopen(filename, "rb")) )
    {
        printf("Requested file does not exist.\n", filename); 
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    file_length = ftell(file);
    rewind(file);

    if (!(buffer = (char *)malloc((file_length+1)*sizeof(char))))
    {
        printf("File is too big to fit in memory.\n", filename);
        exit(1);
    }
    fread(buffer, file_length, 1, file);
    fclose(file);

    return file_length;
}
/*
void construct_next_packet_data(int i, const char* buffer, struct srpacket *packet)
{
    // Create packet headers and such
    packet->type = DATA;
    packet->sequence = i;
    packet->length = 
    // Then fill the packet with data
    // min( (i+1)*PACKETSIZE , datalength)
    // between i+*PACKETSIZE and <that+packetlength
    for (i = i*PACKETSIZE; i < (i+1)*PACKETSIZE; i++)
    {
        // add data to the packet
        
    }
}
*/
int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Arg count was %d.\n", argc);
        usage_err();
    }
    // Parse command line arguments
    int portnum = atoi(argv[1]); // Make sure this is the right argument number
    int cwnd = atoi(argv[2]); // Window size
    double p_loss = atof(argv[3]); // Probability of packet loss
    double p_corr = atof(argv[4]); // Probability of corrupt packet

    int sockfd, bytes_read;
    char *buffer; // A buffer for the data of the served file.
    int numpackets = -1;

    // Send data: The packet to be sent to the client.
    struct srpacket send_data;
    send_data.sequence = 0;
        
    socklen_t addr_size;

    struct sockaddr_in srv_addr, cli_addr; // Server and client addresses

    // sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); // 0 means default protocol, which should be UDP anyway

    if (sockfd < 0)
        error_die("Error opening socket.\n");
    
    bzero((char *) &srv_addr, sizeof(srv_addr));

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(portnum);

    if (bind(sockfd, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0)
        error_die("Error binding socket.\n");
    
    addr_size = sizeof(struct sockaddr);
    printf("Waiting for client on port %d...\n", portnum);

    // Think about this in terms of packets...
    // uninitialized file = divided into packets
    long filelen;
    int base = 0;
    int seq_num = 0;
    int max = cwnd - 1;

    // Need a loop counter perhaps
    while (1)
    {
        // RECEIVE request from client. 
        // ------------------------------------
        struct srpacket recv_data;
        memset(&recv_data, 0, sizeof(struct srpacket)); // Zero out the packet
        recv_data.length = p_size(); // Initialize recv_data to be the full packet size, to be shrunk later
        recv_data.sequence = 0;

        printf("Attempting to receive packet from client.\n");
        
        bytes_read = recvfrom(sockfd, &recv_data, recv_data.length, 0,
                            (struct sockaddr *)&cli_addr, &addr_size);
        // Prepare for host byte order
        recv_data.sequence = ntohl(recv_data.sequence);
        recv_data.length = ntohl(recv_data.length);
        recv_data.type = ntohl(recv_data.type);

        recv_data.data[bytes_read] = '\0';

        printf("Packet received, number of bytes read = %d\n", bytes_read);
        printf("Packet type: %d\n", recv_data.type);
        printf("Packet data: %s\n", recv_data.data);
        printf("Packet seq #: %d\n", recv_data.sequence);

        printf("(%s, %d) said : ", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        printf("%s\n", recv_data.data);

        // What type of request is it? 
        // IF REQUEST, then open the file and load its bytes into the buffer.
        if (recv_data.type == REQUEST) // We need to check if the file exists and split it into packets
        {
            // char* filename; // Doesn't work, accessing uninitialized memory causes segfault
            // char* filename = (char *)malloc(sizeof(char)* (recv_data.length - p_header_size()));
            char filename[150];
            strcpy(filename, recv_data.data); // Get the file name
            printf("Client requested the file %s. Fetching file...\n", filename);
            filelen = fill_buffer_from_file(filename, buffer); // Check existence, fill the buffer.
            printf("File is now in the buffer, size %ld bytes.\n", filelen);

            // Figure out how many packets we will need.
            numpackets = ((filelen-1)/ PACKETSIZE) + 1; // Integer division
                                                        // We want 512 bytes to be 1 packet
                                                        // (512-1)/(512) + 1 = 1
        }
        // --------------------------------------------
        // Construct packet i from the buffer
        // --------------------------------------------
        memset(&send_data, 0, sizeof(struct srpacket)); 
        // construct_next_packet_data(seq_num, buffer, &send_data);
        send_data.type = DATA;
        send_data.sequence = seq_num;
        numpackets--;
        if (numpackets == 0)    // If we're sending the last packet, only send the remaining bytes.
            send_data.length = (filelen % PACKETSIZE);
        else                    // Otherwise send the full packet size
            send_data.length = PACKETSIZE; 
        // Put the data into the packet's data field
        memcpy(send_data.data, &buffer[seq_num*PACKETSIZE], send_data.length);
        send_data.data[send_data.length] = '\0'; // Zero out the value just past the packet.data
        
        printf("Sending packet...\n");
        send_data.length += p_header_size();
        // Prepare for network byte order
        send_data.type = htonl(send_data.type);
        send_data.sequence = htonl(send_data.sequence);
        send_data.length = htonl(send_data.length);
        // Send
        sendto(sockfd, &send_data, send_data.length, 0, 
                    (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
        printf("Packet sent.\n");
        printf("-------------------------------\n");
        
        seq_num++;
        // fflush(stdout);
        /*
        // Send the next packet to the client, if there is one.
        strcpy(send_data.data, "it works!"); // Just something random.
        send_data.length = p_header_size() + strlen(send_data.data) + 1;
        sendto(sockfd, &send_data, send_data.length, 0, 
                    (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
        printf("Packet sent.\n");
        fflush(stdout);
        */
    }
    free(buffer);

    // The client will request a file. Check if that file exists. If not, error.
    // If so, divide that file up into packets.


    // Send the packets to the client one by one. 

    return 0;
}
