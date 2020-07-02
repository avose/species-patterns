#ifndef ARBITER_C
#define ARBITER_C

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>

#include "protocol.h"
#include "util.h"
#include "arbiter.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

char Server[512];      // !!av: from command line! not large enough!
int  Port;

char Path[512];

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void GetCPUVendor(char *s)
{
  FILE *f;  char b[MAX_CMD],*t;

  // Run command
  if( !(f=popen("cat /proc/cpuinfo | grep vendor_id | head -n 1","r")) ) {
    // Can't run command.. uknown processor type
    sprintf(s,"n/a");
    return;
  }

  // Read output
  if( !fgets(b, MAX_CMD, f) ) {
    // No command output. uknown processor type
    sprintf(s,"n/a");
    pclose(f);
    return;
  }
  pclose(f);
  if( b[strlen(b)-1] == '\n' )
    b[strlen(b)-1] = '\0';

  // Parse for cpu vendor string
  t = strtok(b,":");
  t = strtok(NULL,":");
  if( !t ) {
    // Parse error
    sprintf(s,"n/a");
    return;
  }
  while(*t == ' ')
    t++;
  sprintf(s,"%s",t);
}

int SendArbiterInfo(int client)
{
  command_t cmd;
  
  // Build
  gethostname(cmd.cmd,MAX_CMD);
  cmd.cmd[MAX_CMD-1] = '\0';
  GetCPUVendor(cmd.arg);
  
  // Send
  if( !SendCommand(client,&cmd) )
    return 0;

  return 1;
}

//
// Returns the number of isets the governor has waiting to be scheduled.
//
int nQueued() 
{
  int sock,q;  command_t cmd;

  // Connect to server
  if( (sock=Connect(Server, Port)) == -1 ) {
    Log("nQueued: Governor unreachable\n");
    return -1;
  }

  // Ask for a job
  sprintf(cmd.cmd,"queued");
  sprintf(cmd.arg,"unused");
  if( !SendCommand(sock,&cmd) ) goto failure;
  SendArbiterInfo(sock);
  if( !RecvCommand(sock,&cmd) ) goto failure;
  
  // Try to parse response
  if( sscanf(cmd.arg,"%d",&q) != 1 ) goto failure;
  if( !RecvCommand(sock,&cmd) )      goto failure;
  if( strcmp(cmd.arg,"SUCCESS") )    goto failure;
  close(sock);

  // Return number of isets queued
  return q;

 failure:
  // Fail case for after socket is opened
  Log("nQueued(): (%s|%s)\n",cmd.cmd,cmd.arg);
  close(sock);
  return -1;
}

int HandleStatus(const char *sbuf, const char *j)
{
  char *b,buf[MAX_CMD];  int progress,client;  command_t cmd;

  // What the fsck kind of noob programer am I?
  //
  // I have no clue what b,buf or sbuf are supposed to hold here,
  // and their names suck.  I shouldn't code in a way that forces
  // one to look elsewhere...
  //
  // This is my best guess:
  //
  // sbuf holds a string of the form "%c[%s][%s][...]", where the 
  // character represents the status update/request type, and the
  // optional strings are additional information specific to the
  // request type.
  //
  // b holds the addtional information (after the type character).
  //
  // buf is local to each case.

  b = (char*)(&sbuf[1]);
  switch( sbuf[0] ) {
  case 'k':
    ////////////////////////////////////////////////////////////
    // Checkpoint request
    if( b[strlen(b)-1] == '\n' )
      b[strlen(b)-1] = '\0';
    Log("HandleStatus(): %s: checkpointing.\n",j);
    sprintf(cmd.arg,"%s",j);
    sprintf(cmd.cmd,"checkpoint");
    if( !(client=Connect(Server,Port)) )    { return -1; }
    if( !SendCommand(client,&cmd) )         { close(client); return -1; }
    if( !SendArbiterInfo(client) )          { close(client); return -1; }
    if( !RecvCommand(client,&cmd) )         { close(client); return -1; }
    if( strcmp(cmd.arg,"SUCCESS") )         { close(client); return -1; }
    // Tar the checkpoint file and send it to the server
    sprintf(buf,"checkpoint-%d",getpid());
    if( !System("cp -f '%s' '%s'",b,buf) )                       { close(client); return -1; }
    if( !System("tar -zcf '%s.tar.gz' '%s'",buf,buf) )           { close(client); System("rm -f '%s' '%s.tar.gz'",buf,buf); return -1; }
    if( !System("rm '%s' && mv '%s.tar.gz' '%s'",buf,buf,buf) )  { close(client); System("rm -f '%s' '%s.tar.gz'",buf,buf); return -1; }
    if( !SendFile(client,buf) )                                  { close(client); System("rm -f '%s'",buf); return -1; }
    if( !RecvCommand(client,&cmd) )                              { close(client); System("rm -f '%s'",buf); return -1; }
    close(client);
    System("rm '%s'",buf);
    break;

  case 'p':
    ////////////////////////////////////////////////////////////
    // Progress update
    if( sscanf(b,"%d",&progress) != 1 )  { return -1; }
    sprintf(cmd.arg,"p%s:%d",j,progress);
    sprintf(cmd.cmd,"progress");
    if( !(client=Connect(Server,Port)) ) { return -1; }
    if( !SendCommand(client,&cmd) )      { close(client); return -1; }
    if( !SendArbiterInfo(client) )       { close(client); return -1; }
    if( !RecvCommand(client,&cmd) )      { close(client); return -1; }
    if( strcmp(cmd.arg,"SUCCESS") )      { close(client); return -1; }
    close(client);
    break;

  case 'c':
    ////////////////////////////////////////////////////////////
    // Custom status line
    if( b[strlen(b)-1] == '\n' )
      b[strlen(b)-1] = '\0';
    sprintf(cmd.arg,"c%s:%s",j,b);
    sprintf(cmd.cmd,"progress");
    if( !(client=Connect(Server,Port)) ) { return -1; }
    if( !SendCommand(client,&cmd) )      { close(client); return -1; }
    if( !SendArbiterInfo(client) )       { close(client); return -1; }
    if( !RecvCommand(client,&cmd) )      { close(client); return -1; }
    close(client);
    break;

  case 'f':
    ////////////////////////////////////////////////////////////
    // Job is finished
    Log("HandleStatus(): %s says it is finished.\n",j);
    sprintf(cmd.arg,"f%s",j);
    sprintf(cmd.cmd,"progress");
    if( !(client=Connect(Server,Port)) )    { return -1; }
    if( !SendCommand(client,&cmd) )         { close(client); return -1; }
    if( !SendArbiterInfo(client) )          { close(client); return -1; }
    if( !RecvCommand(client,&cmd) )         { close(client); return -1; }
    if( strcmp(cmd.arg,"SUCCESS") )         { close(client); return -1; }
    // Tar the completed run and send it to the server
    System("make clean");
    sprintf(buf,"../RESULTS-%s.tar.gz",j);
    if( !System("tar -zcf '%s' *",buf) ) { close(client); return -1; }
    if( !SendFile(client,buf) )             { close(client); return -1; }
    if( !RecvCommand(client,&cmd) )         { close(client); return -1; }
    close(client);
    System("rm -f '%s'",buf);
    return 2;

  case 'e':
    ////////////////////////////////////////////////////////////
    // Job has suffered an exception
    if( b[strlen(b)-1] == '\n' )
      b[strlen(b)-1] = '\0';
    sprintf(cmd.arg,"e%s:%s",j,b);
    sprintf(cmd.cmd,"progress");
    if( !(client=Connect(Server,Port)) ) { return -1; }
    if( !SendCommand(client,&cmd) )      { close(client); return -1; }
    if( !SendArbiterInfo(client) )       { close(client); return -1; }
    if( !RecvCommand(client,&cmd) )      { close(client); return -1; }
    close(client);
    return 0;

  default:
    // Unknown format
    return -1;
  }

  return 1;
}

//
// Requests a job from the arbiter, downloads it, and returns the filename in fn.
// ck is a flag which is set by RequestJob() to indicate if there was a checkpoint
// file or not.
//
int RequestJob(char *id, int *ck)
{
  int sock;  command_t cmd;  char buf[MAX_CMD];

  // Connect to server
  if( (sock=Connect(Server, Port)) == -1 ) {
    Log("RequestJob(): Governor unreachable\n");
    return 0;
  }

  // Ask for a job
  sprintf(cmd.cmd,"request");
  sprintf(cmd.arg,"unused");
  if( !SendCommand(sock,&cmd) )          { Log("RequestJob():sc0\n"); goto failure; }
  if( !SendArbiterInfo(sock) )           { Log("RequestJob():arb\n"); goto failure; }
  if( !RecvCommand(sock,&cmd) )          { Log("RequestJob():rc0\n"); goto failure; }
  if( strcmp(cmd.cmd,"file") )           { Log("RequestJob():nr\n");  goto failure; }
  strcpy(id,cmd.arg);
  sprintf(buf,"%s.tar.gz",cmd.arg);
  if( !RecvFile(sock, buf) )             { Log("RequestJob():file\n");   goto failure; }
  if( !RecvCommand(sock,&cmd) )          { Log("RequestJob():ck0\n");    goto failure; }
  if( strcmp(cmd.cmd,"checkpoint") )     { Log("RequestJob():ck1\n");    goto failure; }
  if( !strcmp(cmd.arg,"yes") ) {
    *ck = 1;
    if( !RecvFile(sock, "checkpoint") )  { Log("RequestJob():ckfile\n"); goto failure; }
    if( !System("tar -zxf checkpoint") ) { Log("RequestJob():cktar\n");  goto failure; }
    if( !System("rm -f checkpoint") )    { Log("RequestJob():rmck\n");   goto failure; }
  } else {
    *ck = 0;
  }
  if( !RecvCommand(sock,&cmd) )          { Log("RequestJob():rv\n");     goto failure; }
  if( strcmp(cmd.arg,"SUCCESS") )        { Log("RequestJob():rs\n");     goto failure; }
  close(sock);

  // Return success
  return 1;

 failure:
  // Fail case for after socket is opened
  Log("RequestJob(): (%s|%s)\n",cmd.cmd,cmd.arg);
  close(sock);
  System("rm -f '%s' checkpoint*",buf);
  return 0;
}

void CleanUpJobMess(const char *j)
{
  // Move back to parent (arbiter) directory
  if( chdir("..") ) Die("chdir (arb dir)"); 

  // Remove the run direcotry
  System("rm -rf 'DATA-%s'",j);
  
  // Remove the tarball
  System("rm -f '%s.tar.gz'",j);

  // Remove any stale checkpoint files
  System("rm -f checkpoint*");
}

int RunJob(const char *j, const int ck)
{
  char buf[MAX_CMD];  int in[2],rv;  pid_t c;  fd_set fds;  struct timeval tv;  FILE *f;
  
  // Create a dir, copy the iset file and untar the iset.
  sprintf(buf,"DATA-%s",j);
  if( !System("mkdir '%s'",buf) ) Die("mkdir (run dir)");
  if( chdir(buf) )                Die("chdir (run dir)");
  if( !System("tar -zxf '../%s.tar.gz'",j) ) {
    CleanUpJobMess(j);
    return 0;
  }
  
  // Move the checkpoint file in place if needed.
  if( ck ) {
    if( !System("mv ../checkpoint* ./checkpoint") ) {
      CleanUpJobMess(j);
      return 0;
    }
  }

  // !!av: bugfix
  //
  // This really needs to be bi-directional, so that checkpointing can be made
  // synchronous.
  //

  // Open pipe, dup.
  if( pipe(in) )                 Die("pipe()");
  if( dup2(in[1], FD_IN) == -1 ) Die("dup2()");
  close(in[1]);
  if( !(f=fdopen(in[0],"r")) )   Die("fdopen()");
      
  // Fork, and exec the job.
  if( !(c=fork()) ) {
    // Run the child job
    System("make");
    execl("./simulation", "simulation", NULL);
    Die("execl()");
  }

  if( c == -1 )
    Die("fork()");
   
  while(1) {
    // Wait on status data from the client
    FD_ZERO(&fds);
    FD_SET(in[0], &fds);
    tv.tv_sec  = CLIENT_TTL;
    tv.tv_usec = 0;
    if( !select(in[0]+1, &fds, NULL, NULL, &tv) ) {
      // Client timed out;
      Log("RunJob(): Client TTL expired.\n");
      goto failure;
    }
    // Read line from client
    if( !fgets(buf,MAX_CMD,f) ) {
      // Error reading from the client; kill it.
      Log("RunJob(): error reader from client.\n");
      goto failure;
    }

    // React to client message
    if( !(rv=HandleStatus(buf, j)) ) goto failure; // exception
    if( rv == -1 )                   goto failure; // fatal error
    if( rv == 2 ) {
      // Completed
      break;
    }
  }
  
  close(in[0]);
  // This should keep zombies to a minimum
  kill(c, SIGTERM);
  sleep(4);
  kill(c, SIGKILL);
  wait(&rv);
  CleanUpJobMess(j);
  return 1;
  
 failure:
  close(in[0]);
  // This should keep zombies to a minimum
  kill(c, SIGTERM);
  sleep(4);
  kill(c, SIGKILL);
  wait(&rv);
  CleanUpJobMess(j);
  
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void ArbiterLoop()
{
  int n,ck;  char buf[MAX_CMD];

  // Loop forever, but the Yield() ensures that job requests are
  // sent at most once every STATUS_RATE seconds.
  for(; 1; Yield(STATUS_RATE,0)) {

    // Get number of queued isets:
    n = nQueued();
    if( n < 1 ) {
      // No work to do
      Log("ArbiterLoop(): no work.\n");
      continue;
    }
    Log("ArbiterLoop(): %d isets queued on governor.\n",n);

    // Ask for a new job.
    if( !RequestJob(buf,&ck) ) {
      Log("ArbiterLoop(): RequestJob() failed.\n");
      continue;
    } else {
      Log("Job: %s.\n",buf);
    }

    // Run the new job
    if( !RunJob(buf,ck) ) {
      Log("ArbiterLoop(): StartJob() failed.\n");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void UsageError()
{
  fprintf(stderr,"Usage:\n\tarbiter <port:host> <datadir>\n");
  exit(1);
}

void sig_hndlr(int sig)
{
  // This may be cleaner than waiting for a SIGKILL to come as a followup.
  exit(1);
}

int main(int argc, char **argv)
{
  char name[128];  sigset_t s;

  // !!av: make this a cmdline opt
  // Drop root
  setuid(3337);

  // Setup signal handler
  signal(SIGTERM, sig_hndlr);
  if( sigemptyset(&s) )                  Die("sigemptyset()");
  if( sigaddset(&s, SIGPIPE) )           Die("sigaddset()");
  if( sigprocmask(SIG_BLOCK, &s, NULL) ) Die("sigprocmask()");

  if( argc != 3 )                                 UsageError();
  if( sscanf(argv[1],"%d:%s",&Port,Server) != 2 ) UsageError();
  if( sscanf(argv[2],"%s",Path) != 1 )            UsageError();  

  // Basic startup
  if( chdir(Path) )
    Error("Cannot chdir to \"%s\".\n",Path);
  if( gethostname(name, sizeof(name)) )
    Die("main(): gethostname()");
  Log("Starting arbiter on %s slave to %s:%d.\n", name, Server, Port);

  // Start the arbiter loop
  ArbiterLoop();

  return 0;
}

#endif
