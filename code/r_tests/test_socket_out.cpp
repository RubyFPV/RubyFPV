#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"

#include <time.h>
#include <sys/resource.h>
#include<stdio.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

bool bQuit = false;

void handle_sigint(int sig)
{
   log_line("Caught signal to stop: %d\n", sig);
   bQuit = true;
}

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("TestSocketOut");
   log_enable_stdout();
   log_line("\nStarted.\n");

   int port = 5600;


   if ( argc >= 2 )
      port = atoi(argv[1]);


   struct addrinfo hints;
   memset(&hints,0,sizeof(hints));
   hints.ai_family=AF_UNSPEC;
   hints.ai_socktype=SOCK_DGRAM;
   hints.ai_protocol=0;
   hints.ai_flags=AI_ADDRCONFIG;
   struct addrinfo* res=0;
   //int err=getaddrinfo("192.168.42.129",NULL,&hints,&res);
   //if ( err != 0 )
   //   log_line("Failed to get destination info");
   //log_line("Got address info");

   int socketfd;
   struct sockaddr_in server_addr;

   socketfd = socket(AF_INET , SOCK_DGRAM, 0);
   //socketfd = socket(AF_INET,SOCK_DGRAM,res->ai_protocol);
   if (socketfd == -1)
   {
      log_line("Can't create socket.");
      exit(-1);
   }

   memset(&server_addr, 0, sizeof(server_addr));

   server_addr.sin_family = AF_INET;
   //server_addr.sin_addr.s_addr = inet_addr("192.168.42.129");
   server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
   server_addr.sin_port = htons( port );

   log_line("Sending data on port %d ...", port);

   while (!bQuit)
   {
      log_line("Sending data...");
      sendto(socketfd, "Hello", 4,
        0, (struct sockaddr *)&server_addr, sizeof(server_addr) );

       hardware_sleep_ms(500);
   }

   close(socketfd);
   log_line("\nEnded\n");
   exit(0);
}
