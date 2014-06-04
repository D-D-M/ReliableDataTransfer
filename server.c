#include <stdio.h> // Printing
#include <time.h> // To seed the random number generator
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
/*
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

    // Test the buffer
    printf("Buffer contains:\n");
    printf("%s\n-------------------------------\n\n", buffer);

    return file_length;
}
*/
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
    srand48(time(NULL)); // Seed RNG

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
    long file_length = 0;
    int base = 0;
    int seq_num = 0;
    int end_wnd = cwnd - 1; // The end of the current "window"

    // Need a loop counter perhaps
    while (1)
    {
        // RECEIVE request from client. 
        // ------------------------------------
        struct srpacket recv_data;
        memset(&recv_data, 0, sizeof(struct srpacket)); // Zero out the packet
        // recv_data.length = PACKETSIZE; // Initialize recv_data to be the full packet size, to be shrunk later
        // recv_data.sequence = 0;
        // printf("Attempting to receive packet from client.\n");
        // bytes_read = recvfrom(sockfd, &recv_data, recv_data.length+p_header_size(), 0,
        bytes_read = recvfrom(sockfd, &recv_data, sizeof(struct srpacket), 0,
                            (struct sockaddr *)&cli_addr, &addr_size);
        // Convert to host byte order
        /*
        recv_data.sequence = ntohl(recv_data.sequence);
        recv_data.length = ntohl(recv_data.length);
        recv_data.type = ntohl(recv_data.type);
        */
        // recv_data.data[bytes_read] = '\0';
        // printf("Packet received, number of bytes read = %d\n", bytes_read);
        print_packet_info_server(&recv_data, CLIENT); // CLIENT is the one who sent this packet
        // printf("(%s, %d) said : ", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        // printf("%s\n", recv_data.data);
    
        if (recv_data.type == REQUEST) // We need to check if the file exists and split it into packets
        {
            // OPEN THE FILE, LOAD IT INTO A BUFFER
            FILE *file;
            char filename[150];
            strcpy(filename, recv_data.data); // Get the file name
            // Zero-byte the last character of 'filename'???
            printf("Client requested the file %s. Fetching file...\n", filename);
            file = fopen(filename, "rb");
            if (!file)
            {
                printf("File \"%s\" does not exist.\n", filename); 
                // SEND A 'FIN' PACKET to the client
                // continue;
                exit(1);
            }
            fseek(file, 0, SEEK_END);
            file_length = ftell(file);
            rewind(file);
            // buffer = (char *)malloc((file_length+1)*sizeof(char));
            buffer = (char *)malloc((file_length)*sizeof(char));
            // memset(&buffer, 0, sizeof(char)*(file_length+1));
            if (!buffer)
            {
                printf("File \"%s\" too big to fit in memory.\n", filename);
                // SEND A 'FIN' PACKET to the client?
                // The client should exit(1), and the server should continue;
                // continue;
                exit(1);
            }
            fread(buffer, file_length, 1, file);
            fclose(file);
            // char* filename; // Doesn't work, accessing uninitialized memory causes segfault
            // char* filename = (char *)malloc(sizeof(char)* (recv_data.length - p_header_size()));
            // file_length = fill_buffer_from_file(filename, buffer); // Check existence, fill the buffer.
            printf("File is now in the buffer, size %ld bytes.\n", file_length);

            // Figure out how many packets we will need.
            numpackets = ((file_length-1)/ PACKETSIZE) + 1; // Integer division
                                                        // We want 512 bytes to be 1 packet
            printf("This file will be divided into %d packets.\n", numpackets);
            /*
            // Test the buffer again
            printf("NOW Buffer contains:\n");
            printf("%s\n-------------------------------\n\n", buffer);
            */
            // (512-1)/(512) + 1 = 1
        }
        printf("Number of remaining packets to send: %d\n", numpackets);
        if (numpackets > 0)
        {
            // NOTES FOR FUTURE DAVID:
            // The sender is going to need a buffer for the last N packets sent,
            // in case those packets need to be re-sent.
            
            // if (recv_data.sequence > base)
                // --------------------------------------------
                // Construct packet i from the buffer
                // --------------------------------------------
                // printf("Constructing packet with sequence #%d\n", seq_num);
                memset(&send_data, 0, sizeof(struct srpacket)); 
                // printf("Memset worked\n");
                // construct_next_packet_data(seq_num, buffer, &send_data);
                send_data.type = DATA;
                send_data.sequence = seq_num;
                numpackets--;

                if (numpackets == 0)    // If we're sending the last packet, only send the remaining bytes.
                    send_data.length = (file_length % PACKETSIZE);
                else                    // Otherwise send the full packet size
                    send_data.length = PACKETSIZE; 
                // Put the data into the packet's data field
                // printf("Attempting memcpy...\n");
                // printf("packlen is %d\n", send_data.length);
                // printf("seqnum is %d\n", seq_num);
                // printf("packsz is %d\n", PACKETSIZE);
                int buffadd = seq_num*PACKETSIZE;
                char* chunk = &buffer[buffadd];
                // printf("Data at buffer[%d] is %s !\n", buffadd, chunk);
                // memcpy(send_data.data, &buffer[seq_num*PACKETSIZE], send_data.length);
                memcpy(send_data.data, chunk, send_data.length); // ONLY copy send_data.length bytes
                // printf("Memcpy worked\n");
                // send_data.data[send_data.length] = '\0'; // Zero out the value just past the packet.data
                
                printf("Sending packet...\n");
                // send_data.length += p_header_size();
                send_data.corrupt = set_packet_corruption(p_corr);
                // Convert to network byte order
                /*
                send_data.type = htonl(send_data.type);
                send_data.sequence = htonl(send_data.sequence);
                send_data.length = htonl(send_data.length);
                */
                // Send
                // sendto(sockfd, &send_data, send_data.length, 0, 
                sendto(sockfd, &send_data, sizeof(struct srpacket), 0, 
                           (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
                print_packet_info_server(&send_data, SERVER);
                // printf("Packet sent.\n");
                // printf("-------------------------------\n");
                
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
        else
        {
            printf("File transfer complete.\n");
            // SEND A 'FIN' PACKET
            return 0;
        }
        // return 0;
    }
    free(buffer);

    // The client will request a file. Check if that file exists. If not, error.
    // If so, divide that file up into packets.


    // Send the packets to the client one by one. 

    return 0;
}
