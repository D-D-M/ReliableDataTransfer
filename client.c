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
#include "srpacket.h"

int main (int argc, char *argv[])
{
    srand48(time(NULL)); // Seed RNG
    if (argc != 6)
    {
        printf("Arg count was %d.\n", argc);
        usage_err();
    }

    int sockfd, portnum, bytes_recv, bytes_sent;

    socklen_t sin_size;

    struct sockaddr_in srv_addr;
    struct hostent *host;
    
    // Send data
    struct srpacket send_data;
    // Receive data
    struct srpacket recv_data;
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
    memset(&send_data, 0, sizeof(struct srpacket));
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
    int last_packet = 0; // The sequence number of the last packet that was received.
    while (1)
    {
        if (send_data.type == REQUEST)
        {
            // Send
            // bytes_sent = sendto(sockfd, &send_data, send_data.length, 0,
            bytes_sent = sendto(sockfd, &send_data, sizeof(struct srpacket), 0,
                            (struct sockaddr *)&srv_addr, sizeof(struct sockaddr));
                        // TO-DO: Check to see if bytes_sent matches up with the length of the packet in total.
            print_packet_info_client(&send_data, CLIENT);
            // printf("Request sent.\n");
        }

        // --------------------------------
        // RECEIVE data response
        // --------------------------------
        printf("Waiting for response from CS118.\n");
        // bytes_recv = recvfrom(sockfd, &recv_data, recv_data.length, 0,
        bytes_recv = recvfrom(sockfd, &recv_data, sizeof(struct srpacket), 0, 
                        (struct sockaddr*)&srv_addr, &sin_size);
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
        memset(&send_data, 0, sizeof(struct srpacket));
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
        if (recv_data.sequence != last_packet || recv_data.corrupt == 1) // Packet received OUT OF ORDER or is CORRUPT
        {
            // The ACK should be for the last correctly-received packet.
            send_data.sequence = last_packet;
            send_data.corrupt = set_packet_corruption(p_corr);
            // Send
            printf("Sending acknowledgement...\n");
            // sendto(sockfd, &send_data, send_data.length, 0, 
            sendto(sockfd, &send_data, sizeof(struct srpacket), 0, 
                        (struct sockaddr *)&srv_addr, sizeof(struct sockaddr));
            print_packet_info_client(&send_data, CLIENT);

        }
        else // Packet is legit
        {
            send_data.sequence = recv_data.sequence;
            send_data.corrupt = set_packet_corruption(p_corr);
            // Convert to network byte order
            /*
            send_data.type = htonl(send_data.type);
            send_data.sequence = htonl(send_data.sequence);
            send_data.length = htonl(send_data.length);
            */
            last_packet = recv_data.sequence;
            // Send
            printf("Sending acknowledgement...\n");
            // sendto(sockfd, &send_data, send_data.length, 0, 
            sendto(sockfd, &send_data, sizeof(struct srpacket), 0, 
                       (struct sockaddr *)&srv_addr, sizeof(struct sockaddr));
            print_packet_info_client(&send_data, CLIENT);
        }

    }
    

    return 0;
}
