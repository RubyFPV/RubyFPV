#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */

#define MAXRECVSTRING 255  /* Longest string to receive */

void DieWithError(const char *errorMessage)
{
   printf(errorMessage);
   exit(0);
}

int main(int argc, char *argv[])
{
    int sock;                         /* Socket */
    struct sockaddr_in clientAddr; /* Broadcast Address */
    unsigned short clientPort;     /* Port */
    char recvString[MAXRECVSTRING+1]; /* Buffer for received string */
    int recvStringLen;                /* Length of received string */

    if (argc < 3)                     /* Test for correct number of parameters */
    {
        fprintf(stderr,"Usage:  %s <IP Address server> <Port>\n", argv[0]);
        exit(1);
    }

    char* serverIp = argv[1];
    clientPort = atoi(argv[2]);   /* First arg: broadcast port */

    printf("Creating socket on port %d for server %s\n", clientPort, serverIp);
    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    /* Construct bind structure */
    memset(&clientAddr, 0, sizeof(clientAddr));   /* Zero out structure */
    clientAddr.sin_family = AF_INET;                 /* Internet address family */
    clientAddr.sin_port = htons(clientPort);      /* client port */

   if (inet_aton(serverIp , &clientAddr.sin_addr) == 0)
       DieWithError("Failed to convert server ip address.");

    printf("Sending and waiting for data on port %d", clientPort);

   char szMsg[32];
   strcpy(szMsg, "helloclient");
   if (sendto(sock, szMsg, strlen(szMsg)+1 , 0 , (struct sockaddr *) &clientAddr, sizeof(clientAddr))==-1)
      printf("error sending\n");
    /* Receive a single datagram from the server */
    if ((recvStringLen = recvfrom(sock, recvString, MAXRECVSTRING, 0, NULL, 0)) < 0)
        DieWithError("recvfrom() failed");

    recvString[recvStringLen] = '\0';
    printf("Received: %s\n", recvString);    /* Print the received string */

    close(sock);
    exit(0);
}