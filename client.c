#include <sys/types.h>
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
    if (argc != 6)
    {
        printf("Arg count was %d.\n", argc);
        usage_err();
    }

    int sockfd, portnum, bytes_recv, bytes_sent;
//    int num_ints = 3; // Number of ints in a packet.

    socklen_t sin_size;

    struct sockaddr_in srv_addr;
    struct hostent *host;
    
    // Send data
    struct srpacket send_data;
    // Receive data
    struct srpacket recv_data;
    recv_data.length = p_size();
    recv_data.sequence = 0;

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
    send_data.type = REQUEST;
    send_data.sequence = 0;
    strcpy(send_data.data, filename); // Might have to \0 out the last byte of data...not sure yet
    send_data.data[strlen(filename)] = '\0';
    send_data.length = p_header_size() + strlen(filename) + 1; // +1 for the null byte?
    printf("Sending request to CS118 for %s\n", filename);
    // Convert to network byte order
    /*
    send_data.type = htonl(send_data.type);
    send_data.sequence = htonl(send_data.sequence);
    send_data.length = htonl(send_data.length);
    */
   
    while (1)
    {
        if (send_data.type == REQUEST)
        {
            // Send
            bytes_sent = sendto(sockfd, &send_data, send_data.length, 0,
                            (struct sockaddr *)&srv_addr, sizeof(struct sockaddr));
                        // TO-DO: Check to see if bytes_sent matches up with the length of the packet in total.
            print_packet_info_client(&send_data, CLIENT);
            // printf("Request sent.\n");
        }

        // --------------------------------
        // RECEIVE data response
        // --------------------------------
        printf("Waiting for response from CS118.\n");
        bytes_recv = recvfrom(sockfd, &recv_data, recv_data.length, 0,
                        (struct sockaddr*)&srv_addr, &sin_size);
        // Convert to host byte order
        /*
        recv_data.type = ntohl(recv_data.type);
        recv_data.sequence = ntohl(recv_data.sequence);
        recv_data.length = ntohl(recv_data.length);
        */

        recv_data.data[bytes_recv] = '\0';
        print_packet_info_client(&recv_data, SERVER);
//        printf("Received : %s\n", recv_data.data);

        // --------------------------------
        // SEND acknowledgement
        // --------------------------------
        memset(&send_data, 0, sizeof(struct srpacket));
        send_data.type = ACK;
        send_data.sequence = recv_data.sequence;
        strcpy(send_data.data, "Acknowledged.");
        send_data.data[strlen(send_data.data)] = '\0'; // Zero byte
        send_data.length = p_header_size() + strlen(send_data.data) + 1; // Might need +1 for zero byte
        // Convert to network byte order
        /*
        send_data.type = htonl(send_data.type);
        send_data.sequence = htonl(send_data.sequence);
        send_data.length = htonl(send_data.length);
        */
        // Send
        printf("Sending acknowledgement...\n");
        sendto(sockfd, &send_data, send_data.length, 0, 
                    (struct sockaddr *)&srv_addr, sizeof(struct sockaddr));
        print_packet_info_client(&send_data, CLIENT);
        // printf("Packet sent.\n");
        // printf("-------------------------------\n");

    }
    

    return 0;
}
