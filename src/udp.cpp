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
#define MSGS 5    /* default number of messages to send if none provided */
#define MAX_STRING_SIZE 40 /*We assume this to be the Max Size of IP addresses, its fine since its IPv4*/
#define MAX_PEERS 20 /* My implementation of ISIS assumes a maximum of 20 peers.*/
#define INITIAL_WAIT_TIME 60 /* Time spent waiting for all the proceesses to be up. */

char pnames[MAX_PEERS][MAX_STRING_SIZE]; //Array to hold the IP address of other processes.
int psize; // NO of other processes.

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
    char *IPbuffer1;
    struct hostent *host_entry;
    const char *temp;
    int index = 0;
    while (getline(inFile, line))
    {
        temp = line.c_str();
        host_entry = gethostbyname(temp);
        if(host_entry != NULL) {
            IPbuffer1 = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
        }
        if((IPbuffer1 != NULL) && strcmp(IPbuffer1, myIPAddress)!=0) {
            strcpy(pnames[index], IPbuffer1);
            index++;
        }
    };
    inFile.close();
    psize = index; // Total # of processes.

}

//Class to keep track of the highest proposal received till now, and array of it is used one per message index.
class ProposalCounter {
public:
    int sequence; // Max of all suggested sequences.
    int proposed_id[MAX_PEERS]; // All the processes that have suggested a sequence
    int seqPropId; // Process id of the process that suggested sequence(variable #1)
    int ackCount = 0; //Total no of unique acks received.
    
    // Default Constructor
    ProposalCounter()
    {
        sequence = 0;
        for (int i = 0; i < 20; i++) {
            proposed_id[i] = 0;
        }
        seqPropId = 0;
        ackCount = 0;
    }
};

//Comparator to order the SeqMessage in the priority queue, this follows the ISIS algorithm, where the SeqMessages are ordered by the sequence numbers, and in case of matching sequence numbers we use the processID as a tie breaker(we pick the process id with the lesser value).
struct Comp{
    bool operator()(const SeqMessage& a, const SeqMessage& b){
        if(a.final_seq == b.final_seq) {
            return a.final_seq_proposer > b.final_seq_proposer;
        }
        return a.final_seq > b.final_seq;
    }
};

int main(int argc, char **argv)
{
    int option = 0; //for opt values
    int MSG_COUNT = MSGS; //Assigning default value to message count in case one isnt supplied.
    int messageCounter = 0; //Message counter of this process
    int sequnceCounter = 0; // Sequence counter of this process.
    int dropRate = 0; //Default drop rate value
    int msDelay = 0; //Default delay value
    char* path; //Path to hostfile, this is actually unnecessary.
    int myId; //process Id of this process.
    
    struct sockaddr_in myaddr, servaddr[MAX_PEERS]; //My address and address to connect to serve
    struct sockaddr_in remaddr; //Address of the sender in received messages.
    socklen_t addrlen = sizeof(remaddr);
    int fd, i, slen=sizeof(servaddr);
    char buf[BUFLEN];    /* message buffer */
    int recvlen;        /* # bytes in acknowledgement message */
    struct hostent *hp; // To get IP address of this conatiner.
    
    AckMessage ackMessages;
    SeqMessage seqMessage;
    DataMessage sendMessage;
    DataMessage rcvdDMMessage;
    AckMessage rcvdAMMdessage;
    SeqMessage rcvdSeqMessage;
    
    std::priority_queue<SeqMessage,std::vector<SeqMessage>,Comp> pq;//Priority queue of received SeqMessages by this process.
    
    while( (option = getopt(argc, argv, "h:c:d:t:")) != -1) {
        switch(option) {
            case 'h' :
                path = optarg;
                break;
            case 'c' :
                MSG_COUNT =  atoi(optarg);
                break;
            case 'd' :
                dropRate = atoi(optarg);
                break;
            case 't':
                msDelay = atoi(optarg);
                break;
            default: //print_usage();
                exit(EXIT_FAILURE);
        }
    }
    
    //If drop rate is greater than the messge count we ignore.
    if(dropRate > MSG_COUNT) {
        dropRate = 0;
    }
    
    //Creating an array of the proposal counter class, one per message.
    ProposalCounter proposalCounter[MSG_COUNT];
    
    /* create a socket */
    if ((fd=socket(AF_INET, SOCK_DGRAM, 0))==-1)
        printf("socket created\n");
    //Enable broadcast flag..
//    int broadcastEnable=1;
//    int ret=setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    
    /* bind it to all local addresses and pick any port number */
    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(SERVICE_PORT);
    
    
    if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind failed");
        return 0;
    }
    /* now define servaddr, the address to whom we want to send messages */
    /* For convenience, the host address is expressed as a numeric IP address */
    /* that we will convert to a binary format via inet_aton */
    
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
    
    
    /* now let's send the messages */
    for (i=0; i < MSG_COUNT; i++) {
        if(dropRate> 0 && (i % dropRate) == 0) {//If we need to drop messages.
            messageCounter++;
            continue;
        }
        for (int pid = 0; pid < psize; pid ++) {
            sendMessage.type = 1;
            sendMessage.message_id = messageCounter;
            sendMessage.sender = myId;
            serializeDM(&sendMessage, &buf[0]);
            std::this_thread::sleep_for(std::chrono::milliseconds(msDelay));
            if (sendto(fd, buf, sizeof(sendMessage), 0, (struct sockaddr *)&servaddr[pid], sizeof(struct sockaddr_in))==-1) {
                perror("sendto failed");
            }
        }
        messageCounter++;
    }
        while(1) { //Infinite receive..
            recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &addrlen);//Receive anything from anyone
            if (recvlen > 0) {
                            int temp;
                            memcpy(&temp, &buf[0], 4); //We get the type of message
                if(ntohl(temp) == 1) { //If its data type we create an ack and send it back to the sender.
                    deserializeDM(buf, &rcvdDMMessage);
                    ackMessages.type = 2;
                    ackMessages.sender = rcvdDMMessage.sender;
                    ackMessages.msg_id = rcvdDMMessage.message_id;
                    ackMessages.proposed_seq = sequnceCounter;
                    ackMessages.proposer = myId;
                    sequnceCounter++;
                    
                    serializeAM(&ackMessages, &buf[0]);
                    
                    int len=50;
                    char ipbuffer[len];
                    std::this_thread::sleep_for(std::chrono::milliseconds(msDelay));
                    if (sendto(fd, buf, sizeof(ackMessages), 0, (struct sockaddr *)&remaddr, sizeof(struct sockaddr_in))==-1) {
                        perror("sendto failed");
                    }
                } else if (ntohl(temp) == 2) { //If an ack is received, and we have received ACKs from all the processes we create the SeqMessage and broadcast it. otherwise we added it to the proposal class for this particular messageid.
                    deserializeAM(buf, &rcvdAMMdessage);
                    int messageId = rcvdAMMdessage.msg_id;
                    int ackCount = proposalCounter[messageId].ackCount;
                    if(proposalCounter[messageId].ackCount == 0) { //If first Ack Message add directly.
                        proposalCounter[messageId].sequence = rcvdAMMdessage.proposed_seq;
                        proposalCounter[messageId].proposed_id[ackCount] = rcvdAMMdessage.proposer;
                        proposalCounter[messageId].seqPropId = rcvdAMMdessage.proposer;
                        proposalCounter[messageId].ackCount = 1;
                    } else { //Check if its a duplicate before add.
                        int isDuplicate = 0;
                        for (int i = 0; i < ackCount; i++) {
                            if (proposalCounter[messageId].proposed_id[i] == rcvdAMMdessage.proposer) {
                                isDuplicate = 1;
                            }
                        }
                        if(isDuplicate == 1) {
                            if (rcvdAMMdessage.proposed_seq == proposalCounter[messageId].sequence) {
                                if(rcvdAMMdessage.proposer < proposalCounter[messageId].seqPropId) {
                                    proposalCounter[messageId].sequence = rcvdAMMdessage.proposed_seq;
                                    proposalCounter[messageId].proposed_id[ackCount] = rcvdAMMdessage.proposer;
                                    proposalCounter[messageId].seqPropId = rcvdAMMdessage.proposer;
                                    proposalCounter[messageId].ackCount++;
                                }
                            } else if (rcvdAMMdessage.proposed_seq > proposalCounter[messageId].sequence) {
                                    proposalCounter[messageId].sequence = rcvdAMMdessage.proposed_seq;
                                    proposalCounter[messageId].proposed_id[ackCount] = rcvdAMMdessage.proposer;
                                    proposalCounter[messageId].seqPropId = rcvdAMMdessage.proposer;
                                    proposalCounter[messageId].ackCount++;
                                } else {
                                    proposalCounter[messageId].proposed_id[ackCount] = rcvdAMMdessage.proposer;
                                    proposalCounter[messageId].ackCount++;
                                }
                            }
                        }
                        if(ackCount == psize - 1) { //If we get all messages send Seq to everyone.
                            seqMessage.type = 3;
                            seqMessage.sender = myId;
                            seqMessage.msg_id = messageId;
                            seqMessage.final_seq = proposalCounter[messageId].sequence;
                            seqMessage.final_seq_proposer = proposalCounter[messageId].seqPropId;
                            serializeSM(&seqMessage, &buf[0]);
                            for(int i = 0; i < psize; i++) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(msDelay));
                                if (sendto(fd, buf, sizeof(seqMessage), 0, (struct sockaddr *)&servaddr[i], sizeof(struct sockaddr_in))==-1) {
                                    perror("sendto failed");
                                }
                            }
                        }
                } else if (ntohl(temp) == 3) { //If we receive a SeqMessage we add it to the priority queue.
                    deserializeSM(buf, &rcvdSeqMessage);
                    SeqMessage sm = rcvdSeqMessage;
                    pq.push(rcvdSeqMessage);
                    if(pq.size() >= sequnceCounter ) { //If the priority queue has as many messages as the messages received, we display the elements of the queue in order.
                        printf("============================Ordering==================\n");
                        while (pq.size() > 0) {
                            SeqMessage sm = pq.top();
                            pq.pop();
                            printf("Process id %d processed Message %d from sender %d with sequence %d proposed by %d\n", myId,sm.msg_id, sm.sender, sm.final_seq, sm.final_seq_proposer);
                        }

                    }
                }
                
            }
            else {
                printf("uh oh - something went wrong!\n");
            }
        }
    return 0;
}


