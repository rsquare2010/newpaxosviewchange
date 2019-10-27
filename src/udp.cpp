#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "port.h"
#include "messages.h"
#include <inttypes.h>
#include <stdint.h>
#include <getopt.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <bits/stdc++.h>
#include <queue>
#include <vector>
#include <cstdlib>

#define BUFLEN 4096 /* buffer size to read from socket */
#define MAX_STRING_SIZE 40 /*We assume this to be the Max Size of IP addresses, its fine since its IPv4*/
#define MAX_PEERS 20 /* My implementation of Paxos assumes a maximum of 20 peers.*/
#define INITIAL_WAIT_TIME 60 /* Time spent waiting for all the proceesses to be up. */

char pnames[MAX_PEERS][MAX_STRING_SIZE]; //Array to hold the IP address of other processes.
int psize; // # of other processes.

//Method to read from host file, resolve the IP address for the container names  and add unique IPs to an array of IP addresses.
void readFromHostFile( char* myIP) {
    FILE *fp;
    long lSize;
    char* contents;
    char myIPAddress[40];
    strcpy(myIPAddress, myIP);

    
    std::ifstream inFile;
    inFile.open("hostfile.txt");
    if (inFile.fail()) {
        std::cerr << "Error opeing a file" << std::endl;
        inFile.close();
        exit(1);
    }
    std::string line;//Each line represents a different container name.
    char *IPbufferLocal;
    struct hostent *host_entry;
    const char *temp;
    int index = 0;
    while (getline(inFile, line))
    {
        temp = line.c_str();
        host_entry = gethostbyname(temp);
        if(host_entry != NULL) {
            IPbufferLocal = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
        }
        if((IPbufferLocal != NULL) && strcmp(IPbufferLocal, myIPAddress)!=0) {
            strcpy(pnames[index], IPbufferLocal);
            index++;
        }
    };
    inFile.close();
    psize = index; // Total # of processes.

}

//Class to keep track of the highest proposal received till now, and array of it is used one per message index.
//class ProposalCounter {
//public:
//    int sequence; // Max of all suggested sequences.
//    int proposed_id[MAX_PEERS]; // All the processes that have suggested a sequence
//    int seqPropId; // Process id of the process that suggested sequence(variable #1)
//    int ackCount = 0; //Total no of unique acks received.
//
//    // Default Constructor
//    ProposalCounter()
//    {
//        sequence = 0;
//        for (int i = 0; i < 20; i++) {
//            proposed_id[i] = 0;
//        }
//        seqPropId = 0;
//        ackCount = 0;
//    }
//};

//Comparator to order the SeqMessage in the priority queue, this follows the ISIS algorithm, where the SeqMessages are ordered by the sequence numbers, and in case of matching sequence numbers we use the processID as a tie breaker(we pick the process id with the lesser value).
//struct Comp{
//    bool operator()(const SeqMessage& a, const SeqMessage& b){
//        if(a.final_seq == b.final_seq) {
//            return a.final_seq_proposer > b.final_seq_proposer;
//        }
//        return a.final_seq > b.final_seq;
//    }
//};

int main(int argc, char **argv)
{
    int option = 0; //for opt values
    char* path; //Path to hostfile, this is actually unnecessary.
    int myId; //process Id of this process.
    int testCase = 0;
    
    struct sockaddr_in myaddr, servaddr[MAX_PEERS]; //My address and address to connect to serve
    struct sockaddr_in remaddr; //Address of the sender in received messages.
    socklen_t addrlen = sizeof(remaddr);
    int fd, i, slen=sizeof(servaddr);
    char buf[BUFLEN];    /* message buffer */
    int recvlen;        /* # bytes in acknowledgement message */
    struct hostent *hp; // To get IP address of this container.

    
//    std::priority_queue<SeqMessage,std::vector<SeqMessage>,Comp> pq;//Priority queue of received SeqMessages by this process.
    
    while( (option = getopt(argc, argv, "h:t:")) != -1) {
        switch(option) {
            case 'h' :
                path = optarg;
                break;
            case 't':
                testCase = atoi(optarg);
                break;
            default: //print_usage();
                exit(EXIT_FAILURE);
        }
    }

    
    /* create a socket */
    if ((fd=socket(AF_INET, SOCK_DGRAM, 0))==-1)
        printf("socket created\n");

    
    /* bind it to all local addresses and pick any port number */
    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(SERVICE_PORT);
    
    
    if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind failed");
        return 0;
    }
    
    sleep(INITIAL_WAIT_TIME); //We wait 60 seconds for all the other processes to be up, apologies for the dirty implementation.

    //Find this process's ip address after finding its name
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    host_entry = gethostbyname(hostbuffer);
    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));

    //The IP address of this process
    printf("my ip is: %s\n", IPbuffer);
    readFromHostFile(IPbuffer);

    /*We derive a unique id for each process by getting the sum of the ascii values of the first 4 characters fo the container name and a random number between 0 and 999.*/
    int randNumber = rand() % 1000;
    myId = hostbuffer[0] + hostbuffer [1] + hostbuffer[3] + hostbuffer[4] + randNumber;
    printf("my Id is:%d\n", myId);



    /* put the host's address into the server address structure */
    for (int i = 0; i < psize; i++) {
        memset((char *) &servaddr[i], 0, sizeof(servaddr[i]));
        servaddr[i].sin_family = AF_INET;
        servaddr[i].sin_port = htons(SERVICE_PORT);
        servaddr[i].sin_addr.s_addr = inet_addr(pnames[i]);
    }
    
    return 0;
}


