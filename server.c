#include <stdio.h> // Printing
#include <time.h> // To seed the random number generator
#include <stdlib.h> // atoi
#include <string.h>
#include <sys/socket.h> // C sockets
#include <sys/types.h>
#include <sys/poll.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>

#include "gbnpacket.h"

#define TIMEOUT 3000 // Packet timeout TIMEOUT milliseconds

int main(int argc, char *argv[])
{
    srand48(time(NULL)); // Seed RNG
    // struct timespec timer, rightnow; // For packet timeouts
    // time_t timert, rightnowt;
    struct pollfd ufd; // Polling for the timeout
    int rv;

    // For statistical purposes
    int corrupt_count = 0;
    int loss_count = 0;

    if (argc != 5)
    {
        printf("Arg count was %d.\n", argc);
        usage_err();
    }
    // Parse command line arguments
    int portnum = atoi(argv[1]); // Make sure this is the right argument number
    int N = atoi(argv[2]); // Window size
    double p_loss = atof(argv[3]); // Probability of packet loss
    double p_corr = atof(argv[4]); // Probability of corrupt packet

    int sockfd, bytes_read, bytes_sent;
    char *buffer; // A buffer for the data of the served file.
    // int numpackets = -1;

    int lost_packet = 0;
    int totalnumpackets = -1;
    long file_length = 0;

    // Send data: The packet to be sent to the client.
    struct gbnpacket send_data;
    struct gbnpacket recv_data;
        
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

    long last_ACK = 0; // We know that the first ACK# we get will be 1024
    int base = 0;
    int rn = 0; // Request number
    int sn = 0; // Sequence number, incremented by 1 every time
    long seq_num = 0;
    int ew = N - 1; // The end of the current "window"
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RECEIVE INITIAL CLIENT REQUEST
////////////////////////////////////////////////////////////////////////////////////////////////////////////
clientrequest:
    printf("Waiting for client on port %d...\n", portnum);
    last_ACK = 0; base = 0; rn = 0; sn = 0; seq_num = 0; ew = N - 1;
    memset(&recv_data, 0, sizeof(struct gbnpacket)); // Zero out the packet
    bytes_read = recvfrom(sockfd, &recv_data, sizeof(struct gbnpacket), 0,
                                (struct sockaddr *)&cli_addr, &addr_size);
    recv_data.type = ntohl(recv_data.type);
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
            char *msg = "File does not exist";
            memset(&send_data, 0, sizeof(struct gbnpacket));
            send_data.type = FIN;
            send_data.sequence = 0;
            send_data.corrupt = 0; // play_the_odds();
            send_data.length = 0;
            // Don't need to initialize the data because of the memset
            printf("%s. Sending FIN packet.\n", msg);
            bytes_sent = sendto(sockfd, &send_data, sizeof(struct gbnpacket), 0, 
                            (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
            goto clientrequest;
            // printf("File \"%s\" does not exist.\n", filename); 
            // SEND A 'FIN' PACKET to the client
            // continue;
            // exit(1);
        }
        fseek(file, 0, SEEK_END);
        file_length = ftell(file);
        rewind(file);
        // buffer = (char *)malloc((file_length+1)*sizeof(char));
        buffer = (char *)malloc((file_length)*sizeof(char));
        // memset(&buffer, 0, sizeof(char)*(file_length+1));
        if (!buffer)
        {
            // printf("File \"%s\" too big to fit in memory.\n", filename);
            char *msg = "File was too big to fit into memory";
            memset(&send_data, 0, sizeof(struct gbnpacket));
            send_data.type = FIN;
            send_data.sequence = 0;
            send_data.corrupt = 0; // play_the_odds();
            send_data.length = 0;
            // Don't need to initialize the data because of the memset
            printf("%s. Sending FIN packet.\n", msg);
            bytes_sent = sendto(sockfd, &send_data, sizeof(struct gbnpacket), 0, 
                            (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
            goto clientrequest;
            // SEND A 'FIN' PACKET to the client?
            // The client should exit(1), and the server should continue;
            // continue;
            // exit(1);
        }
        fread(buffer, file_length, 1, file);
        fclose(file);
        printf("File is now in the buffer, size %ld bytes.\n", file_length);

        // Figure out how many packets we will need.
        totalnumpackets = ((file_length-1)/ PACKETSIZE) + 1; // Integer division
                                                    // We want 512 bytes to be 1 packet
        // totalnumpackets = numpackets;
        printf("This file will be divided into %d packets.\n", totalnumpackets);
        goto servicerequest;
    }
    else if (recv_data.type == FIN)
    {
        goto clientrequest; // Do nothing
    }
    else 
    {
        // Construct & send a FIN packet letting the client know it was an invalid request
            char *msg = "Invalid request";
            memset(&send_data, 0, sizeof(struct gbnpacket));
            send_data.type = FIN;
            send_data.sequence = 0;
            send_data.corrupt = 0; // play_the_odds();
            send_data.length = 0;
            // Don't need to initialize the data because of the memset
            printf("%s. Sending FIN packet.\n", msg);
            bytes_sent = sendto(sockfd, &send_data, sizeof(struct gbnpacket), 0, 
                            (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
            goto clientrequest;
    }
////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Think about this in terms of packets...
    // uninitialized file = divided into packets
    // Need a loop counter perhaps
    // time(&timert);
    while (1)
    {
        // RECEIVE request from client. 
        // ------------------------------------
        // struct gbnpacket recv_data;
        memset(&recv_data, 0, sizeof(struct gbnpacket)); // Zero out the packet
        // recv_data.length = PACKETSIZE; // Initialize recv_data to be the full packet size, to be shrunk later
        // recv_data.sequence = 0;
        // printf("Attempting to receive packet from client.\n");
        // bytes_read = recvfrom(sockfd, &recv_data, recv_data.length+p_header_size(), 0,

        ufd.fd = sockfd;
        ufd.events = POLLIN;
        printf("Polling for a client request.....\n");
        rv = poll(&ufd, 1, TIMEOUT); // Poll for TIMEOUT milliseconds
        if (rv == -1)
        {
            perror("poll");
        }
        else if (rv == 0)
        {
            printf("===================================================\n");
            printf("====================TIMEOUT!=======================\n");
            printf("===================================================\n\n");
            // printf("Last ack was %d. Sn was %d, base was %d.\n", last_ACK, sn, base);
            recv_data.sequence = last_ACK;
            seq_num = last_ACK;
            recv_data.type = ACK; // Give it the ACK flag
            sn = base;
            printf("Resending (at most) %d packets, beginning with sequence number %ld\n", N, last_ACK);
            printf("%.0f%% of packets have been reliably transferred.\n", (double)last_ACK/(double)PACKETSIZE);
            sleep(2);
        }
        else
        {
            if (ufd.revents & POLLIN)
            {
                bytes_read = recvfrom(sockfd, &recv_data, sizeof(struct gbnpacket), 0,
                                (struct sockaddr *)&cli_addr, &addr_size);
                lost_packet = play_the_odds(p_loss, &loss_count);
                if (lost_packet)
                {
                    printf("\nLOST PACKET!\n\n");
                    continue; // Do nothing else, just receive the packet and move on.
                }
            }
        }
        // CHECK TIMEOUT HERE
        // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &rightnow);
        // int difftime = diff(timer, rightnow).tv_sec;
        // rightnowt = clock();
        /*
        time(&rightnowt);
        time_t difftime = rightnowt - timert;
        printf("Difftime is %ld\n", (long int)difftime);
        if (difftime >= TIMEOUT)
        {
            printf("=\n=\n=\nTimeout! Now what?\n=\n=\n=\n"); 
            sn = base; // I think this is all we need to do....
            // sn = recv_data.sequence/(int)PACKETSIZE;
        }
        */
        //
        //
        //
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
    
        
        // else // The received packet is an ACK
        // { 
        // printf("Number of remaining packets to send: %d\n", numpackets);
        // if (numpackets > 0)
        /*
        if (last_ACK == recv_data.sequence)
        {
            // reset timer
            // update lastack
            last_ACK += PACKETSIZE;
        }
        */
servicerequest:
        // printf("recvdataseq is %d, filelen is %ld, last_ACK is %d\n", recv_data.sequence, file_length, last_ACK);
        // if (recv_data.sequence < file_length)
        if (last_ACK < file_length)
        {
            rn = recv_data.sequence / (int)PACKETSIZE;
            // If the request number is bigger than the base,
            // If the request seqnum is equal to the last ACK'd packet,
            // and if the request is not corrupt....
            // printf("base = %d\n", base);
            // printf("rn   = %d\n", rn);
            // printf("lACK before = %d, ", last_ACK);
            if (rn > base && recv_data.corrupt == 0 && last_ACK == (recv_data.sequence - PACKETSIZE))
            {
                // Then we can shift the window over by one, and reset the timer
                // printf("\nSHIFTING SEND WINDOW.\n\n");
                ew = ew + (rn - base);
                base = rn;
                // timert = clock();
                // time(&timert);
                // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timer); // Reset the timer
                last_ACK += PACKETSIZE; // Update the last acknowledged packet
            }
            // printf("lACK after = %d, ", last_ACK);
            // printf("Before constructing packet %d,\n", seq_num);
            // printf("sn is %d, ew is %d, and base is %d\n", sn, ew, base);
            while (sn < totalnumpackets && sn <= ew && sn >= base)
            {
                // Construct packet i from the buffer
                memset(&send_data, 0, sizeof(struct gbnpacket)); 
                // printf("Memset worked\n");
                // construct_next_packet_data(seq_num, buffer, &send_data);
                send_data.type = DATA;
                send_data.sequence = seq_num;
                // THIS IS IMPORTANT FOR RE-SENDING THE 'N' FRAMES
                if (sn == base)
                    seq_num = last_ACK;
                int buffadd = seq_num;
                char* chunk = &buffer[buffadd];
                // data length is 1024 unless we're on the last packet
                if ( (file_length - seq_num) < PACKETSIZE)
                    send_data.length = (file_length % PACKETSIZE);
                else                    // Otherwise send the full packet size
                    send_data.length = PACKETSIZE; 
                memcpy(send_data.data, chunk, send_data.length); // ONLY copy send_data.length bytes
                
                // printf("Sending packet with sequence #%d...\n", seq_num);
                send_data.corrupt = play_the_odds(p_corr, &corrupt_count);
                send_data.total_length = file_length;
                // Convert to network byte order
                /*
                send_data.type = htonl(send_data.type);
                send_data.sequence = htonl(send_data.sequence);
                send_data.length = htonl(send_data.length);
                */
                // Send
                // sendto(sockfd, &send_data, send_data.length, 0, 
//                double legit = drand48();
//                if (legit >= p_loss)
//                {
                    bytes_sent = sendto(sockfd, &send_data, sizeof(struct gbnpacket), 0, 
                            (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
                    /*
                    if (send_data.sequence/PACKETSIZE == base)
                    {
                        // start timer
                        // timert = clock();
                        // time(&timert);
                        // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timer);
                    }
                    */
                    print_packet_info_server(&send_data, SERVER);
//                }
//                else
//                    continue;
                // printf("Packet sent.\n");
                // printf("-------------------------------\n");
                
                seq_num += (bytes_sent - p_header_size());
                sn++;
                // fflush(stdout);
            }

            if (last_ACK >= file_length)
            {
                char *msg = "File transfer complete";
                memset(&send_data, 0, sizeof(struct gbnpacket));
                send_data.type = FIN;
                send_data.sequence = 0;
                send_data.corrupt = 0; // play_the_odds();
                send_data.length = 0;
                // Don't need to initialize the data because of the memset
                printf("===================================================\n");
                printf("===================================================\n");
                printf("%s. %ld bytes transmitted. Sending FIN packet.\n", msg, file_length);
                printf("STATS: Sent %d corrupt packets. Treated %d packets as \'lost\'.\n",
                            corrupt_count, loss_count);
                printf("===================================================\n");
                printf("===================================================\n");
                bytes_sent = sendto(sockfd, &send_data, sizeof(struct gbnpacket), 0, (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
                goto clientrequest;
            }
            /*
            else
            {
                printf("|                |\n");
                printf("| Pipeline full! |\n");
                printf("------------------------------------\n");
            }
            */
        }
        /*
        else
        {
            char *msg = "NEVER GETTING HEREFile transfer complete";
            memset(&send_data, 0, sizeof(struct gbnpacket));
            send_data.type = FIN;
            send_data.sequence = 0;
            send_data.corrupt = 0; // play_the_odds();
            send_data.length = 0;
            // Don't need to initialize the data because of the memset
            printf("===================================================\n");
            printf("===================================================\n");
            printf("%s. Sending FIN packet.\n", msg);
            printf("===================================================\n");
            printf("===================================================\n");
            bytes_sent = sendto(sockfd, &send_data, sizeof(struct gbnpacket), 0, 
                            (struct sockaddr *)&cli_addr, sizeof(struct sockaddr));
            goto clientrequest;
        }
        */
            // return 0;
        //}
    }
    free(buffer);

    // The client will request a file. Check if that file exists. If not, error.
    // If so, divide that file up into packets.


    // Send the packets to the client one by one. 

    return 0;
}
