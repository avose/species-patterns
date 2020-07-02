#ifndef UTIL_C
#define UTIL_C

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/select.h>

#include "util.h"

////////////////////////////////////////////////////////////////////////////////

// Exit loudly
void Die(char *e)
{
  perror(e);
  exit(1);      
}

// fprintf(stderr); exit();
void Error(const char *fmt, ...)
{
  va_list ap;

  if(!fmt) exit(1);
  va_start(ap, fmt);  
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fflush(stderr);
  exit(1);
}

// fprintf(stderr);
void Warn(const char *fmt, ...)
{
  va_list ap;

  if(!fmt) return;
  va_start(ap, fmt);  
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fflush(stderr);
}

// printf(); with timestamp
void Log(const char *fmt, ...)
{
  va_list ap;

  if(!fmt) return;
  printf("%lu: ",time(NULL));
  va_start(ap, fmt);  
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////

long minl(long a, long b)
{
  if( a < b ) return a;
  return b;
}

////////////////////////////////////////////////////////////////////////////////

int System(const char *fmt, ...)
{
  va_list ap;  char buf[4096];  int rv;

  // !!av: SECURITY_HOLE: buf[]

  // Build command
  if(!fmt) return 0;
  va_start(ap, fmt);  
  vsprintf(buf, fmt, ap);
  va_end(ap);

  // Run command and return success/fail
  if( (rv=system(buf)) == -1 ) 
    return 0;
  return ( WEXITSTATUS(rv) == 0 );
}

////////////////////////////////////////////////////////////////////////////////

int Connect(char *name, int port)
{
  struct sockaddr_in  server;
  struct hostent     *host;
  struct in_addr      addr;
  int                 sock;

  if ( (host=gethostbyname(name)) == NULL )
    return -1;
  addr.s_addr = *((unsigned int*)host->h_addr_list[0]);

  // Setup a client socket
  if ( (sock=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0 ) 
    return -1;
  memset(&server, 0, sizeof(server));
  server.sin_family      = AF_INET;
  server.sin_addr.s_addr = addr.s_addr;
  server.sin_port        = htons(port);

  // Try to connect 
  if ( connect(sock,(struct sockaddr*)&server,sizeof(server)) < 0 ) {
    close(sock);
    return -1;
  }

  return sock;
}

////////////////////////////////////////////////////////////////////////////////

void Yield(int sec, int usec)
{
  struct timeval tv;

  // Sleep for a ms
  tv.tv_sec  = sec;
  tv.tv_usec = usec;
  select(0, NULL,  NULL, NULL, &tv);
}

////////////////////////////////////////////////////////////////////////////////

#endif
