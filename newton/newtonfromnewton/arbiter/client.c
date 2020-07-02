#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "protocol.h"
#include "util.h"
#include "client.h"


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void eexit(char *s)
{
  printf("%s\n",s);
  exit(1);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void UsageError()
{
  fprintf(stderr,"Usage:\n\tclient <port:server> <command> <argument>\n");
  exit(1);
}

int main(int argc, char **argv)
{
  char server[128],buf[512];  int port,sock;  command_t cmd;

  ////////////////////////////////////////////////////////////////////////////////
  // Check command line args
  ////////////////////////////////////////////////////////////////////////////////
  if( argc != 4 )                                 UsageError();
  if( sscanf(argv[1],"%d:%s",&port,server) != 2 ) UsageError();
  if( sscanf(argv[2],"%s",cmd.cmd) != 1 )         UsageError();
  if( sscanf(argv[3],"%s",cmd.arg) != 1 )         UsageError();

  ////////////////////////////////////////////////////////////////////////////////
  // Connect to server and send command
  ////////////////////////////////////////////////////////////////////////////////
  if( !(sock=Connect(server,port)) ) eexit("main(): Connect()");
  if( !SendCommand(sock,&cmd) )      eexit("main(): SendCommand()"); 

  ////////////////////////////////////////////////////////////
  if( !strcmp(argv[2],"status") ) {
    // Receive status file
    sprintf(buf,".client.%d",getpid());
    if( !RecvFile(sock,buf) )  
      eexit("main(): RecvFile()");
    // Print out and remove file
    if( !System("cat .client.%d",getpid()) )
      eexit("main(): cat status");
    System("rm -f .client.%d",getpid());
  }
  ////////////////////////////////////////////////////////////
  if( !strcmp(argv[2],"job") ) {
    // Send job file
    sprintf(buf,"%s.tar.gz",strtok(cmd.arg,":"));
    if( !SendFile(sock,buf) )  
      eexit("main(): SendFile()");
  }
  ////////////////////////////////////////////////////////////
  if( !strcmp(argv[2],"data") ) {
    // Check for job found response
    if( !RecvCommand(sock,&cmd) )
      eexit("main(): RecvCommand");
    if( strcmp(cmd.arg,"SUCCESS") ) {
      printf("(%s|%s)\n",cmd.cmd,cmd.arg);
      return 1;
    } 
    // Download data file
    sprintf(buf,"%s.results.tar.gz",argv[3]);
    if( !RecvFile(sock,buf) )  
      eexit("main(): RecvFile()");
  }
  ////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////////
  // Check return status
  ////////////////////////////////////////////////////////////////////////////////
  if( !RecvCommand(sock,&cmd) )
    eexit("main(): RecvCommand");
  if( strcmp(cmd.arg,"SUCCESS") ) {
    printf("(%s|%s)\n",cmd.cmd,cmd.arg);
    return 1;
  }

  // Return silent success
  return 0;
}
