#ifndef PROTOCOL_C
#define PROTOCOL_C

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <string.h>

#include "protocol.h"
#include "util.h"

////////////////////////////////////////////////////////////////////////////////

int RecvData(const int sock, void *d, const int s)
{
  int n,r;  unsigned char sha[SHA_DIGEST_LENGTH],shac[SHA_DIGEST_LENGTH];  SHA_CTX c;

  // Read the data packet
  for(r=s; (r > 0) && ((n=read(sock,d+s-r,r)) > 0); r-=n);
  if( r ) return 0;
  
  // Read the SHA1 hash
  for(r=SHA_DIGEST_LENGTH; (r > 0) && ((n=read(sock,sha+SHA_DIGEST_LENGTH-r,r)) > 0); r-=n);
  if( r ) return 0;

  // Build hash for this packet
  SHA1_Init(&c);
  SHA1_Update(&c,d,s);
  SHA1_Update(&c,ARB_KEY,strlen(ARB_KEY));
  SHA1_Final(shac,&c);

  // Make sure hashes match
  for(n=0; n<SHA_DIGEST_LENGTH; n++)
    if( sha[n] != shac[n] )
      return 0;

  // Return success
  return 1;
}

int SendData(const int sock, const void *d, const int s)
{
  int n,r;  unsigned char sha[SHA_DIGEST_LENGTH];  SHA_CTX c;

  // Build hash for this packet
  SHA1_Init(&c);
  SHA1_Update(&c,d,s);
  SHA1_Update(&c,ARB_KEY,strlen(ARB_KEY));
  SHA1_Final(sha,&c);

  // Write packet data
  for(r=s; (r > 0) && ((n=write(sock,d+s-r,r)) > 0); r-=n);
  if( r ) return 0;

  // Write SHA1 hash
  for(r=SHA_DIGEST_LENGTH; (r > 0) && ((n=write(sock,sha+SHA_DIGEST_LENGTH-r,r)) > 0); r-=n);
  if( r ) return 0;

  // Return success
  return 1;
}

////////////////////////////////////////////////////////////////////////////////

int RecvCommand(const int sock, command_t *cmd)
{
  int rv;

  rv = RecvData(sock, cmd, sizeof(command_t));

  cmd->cmd[sizeof(cmd->cmd)-1] = 0;
  cmd->arg[sizeof(cmd->arg)-1] = 0;

  return rv;
}

int SendCommand(const int sock, const command_t *cmd)
{
  return SendData(sock, cmd, sizeof(command_t));
}

////////////////////////////////////////////////////////////////////////////////

int RecvFile(const int sock, const char *fn)
{
  long b,r;  char buf[512];  command_t cmd;  FILE *f;

  // Get and parse a header for this file transfer from the server
  if( !RecvCommand(sock,&cmd) )       return 0;
  if( sscanf(cmd.arg,"%ld",&b) != 1 ) return 0;
  
  // Open file 
  if( !(f=fopen(fn,"w")) )
    return 0;

  // Read blocks from the socket to the file
  for(r=b; (r > 0) && RecvData(sock, buf, minl(r,sizeof(buf))) && fwrite(buf,minl(r,sizeof(buf)),1,f); r-=sizeof(buf));

  // Close file and return status
  fclose(f);
  return (r <= 0);
}

int SendFile(const int sock, const char *fn)
{
  long b,r;  char buf[512];  command_t cmd;  FILE *f;  int t;

  // Open file
  if( !(f=fopen(fn,"r")) )
    return 0;

  // Build and send a header for this file transfer
  fseek(f,0,SEEK_END);  b = ftell(f);  rewind(f);
  sprintf(cmd.cmd,"%s",fn);
  sprintf(cmd.arg,"%ld",b);
  if( !SendCommand(sock,&cmd) ) {
    fclose(f);
    return 0;
  }

  // Read blocks from file and write to socket
  for(r=b; (r > 0) && (t=fread(buf,minl(r,sizeof(buf)),1,f)) && SendData(sock,buf,minl(r,sizeof(buf))); r-=sizeof(buf));
  // Return success
  fclose(f);
  return (r <= 0);
}

#endif
