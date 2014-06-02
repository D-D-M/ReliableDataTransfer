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
    int base = 0;
    int seq_num = 0;
    int max = cwnd - 1;
    while (1)
    {
        // Recv data: The packet that carries the message from the client.
        struct srpacket recv_data;
        recv_data.length = p_size(); // Initialize recv_data to be the full packet size, to be shrunk later
        recv_data.sequence = 0;

        // Receive request from client. What type of request is it? 
        printf("Attempting to receive packet from client.\n");
        bytes_read = recvfrom(sockfd, &recv_data, recv_data.length, 0,
                            (struct sockaddr *)&cli_addr, &addr_size);
        printf("Packet received, number of bytes read = %d\n", bytes_read);
        printf("Packet data:\n");
        printf("%s\n",recv_data.data);

        recv_data.data[bytes_read] = '\0';
        printf("(%s, %d) said : ",inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        printf("%s\n", recv_data.data);

        printf("Sending packet...\n");
        strcpy(send_data.data, "it works!"); // Just something random.
        send_data.length = p_header_size() + strlen(send_data.data) + 1;
        sendto(sockfd, &send_data, send_data.length, 0, 
                    (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
        printf("Packet sent.\n");
        fflush(stdout);
        return 0;
    }

    // The client will request a file. Check if that file exists. If not, error.
    // If so, divide that file up into packets.


    // Send the packets to the client one by one. 

    return 0;
}
