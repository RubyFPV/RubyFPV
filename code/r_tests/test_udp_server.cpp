#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */

void DieWithError(const char *errorMessage)
{
   printf(errorMessage);
   exit(0);
}

int main(int argc, char *argv[])
{
    int sock;                         /* Socket */
    struct sockaddr_in broadcastAddr; /* Broadcast address */
    char *broadcastIP;                /* IP broadcast address */
    unsigned short broadcastPort;     /* Server port */
    char *sendString;                 /* String to broadcast */
    int broadcastPermission;          /* Socket opt to set permission to broadcast */
    unsigned int sendStringLen;       /* Length of string to broadcast */

    if (argc < 3)                     /* Test for correct number of parameters */
    {
        fprintf(stderr,"Usage:  %s <Port> <Send String>\n", argv[0]);
        exit(1);
    }

    broadcastPort = atoi(argv[1]);    /* Second arg:  broadcast port */
    sendString = argv[2];             /* Third arg:  string to broadcast */

    printf("Creating server on port %d, response msg: %s\n", broadcastPort, sendString);

    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    printf("Created socket\n");

    /* Set socket to allow broadcast */
    broadcastPermission = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission,
          sizeof(broadcastPermission)) < 0)
        DieWithError("setsockopt() failed");

    /* Construct local address structure */
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));   /* Zero out structure */
    broadcastAddr.sin_family = AF_INET;                 /* Internet address family */
    broadcastAddr.sin_addr.s_addr = htonl(INADDR_ANY);/* Broadcast IP address */
    broadcastAddr.sin_port = htons(broadcastPort);         /* Broadcast port */

    if (bind(sock, (struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr))==-1)
       DieWithError("bind failed." );

    printf("Waiting for clients on port %d\n", broadcastPort);

    sendStringLen = strlen(sendString);  /* Find length of sendString */
    int x = 0;
    for (;;) /* Run forever */
    {
        char szBuff[1024];
        socklen_t  recvLen = 0;
        struct sockaddr_in addrClient;
        if ( recvfrom(sock, szBuff, 512, 0, (sockaddr*)&addrClient, &recvLen) == -1 )
           printf("Receive error\n");
        else
        {
           szBuff[recvLen] = 0;
           printf("Received from client: %s", szBuff);
        }
        /*
        if (sendto(sock, sendString, sendStringLen, 0, (struct sockaddr *)
               &broadcastAddr, sizeof(broadcastAddr)) != sendStringLen)
             DieWithError("sendto() sent a different number of bytes than expected");
         printf("Sent message %d to port %d\n", x, broadcastPort);

         */
    }

}