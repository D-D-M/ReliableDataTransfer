#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "gbnpacket.h"

int main (int argc, char *argv[])
{
    // double legit;
    srand48(time(NULL)); // Seed RNG
    char *buffer = NULL; // A buffer for the received data
    long bufsize = 0;
    FILE* outfile;

    if (argc != 6)
    {
        printf("Arg count was %d.\n", argc);
        usage_err();
    }

    // For statistical purposes
    int corrupt_count = 0;
    int loss_count = 0;

    int sockfd, portnum, bytes_recv, bytes_sent;
    int lost_packet = 0;

    socklen_t sin_size;

    struct sockaddr_in srv_addr;
    struct hostent *host;
    
    // Send data
    struct gbnpacket send_data;
    // Receive data
    struct gbnpacket recv_data;
    // recv_data.length = p_size();
    // recv_data.sequence = 0;

    // Parse command line arguments
    host = (struct hostent *) gethostbyname(argv[1]);
    portnum = atoi(argv[2]);
    char* filename = argv[3];
    printf("Filename is %s\n", filename);
    double p_loss = atof(argv[4]); // Probability of packet loss
    double p_corr = atof(argv[5]); // Probability of packet corruption

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error_die("Error opening socket.\n");
    
    // Initialize server address
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(portnum);
    srv_addr.sin_addr = *((struct in_addr *)host->h_addr);
    bzero(&(srv_addr.sin_zero),8);
    sin_size = sizeof(struct sockaddr);

    //------------------------------------
    // SEND request for file
    //------------------------------------
    memset(&send_data, 0, sizeof(struct gbnpacket));
    send_data.type = REQUEST;
    send_data.sequence = 0;
    send_data.corrupt = 0;
    strcpy(send_data.data, filename); // Might have to \0 out the last byte of data...not sure yet
    // send_data.data[strlen(filename)] = '\0';
    // send_data.length = p_header_size() + strlen(filename) + 1; // +1 for the null byte?
    send_data.length = strlen(filename);
    printf("Sending request to CS118 for %s\n", filename);
    // Convert to network byte order
    /*
    send_data.type = htonl(send_data.type);
    send_data.sequence = htonl(send_data.sequence);
    send_data.length = htonl(send_data.length);
    */
    long expecting_packet = 0; // The sequence number of the expected packet
    while (1)
    {
        if (send_data.type == REQUEST)
        {
            // Send
            // bytes_sent = sendto(sockfd, &send_data, send_data.length, 0,
            bytes_sent = sendto(sockfd, &send_data, sizeof(struct gbnpacket), 0,
                            (struct sockaddr *)&srv_addr, sizeof(struct sockaddr));
                        // TO-DO: Check to see if bytes_sent matches up with the length of the packet in total.
            // buffer = NULL;
            print_packet_info_client(&send_data, CLIENT);
            // printf("Request sent.\n");
        }

        // --------------------------------
        // RECEIVE data response
        // --------------------------------
        printf("Waiting for response from CS118.\n");
        // bytes_recv = recvfrom(sockfd, &recv_data, recv_data.length, 0,
        bytes_recv = recvfrom(sockfd, &recv_data, sizeof(struct gbnpacket), 0, 
                        (struct sockaddr*)&srv_addr, &sin_size);
        lost_packet = play_the_odds(p_loss, &loss_count);
        if (lost_packet)
        {
            printf("\nLOST PACKET!\n\n");
            continue; // Do nothing else, just receive the packet and move on.
        }
        // Convert to host byte order
        /*
        recv_data.type = ntohl(recv_data.type);
        recv_data.sequence = ntohl(recv_data.sequence);
        recv_data.length = ntohl(recv_data.length);
        */
        // TO-DO: CHECK TO MAKE SURE THAT THE PROGRAM DOESN'T SEGFAULT IF recvfrom() FAILS! i.e. if bytesrecv == 0
        // recv_data.data[bytes_recv] = '\0';
        print_packet_info_client(&recv_data, SERVER);
        // printf("Received : %s\n", recv_data.data);

        // --------------------------------
        // PREPARE acknowledgement
        // --------------------------------
        if (recv_data.type == FIN)
        {
            // Send a FIN in return, and end the client process
            char *msg = "Terminating connection.";
            memset(&send_data, 0, sizeof(struct gbnpacket));
            send_data.type = FIN;
            send_data.sequence = 0;
            send_data.corrupt = 0; // set_packet_corruption();
            send_data.length = 0;
            // Don't need to initialize the data because of the memset
            printf("%s. Sending FIN packet.\n", msg);
            printf("STATS: Sent %d corrupt ACKs. Treated %d packets as \'lost\'.\n",
                            corrupt_count, loss_count);
            bytes_sent = sendto(sockfd, &send_data, sizeof(struct gbnpacket), 0, 
                            (struct sockaddr *)&srv_addr, sizeof(struct sockaddr));

            // Modify the filename
            strcat(filename, ".received");
            outfile = fopen(filename, "wb");
            if (outfile)
            {
                fwrite(buffer, bufsize, 1, outfile);
                printf("Successfully wrote buffer to file \"%s\"\n.", filename);
            }
            else
                error_die("Error writing buffer to file.");

            return 0; // End the client process
        }
        if (!buffer)
        {
            bufsize = sizeof(char) * recv_data.total_length;
            buffer = (char *)malloc(bufsize);
            if (!buffer)
                error_die("Not enough space for client file buffer.");
        }
        memset(&send_data, 0, sizeof(struct gbnpacket));
        send_data.type = ACK;
        // strcpy(send_data.data, "Acknowledged.");
        // send_data.data[strlen(send_data.data)] = '\0'; // Zero byte
        // send_data.length = p_header_size() + strlen(send_data.data) + 1; // Might need +1 for zero byte
        send_data.length = 0; // ACKs have no data
        // send_data.corrupt = 0;
        // send_data.corrupt = set_packet_corruption(p_corr);
        // ----------------------------------------------------------------
        // SEND acknowledgement, depending on the last packet received.
        // ----------------------------------------------------------------
        // Packet is legit
        if (recv_data.corrupt == 0 && recv_data.sequence <= expecting_packet) // Use <= for duplicate packets
        {
            // Copy file data into the buffer
            memcpy(buffer+recv_data.sequence, recv_data.data, recv_data.length);
            // send_data.sequence = recv_data.sequence + (bytes_recv - p_header_size());
            send_data.sequence = recv_data.sequence + PACKETSIZE;
            send_data.corrupt = play_the_odds(p_corr, &corrupt_count);

            // Convert to network byte order
            /*
            send_data.type = htonl(send_data.type);
            send_data.sequence = htonl(send_data.sequence);
            send_data.length = htonl(send_data.length);
            */
            // expecting_packet = recv_data.sequence + PACKETSIZE;
            expecting_packet = send_data.sequence;
            // Send
            // printf("Sending acknowledgement from else...\n");
            // sendto(sockfd, &send_data, send_data.length, 0, 

//            if (legit >= p_loss)
//            {
            sendto(sockfd, &send_data, sizeof(struct gbnpacket), 0, 
                    (struct sockaddr *)&srv_addr, sizeof(struct sockaddr));
            print_packet_info_client(&send_data, CLIENT);
//            }
        }
        // else if (recv_data.corrupt == 0 && recv_data.sequence < expecting_packet)
        // {
        //     printf("\nDUPLICATE PACKET, IGNORING!\n\n");
        //     continue; // DUPLICATE PACKET
        // }
        // if (recv_data.sequence != expecting_packet || recv_data.corrupt == 1) // Packet received OUT OF ORDER or is CORRUPT
        else // Packet is not legit
        {
            // Be sure to send duplicate ACKs? Not in the book
            // The ACK should be for the last correctly-received packet.
            send_data.sequence = expecting_packet;
            send_data.corrupt = play_the_odds(p_corr, &corrupt_count);
            // Send
            // printf("Sending acknowledgement from if...\n");
            // sendto(sockfd, &send_data, send_data.length, 0, 
//            if (legit >= p_loss)
//            {
            sendto(sockfd, &send_data, sizeof(struct gbnpacket), 0, 
                        (struct sockaddr *)&srv_addr, sizeof(struct sockaddr));
            print_packet_info_client(&send_data, CLIENT);
//            }

        }


    }
    

    return 0;
}
