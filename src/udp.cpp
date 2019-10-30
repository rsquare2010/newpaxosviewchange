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
#include <chrono>
#include <thread>

#define BUFLEN 4096 /* buffer size to read from socket */
#define MAX_STRING_SIZE 40 /*We assume this to be the Max Size of IP addresses, its fine since its IPv4*/
#define MAX_PEERS 20 /* My implementation of Paxos assumes a maximum of 20 peers.*/
#define INITIAL_WAIT_TIME 6 /* Time spent waiting for all the proceesses to be up. */

char pnames[MAX_PEERS][MAX_STRING_SIZE]; //Array to hold the IP address of other processes.
int psize; // # of other processes.
int installed = 0; // Default view Id of all views.
int attempted = 0; // View to install, will be incremented
int myId; //The id of each server.
int testCase = 1; //Default test case.
struct sockaddr_in servaddr[MAX_PEERS];

void readFromHostFile(char*);
void receive(int);
void sendHeartBeat(int);
void sendVCProof(int);
void sendViewChange(int, int);
void startElection(int);
bool shouldKillServer(int);

/**
 * Global data structure to keep track of the votes..
 * I have this to track votes for future views.
 * The votes for a view is reset in the next view.
 */
class VoteCounter {
public:
    int voteArray[5][5] = {};
    bool isMajority(int attempt) {
        if(getCount(attempt) > psize/2) {
            return true;
        }
        return false;
    }
    int getCount(int attempt) {
        int count = 0;
        for (int i = 0; i < 5; i++) {
            if (voteArray[attempt][i] > 0) {
                count++;
            }
        }
        return count;
    }
    void reset(int attempt) {
        std::fill(std::begin(voteArray[attempt]), std::end(voteArray[attempt]), 0);
    }
};
//Create an instance of the global counter
VoteCounter globalVoteCounter;

int main(int argc, char **argv)
{
    int option = 0; //for opt values
    char* path; //Path to hostfile, this is actually unnecessary.
    int myId; //process Id of this process.
    
    struct sockaddr_in myaddr; //My address and address to connect to serve
    struct sockaddr_in remaddr; //Address of the sender in received messages.
    socklen_t addrlen = sizeof(remaddr);
    int fd, i, slen=sizeof(servaddr);
    char buf[BUFLEN];    /* message buffer */
    int recvlen;        /* # bytes in acknowledgement message */
    struct hostent *hp; // To get IP address of this container.
    
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

    /* put the host's address into the server address structure */
    for (int i = 0; i < psize; i++) {
        memset((char *) &servaddr[i], 0, sizeof(servaddr[i]));
        servaddr[i].sin_family = AF_INET;
        servaddr[i].sin_port = htons(SERVICE_PORT);
        servaddr[i].sin_addr.s_addr = inet_addr(pnames[i]);
    }

    //Send parallel heartbeat
    std::thread th1(sendHeartBeat, fd);
    receive(fd);// start the 0th view.

    th1.join();
    return 0;
}

/**
 * Infinitely send heartbeats(VCProof) every 3 seconds. This will be called from a thread in the main method.
 */
void sendHeartBeat(int fd) {
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds;

    start = std::chrono::system_clock::now();
    for (;;) {
        if(elapsed_seconds.count() > 3) {
            sendVCProof(fd);
            start = std::chrono::system_clock::now(); //reset
        }
        end = std::chrono::system_clock::now();
        elapsed_seconds = end-start;
    }
}

/**
 * progress countdown during a view.
 */
void receive(int fd) {
    struct sockaddr_in remaddr;
    socklen_t addrlen = sizeof(remaddr);
    char buf[BUFLEN];
    int nready;
    int recvlen;        /* # bytes in acknowledgement message */

    fd_set writefd;
    FD_ZERO(&writefd);
    FD_SET(fd, &writefd);

    printf("current view is: %d \n", installed);
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds;

    start = std::chrono::system_clock::now();

    while (elapsed_seconds.count() < 10) { //Make this global

        nready = select(fd + 1, &writefd, NULL, NULL, NULL);
        if(nready == 0) {
        } else {
            recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &addrlen);
            if (recvlen > 0) {
                //Do nothing cause we do nothing while our view is progressing.
            }
        }
        end = std::chrono::system_clock::now();
        elapsed_seconds = end-start;
    }

    //Start the leader election process once the timer runs out.
    if(testCase > 1 || installed == 0) {
        startElection(fd);
    }
}

/**
 * Leader election process.
 */
void startElection(int fd) {

    printf("Starting leader election\n");
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds;
    ViewChange rcvdViewChange;
    VCProof rcvdVCProof;
    struct sockaddr_in remaddr;
    socklen_t addrlen = sizeof(remaddr);
    char buf[BUFLEN];
    int recvlen;
    int nready;

    fd_set writefd;
    FD_ZERO(&writefd);
    FD_SET(fd, &writefd);

    start = std::chrono::system_clock::now();
    globalVoteCounter.reset(attempted);
    sendViewChange(fd, attempted + 1);
    attempted++;

    printf("attempting to install view id: %d \n", attempted);
    bool isCompletedSuccessfully = false;
    if(globalVoteCounter.isMajority(attempted)) {
        if(shouldKillServer(attempted)) {
            exit(1);
        }
    }
    while (elapsed_seconds.count() < 10) { //Make this global

        nready = select(fd + 1, &writefd, NULL, NULL, NULL);
        if(nready == 0) {
//            printf("The view ID: %d timed out because of inactivity", 0);
        } else if (nready > 0) {
            recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &addrlen);
            if (recvlen > 0) {
                int temp;
                memcpy(&temp, &buf[0], 4);
                if (ntohl(temp) == 1) {
                    deserializeVC(buf, &rcvdViewChange);
//                    printf("ViewChange message for view:%d received from %d\n", rcvdViewChange.attempted,rcvdViewChange.server_id);
                    if( rcvdViewChange.attempted > attempted) {
                        globalVoteCounter.voteArray[rcvdViewChange.attempted][rcvdViewChange.server_id] = 1;
                    } else if (rcvdViewChange.attempted == attempted) {
                        globalVoteCounter.voteArray[attempted][rcvdViewChange.server_id] = 1;
                        if(globalVoteCounter.isMajority(attempted)) {

                            if(shouldKillServer(attempted)) {
                                exit(1);
                            }
                            installed = attempted;
                            isCompletedSuccessfully = true;
                            break;
                        }
                    }
                } else if(ntohl(temp) == 2){
                    deserializeVP(buf, &rcvdVCProof);
                    if(rcvdVCProof.installed > installed) {
                        if(shouldKillServer(rcvdVCProof.installed)) {
                            exit(1);
                        }
                        installed = rcvdVCProof.installed;
                        isCompletedSuccessfully = true;
//                        printf("higher VC proof from: %d\n", rcvdVCProof.server_id);
                        break;
                    }
                }
            }
        } else {
            printf("select error \n");
        }
        end = std::chrono::system_clock::now();
        elapsed_seconds = end-start;
    }

    if(isCompletedSuccessfully) {
        if(attempted == installed) {
            sendVCProof(fd);
        }
        receive(fd);
    } else {
        startElection(fd);
    }
}


/**
 * Method to send VCProof message.
 * We need this in addition to the heartbeats in instances where this server elects the new leader,
 * it should let others know.
 */
void sendVCProof(int fd) {

    struct sockaddr_in myaddr;
    char buf[BUFLEN];

    VCProof vcProofMessage;
    vcProofMessage.type = 2;
    vcProofMessage.server_id = myId;
    vcProofMessage.installed = installed;

    serializeVP(&vcProofMessage, &buf[0]);
    for (int pid = 0; pid < psize; pid ++) {
        if (sendto(fd, buf, sizeof(vcProofMessage), 0, (struct sockaddr *)&servaddr[pid], sizeof(struct sockaddr_in))==-1) {
            perror("heartbeat failed");
        }
    }
}

/**
 * Send view change every time this server moves to leader election.
 */
void sendViewChange(int fd, int attempt) {
    char buf[BUFLEN];

    ViewChange viewChange;
    viewChange.type = 1;
    viewChange.server_id = myId;
    viewChange.attempted = attempt;

    serializeVC(&viewChange, &buf[0]);
    for (int pid = 0; pid < psize; pid++) {
        if (sendto(fd, buf, sizeof(viewChange), 0, (struct sockaddr *)&servaddr[pid], sizeof(struct sockaddr_in))==-1) {
            printf("view change failed for %d", pid);
        }
    }

    globalVoteCounter.voteArray[attempt][myId] = 1;
}

/**
 * Method to decide if a server should be killed or not based on the current view and test case.
 * @return true if this server should die.
 */
bool shouldKillServer(int view) {
    if (testCase == 3) {
        if(view % 5 == 1 && myId == 1) {
            return true;
        }
    } else if (testCase == 4) {
        if(view % 5  == 1 && myId == 1) {
            return true;
        }
        if(view %5 == 2 && myId == 2) {
            return true;
        }
    } else if (testCase == 5) {
        if(view %5 ==1 && myId == 1) {
            return true;
        }
        if(view %5 == 2 && myId == 2) {
            return true;
        }
        if(view %5 == 3 && myId == 3) {
            return true;
        }
    }
    return false;
}

/**
 * Method to read from host file, resolve the IP address for the container names and add unique IPs to an array of IP addresses.
 * Takes in the IP address of the current server as a parameter to not add it.
 */
void readFromHostFile( char* myIP) {
    FILE *fp;
    long lSize;
    char* contents;
    char myIPAddress[40];
    strcpy(myIPAddress, myIP);


    std::ifstream inFile;
    inFile.open("hostfile.txt");
    if (inFile.fail()) {
        std::cerr << "Error opening a file" << std::endl;
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
        if (strcmp(IPbufferLocal, myIPAddress) == 0) {
            char id = *(temp + 6);
            myId = id - 48;
            printf("my id is: %d \n", myId);

        }
        if((IPbufferLocal != NULL) && strcmp(IPbufferLocal, myIPAddress)!=0) {
            strcpy(pnames[index], IPbufferLocal);

            index++;
        }
    };
    inFile.close();
    psize = index; // Total # of processes.
}