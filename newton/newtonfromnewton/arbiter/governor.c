#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "protocol.h"
#include "util.h"
#include "governor.h"
#include "random.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

arbiter_t Arbiters[MAX_ARBITERS];
int       nArbiters;

job_t     Jobs[MAX_JOBS];
int       nJobs;

pthread_mutex_t Mutex;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void LockMutex(pthread_mutex_t *m)
{
  if( pthread_mutex_lock(m) ) 
    Die("pthread_mutex_lock()");
}

void UnlockMutex(pthread_mutex_t *m)
{
  if( pthread_mutex_unlock(m) ) 
    Die("pthread_mutex_unlock()");
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void SaveState()
{
  char fn[512];  FILE *f;  int i,j,k;

  // Open checkpoint file
  sprintf(fn,"checkpoint");
  if( !(f=fopen(fn,"w")) )
    return;

  // Dump arbiters
  fprintf(f,"na:%d\n",nArbiters);
  for(i=0; i<nArbiters; i++) {
    //char   name[MAX_CMD];
    //char   cpu[MAX_CMD];
    //time_t speed;
    //time_t time;
    //int    isets;
    fprintf(f,"a:%d:%lu:%lu:%s\n",
	    Arbiters[i].isets,Arbiters[i].time,Arbiters[i].speed,Arbiters[i].name);
    fprintf(f,"a:%s\n",
	    Arbiters[i].cpu);
  }

  // Dump jobs
  fprintf(f,"nj:%d\n",nJobs);
  for(i=0; i<nJobs; i++) {
    //char    id[];
    //int     priority;
    //int     runs;
    //set_t  *sets;
    //int     nsets;
    //time_t  stime;
    //time_t  etime;
    //int     flags;
    fprintf(f,"j:%d:%d:%lu:%lu:%d:%s\n",
	    Jobs[i].priority,Jobs[i].runs,Jobs[i].stime,Jobs[i].etime,Jobs[i].flags,Jobs[i].id);
    // Dump sets
    fprintf(f,"ns:%d\n",Jobs[i].nsets);
    for(j=0; j<Jobs[i].nsets; j++) {
      //char    file[];
      //iset_t *isets;
      //int     nisets;
      fprintf(f,"s:%s\n",
	      Jobs[i].sets[j].file);
      // Dump isets
      fprintf(f,"ni:%d\n",Jobs[i].sets[j].nisets);
      for(k=0; k<Jobs[i].sets[j].nisets; k++) {
	//char      id[MAX_CMD];
	//time_t    stime;
	//time_t    etime;
	//time_t    ptime;
	//int       progress;
	//char      arbiter[MAX_CMD];
	//char      custom[MAX_CMD];
	//char      chkpnt[MAX_CMD];
	fprintf(f,"i:%lu:%lu:%lu:%s\n",
		Jobs[i].sets[j].isets[k].stime,Jobs[i].sets[j].isets[k].etime,Jobs[i].sets[j].isets[k].ptime,
		Jobs[i].sets[j].isets[k].id);
	fprintf(f,"i:%d:%s\n",
		Jobs[i].sets[j].isets[k].progress,Jobs[i].sets[j].isets[k].custom);
	fprintf(f,"i:%s\n",
		Jobs[i].sets[j].isets[k].arbiter);
	fprintf(f,"i:%s\n",
		Jobs[i].sets[j].isets[k].chkpnt);
      }
    }
  }
  
  fclose(f);
}

void RestoreState()
{
  char ti,tc;  FILE *f;  int i,j,k;

  // Open checkpoint file
  if( !(f=fopen("checkpoint","r")) )
    return;

  // Restore arbiters
  if( fscanf(f,"na:%d\n",&nArbiters) != 1 ) Error("RestoreState(): nArbiters\n");
  for(i=0; i<nArbiters; i++) {
    //char   name[MAX_CMD];
    //char   cpu[MAX_CMD];
    //time_t speed;
    //time_t time;
    //int    isets;
    if( fscanf(f,"a:%d:%lu:%lu:%s\n",
	       &Arbiters[i].isets,&Arbiters[i].time,&Arbiters[i].speed,Arbiters[i].name) != 4 )
      Error("RestoreState(): Arbiter.\n");
    if( fscanf(f,"a:%s\n",Arbiters[i].cpu) != 1 )
      Error("RestoreState(): Arbiter2.\n");
  }

  // Restore jobs
  if( fscanf(f,"nj:%d\n",&nJobs) != 1 ) Error("RestoreState(): nJobs\n");
  for(i=0; i<nJobs; i++) {
    //char    id[64];
    //int     priority;
    //int     runs;
    //set_t  *sets;
    //int     nsets;
    //time_t  stime;
    //time_t  etime;
    //int     flags;
    if( fscanf(f,"j:%d:%d:%lu:%lu:%d:%s\n",
	       &Jobs[i].priority,&Jobs[i].runs,&Jobs[i].stime,&Jobs[i].etime,&Jobs[i].flags,Jobs[i].id)
	!= 6 ) {
      Error("RestoreState(): Job\n");
    }
    // Restore sets
    if( fscanf(f,"ns:%d\n",&Jobs[i].nsets) != 1 ) 
      Error("RestoreState(): nsets\n");
    if( !(Jobs[i].sets=malloc(Jobs[i].nsets*sizeof(set_t))) ) 
      Error("RestoreState(): malloc sets\n");
    for(j=0; j<Jobs[i].nsets; j++) {
      //char    file[64];
      //iset_t *isets;
      //int     nisets;
      if( fscanf(f,"s:%s\n",Jobs[i].sets[j].file) != 1 ) 
	Error("RestoreState(): Set\n");
      // Dump isets
      if( fscanf(f,"ni:%d\n",&Jobs[i].sets[j].nisets) != 1 )
 	Error("RestoreState(): nisets\n");
      if( !(Jobs[i].sets[j].isets=malloc(Jobs[i].sets[j].nisets*sizeof(iset_t))) ) 
	Error("RestoreState(): malloc isets\n");
      for(k=0; k<Jobs[i].sets[j].nisets; k++) {
	//char      id[MAX_CMD];
	//time_t    stime;
	//time_t    etime;
	//time_t    ptime;
	//int       progress;
	//char      arbiter[MAX_CMD];
	//char      custom[MAX_CMD];
	//char      chkpnt[MAX_CMD];
	if( fscanf(f,"i:%lu:%lu:%lu:%s\n",
		   &Jobs[i].sets[j].isets[k].stime,&Jobs[i].sets[j].isets[k].etime,&Jobs[i].sets[j].isets[k].ptime,
		   Jobs[i].sets[j].isets[k].id) 
	    != 4 )
	  Error("RestoreState(): (%s) (%s) iset %d/%d\n",Jobs[i].id,Jobs[i].sets[j].file,k,Jobs[i].sets[j].nisets);
	if( fscanf(f,"i:%d:", &Jobs[i].sets[j].isets[k].progress) != 1 )
	  Error("RestoreState(): iset2\n");
	if( !fgets(Jobs[i].sets[j].isets[k].custom, MAX_CMD, f) ) {
	  Jobs[i].sets[j].isets[k].custom[0] = '\0';
	} else {
	  Jobs[i].sets[j].isets[k].custom[strlen(Jobs[i].sets[j].isets[k].custom)-1] = '\0';
	}
	if( fscanf(f,"i:%s\n",Jobs[i].sets[j].isets[k].arbiter) != 1 )
	  Error("RestoreState(): iset3\n");
	if( fscanf(f,"%c%c", &ti, &tc) != 2 )
	  Error("RestoreState(): iset4\n");
	if( (ti != 'i') || (tc != ':') )
	  Error("RestoreState(): iset4hdr\n");
	if( !fgets(Jobs[i].sets[j].isets[k].chkpnt, MAX_CMD, f) ) {
	  Jobs[i].sets[j].isets[k].chkpnt[0] = '\0';
	} else {
	  Jobs[i].sets[j].isets[k].chkpnt[strlen(Jobs[i].sets[j].isets[k].chkpnt)-1] = '\0';
	}
      }
    }
  }
  
  fclose(f);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//
// Returns the set name of the set (prefix on file)
//
void SetfiletoSetname(const char *src, char **set)
{
  char *t;

  // Find .tar.gz
  if( !(t=strstr(src, ".tar.gz")) )
    Error("SetfiletoSetname(): src does not contain \".tar.gz\"");
  memcpy(*set,src,t-src);
  (*set)[t-src] = '\0';
}

//
// Returns the index of the match or -1 on not found.
//
int FindJob(const char *id)
{
  int i;

  for(i=0; i<nJobs; i++)
    if( !strcmp(id,Jobs[i].id) )
      return i;
  
  // ID not found.
  return -1;
}

//
// Returns the index of the match or -1 on not found.
//
int FindArbiter(const char *id)
{
  int i;

  for(i=0; i<nArbiters; i++)
    if( !strcmp(id,Arbiters[i].name) )
      return i;
  
  // ID not found.
  return -1;
}

job_t *FindJobofSeti(const char *id)
{
  int j,s,i;  job_t *job;

  // !!av: teh lamez0r!!
  for(j=0; j<nJobs; j++) {
    job = &Jobs[j];
    for(s=0; s<job->nsets; s++) {
      for(i=0; i<job->sets[s].nisets; i++) {
	// Look for a match
	if( !strcmp(job->sets[s].isets[i].id,id) ) {
	  return job;
	}
      }
    }
  }

  // We made it through the whole job and all isets are complete
  return NULL;
}

iset_t *FindSeti(const char *id)
{
  int j,s,i;  job_t *job;

  // !!av: teh lamez0r!!
  for(j=0; j<nJobs; j++) {
    job = &Jobs[j];
    for(s=0; s<job->nsets; s++) {
      for(i=0; i<job->sets[s].nisets; i++) {
	// Look for a match
	if( !strcmp(job->sets[s].isets[i].id,id) ) {
	  return &job->sets[s].isets[i];
	}
      }
    }
  }

  // We made it through the whole job and all isets are complete
  return NULL;
}

//
// Returns 1 if the job is completed; else 0.
//
int isCompleted(const job_t *job)
{
  int s,i;

  for(s=0; s<job->nsets; s++) {
    if( job->sets[s].nisets < job->runs )
      return 0;
    for(i=0; i<job->sets[s].nisets; i++)
      if( (!job->sets[s].isets[i].etime) || (!job->sets[s].isets[i].stime) )
	return 0;
  }

  // We made it through the whole job and all isets are complete
  return 1;
}

//
// Returns 1 if the job is scheduled; else 0.
//
int isScheduled(const job_t *job)
{
  int s,i;

  for(s=0; s<job->nsets; s++) {
    if( job->sets[s].nisets < job->runs )
      return 0;
    for(i=0; i<job->sets[s].nisets; i++)
      if( !job->sets[s].isets[i].stime )
	return 0;
  }

  // We made it through the whole job and all isets are complete
  return 1;
}

// 
// Synchronizes the flags with the state of the isets ( isets -> flags )
//
void SyncJob(job_t *job)
{
  // Check for completion
  if( isScheduled(job) ) { 
    job->flags |=  FLAG_SCHEDULED; 
  } else {
    job->flags &= ~FLAG_SCHEDULED;
  }

  // Check for completion
  if( isCompleted(job) ) { 
    job->flags |=  FLAG_COMPLETE; 
    if( !job->etime ) {
      job->etime = time(NULL);
    }
  } else {
    job->etime = 0;
    job->flags &= ~FLAG_COMPLETE;
  }
}

//
// Returns the index of the which will be scheduled next, -1
// indicates no job.
// As a side effect, SyncJob() will be called on all jobs.
// This function assumes the parent manages the lock.
//
int NextJob()
{
  int j,n;  static dist *sched_dist=NULL;  double s;

  // Initialize probability scheduler
  if( !sched_dist ) {
    initrand(time(NULL));
    sched_dist = allocdist(MAX_JOBS);
  }

  for(j=n=0,s=0.0; j<nJobs; j++) {

    // Check the job for any maintenance needs or status changes
    SyncJob(&Jobs[j]);

    // Jobs in any combination of these states are not 
    // considered for scheduling.
    if( (Jobs[j].flags & FLAG_PAUSED)    ||
	(Jobs[j].flags & FLAG_B0RKED)    ||
	(Jobs[j].flags & FLAG_ZOMBIE)    ||
	(Jobs[j].flags & FLAG_SCHEDULED) ||
	(Jobs[j].flags & FLAG_COMPLETE)     ) {
      sched_dist->p[j] = 0.0;
      continue;
    }

    // The job looks runnable, assign it a weighted probability.
    s += sched_dist->p[j] = Jobs[j].priority;
    n++;
  }

  // If anything needs to be scheduled, choose a weighted random job.
  if( n && s ) {
    sched_dist->n = nJobs;
    initdist(sched_dist, s);
    if( (n=drand(sched_dist)) >= nJobs )
      Error("drand(sched_dist) out of bounds\n");
    return n;
  }

  // Return none-runnable
  return -1;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int CmdRequest(const int client, command_t *cmd)
{
  char buf[512],*b; // size: sprintf(buf,"%s/%s",jb.id,st.file);
  int i,j,s;  job_t jb;  set_t st;  iset_t is,*in;  command_t rsp,arb;

  // Get additional arbiter data
  if( !RecvCommand(client,&arb) ) { sprintf(cmd->arg,"FAILURE:request:arbiter"); return 0; }
  LockMutex(&Mutex);
  if( (i=FindArbiter(arb.cmd)) < 0 ) { 
    // Room for new arbiter?
    if( nArbiters >= MAX_ARBITERS ) {
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:request:arbiter:full"); 
      return 0; 
    }
    // Add a new arbiter
    i = nArbiters;
    nArbiters++;
    Arbiters[i].speed = 0;
    Arbiters[i].isets = 0;
    sprintf(Arbiters[i].name,"%s",arb.cmd);
    sprintf(Arbiters[i].cpu,"%s",arb.arg);
  }
  Arbiters[i].time = time(NULL);

  // Pick the next job to schedule
  if( (j=NextJob()) == -1 ){
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:no jobs"); 
    return 0;
  }

  // Pick an iset from this job to schedule
  for(s=0; s<Jobs[j].nsets; s++) {
    // First, check for isets that need to be re-scheduled
    for(i=0; i<Jobs[j].sets[s].nisets; i++) {
      if( !Jobs[j].sets[s].isets[i].ftflags && !Jobs[j].sets[s].isets[i].stime ) {
	// Make a local copy of the iset to be rescheduled
	memcpy(&jb,&Jobs[j],sizeof(job_t));
	memcpy(&st,&Jobs[j].sets[s],sizeof(set_t));
	sprintf(Jobs[j].sets[s].isets[i].arbiter,"%s",arb.cmd);
	memcpy(&is,&Jobs[j].sets[s].isets[i],sizeof(iset_t));
	// Flag the iset in memory so that we know it's being scheduled.
	Jobs[j].sets[s].isets[i].ftflags |= FFLAG_SCHEDULE;
	goto schedule;
      }
    }
    // Now, see if there is a new iset that needs to be scheduled
    if( Jobs[j].sets[s].nisets < Jobs[j].runs ) {
      // Fill in local copy of iset
      memcpy(&jb,&Jobs[j],sizeof(job_t));
      memcpy(&st,&Jobs[j].sets[s],sizeof(set_t));
      memset(&is,0,sizeof(iset_t));
      b = strstr(st.file,".tar.gz");
      *b = 0;
      sprintf(is.id,"%s_%s_%d",jb.id,st.file,i);
      *b = '.';
      sprintf(is.arbiter,"%s",arb.cmd);
      // Allocate space for the new iset and initialize it.  Also set the scheduling flag
      // so that we know it's being scheduled.
      i = Jobs[j].sets[s].nisets++;
      if( !(Jobs[j].sets[s].isets=realloc(Jobs[j].sets[s].isets,Jobs[j].sets[s].nisets*sizeof(iset_t))) )
	Die("CmdRequest(): realloc(iset)");
      memset(&Jobs[j].sets[s].isets[i],0,sizeof(iset_t));
      Jobs[j].sets[s].isets[i].ftflags = FFLAG_SCHEDULE;
      sprintf(Jobs[j].sets[s].isets[i].arbiter,"%s",arb.cmd);
      b = strstr(st.file,".tar.gz");
      *b = 0;
      sprintf(Jobs[j].sets[s].isets[i].id,"%s_%s_%d",jb.id,st.file,i);
      *b = '.';
      goto schedule;	
    }
  }
  UnlockMutex(&Mutex);
  sprintf(cmd->arg,"FAILURE:internal scheduling error"); 
  return 0;

 schedule:
  // Send OK, file name, and file
  UnlockMutex(&Mutex);
  sprintf(rsp.cmd,"file");
  sprintf(rsp.arg,"%s",is.id);
  sprintf(buf,"%s/%s",jb.id,st.file);
  if( !SendCommand(client, &rsp) ) { 
    // Cleanup and return failure 
    LockMutex(&Mutex);
    if( (in=FindSeti(is.id)) ) in->ftflags &= ~FFLAG_SCHEDULE;
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:fn");   
    return 0; 
  }
  if( !SendFile(client, buf) )     { 
    LockMutex(&Mutex);
    if( (in=FindSeti(is.id)) ) in->ftflags &= ~FFLAG_SCHEDULE;
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:file"); 
    return 0;
  }

  // Send checkpoint info + file (if needed)
  if( is.chkpnt[0] ) {
    // Need to send checkpoint data
    sprintf(rsp.cmd,"checkpoint");
    sprintf(rsp.arg,"yes");
    sprintf(buf,"%s/%s",jb.id,is.chkpnt);
    if( !SendCommand(client, &rsp) ) { 
      // Cleanup and return failure 
      LockMutex(&Mutex);
      if( (in=FindSeti(is.id)) ) in->ftflags &= ~FFLAG_SCHEDULE;
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:ckfn"); 
      return 0;
    }
    if( !SendFile(client, buf) )     { 
      LockMutex(&Mutex);
      if( (in=FindSeti(is.id)) ) in->ftflags &= ~FFLAG_SCHEDULE;
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:ckfile"); 
      return 0; 
    }
  } else {
    // No checkpoint data
    sprintf(rsp.cmd,"checkpoint");
    sprintf(rsp.arg,"no");
    if( !SendCommand(client, &rsp) ) { 
      // Cleanup and return failure 
      LockMutex(&Mutex);
      if( (in=FindSeti(is.id)) ) in->ftflags &= ~FFLAG_SCHEDULE;
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:ckfn");  
      return 0; 
    }
  }

  // The NET IO seemed to work, aquire the lock and make sure our iset is still here
  LockMutex(&Mutex);
  if( !(in=FindSeti(is.id)) ) {
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:removed"); 
    return 0;
  }

  // Init times
  is.etime = 0;
  is.stime = time(NULL);
  is.ptime = is.stime;

  // Copy the new iset from stack to heap.  Note that this will also
  // clear the schedule flag, as the local stack copy's schedule flag
  // is not set.
  memcpy(in,&is,sizeof(iset_t));

  // Unlock and return success
  UnlockMutex(&Mutex);
  return 1;
}

int CmdCheckpoint(const int client, command_t *cmd)
{
  int p;  char *b = &cmd->arg[0],buf[MAX_CMD];  command_t rsp,arb;  iset_t *is;  job_t *jb;

  // Get additional arbiter data
  if( !RecvCommand(client,&arb) ) { sprintf(cmd->arg,"FAILURE:checkpoint:arbiter"); return 0; }
  LockMutex(&Mutex);
  if( (p=FindArbiter(arb.cmd)) < 0 ) { 
    // The initial iset request should have registered this arbiter.
    // If not found, there is probably some kind of error.
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:progress:arbiter:not found"); 
    return 0; 
  }
  Arbiters[p].time = time(NULL);
  // Make sure the desired iset exists
  if( !(is=FindSeti(b)) ) { 
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:not found"); 
    return 0; 
  }
  // Make sure the arbiter offering the checkpoint actually owns
  // the iset it is trying to update.
  if( strcmp(arb.cmd,is->arbiter) ) {
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:not owner"); 
    return 0; 
  }

  ////////////////////////////////////////////////////////////
  // Save checkpoint data
    
  // Tag this run as being checkpointed
  is->ftflags |= FFLAG_CHECKPOINT;
  UnlockMutex(&Mutex);
  
  // Tell arbiter job is found and download checkpoint data from arbiter
  sprintf(rsp.cmd,"%s",b);
  sprintf(rsp.arg,"SUCCESS");
  sprintf(buf,"%s-chkpnt-new",b);
  if( !SendCommand(client,&rsp) ) { 
    // SECCESS response failed; clear the checkpoint flag
    LockMutex(&Mutex);
    if( (is=FindSeti(b)) ) is->ftflags &= ~FFLAG_CHECKPOINT;
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:rv");   
    return 0; 
  }
  if( !RecvFile(client,buf) )     {
    // Checkpoint upload failed, clear the checkpoint flag
    LockMutex(&Mutex);
    if( (is=FindSeti(b)) ) is->ftflags &= ~FFLAG_CHECKPOINT;
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:file"); 
    return 0; 
  }
  
  // File is uploaded, verify that the job still exists and finalize the upload.
  LockMutex(&Mutex);
  if( !(is=FindSeti(b)) || !(jb=FindJobofSeti(b)) ) { 
    if( is ) is->ftflags &= ~FFLAG_CHECKPOINT;
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:not found"); 
    return 0; 
  }
  // The job is still here after regaining the lock
  if( !System("mv %s %s/%s-chkpnt",buf,jb->id,b) ) { 
    // Installation of checkpoint file failed; clear the checkpoint flag
    is->ftflags &= ~FFLAG_CHECKPOINT;
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:mv"); 
    return 0; 
  }
  sprintf(is->chkpnt,"%s-chkpnt",b);
  // Checkpointing finished; set time and clear the checkpoint flag
  is->ptime = time(NULL);
  is->ftflags &= ~FFLAG_CHECKPOINT;

  // Unlock and return success
  UnlockMutex(&Mutex);
  return 1;
}

int CmdProgress(const int client, command_t *cmd)
{
  char *b = &cmd->arg[1],buf[MAX_CMD],*s,*d,*id;  iset_t *is;  int p;  job_t *jb;  command_t rsp,arb;

  // Get additional arbiter data
  if( !RecvCommand(client,&arb) ) { sprintf(cmd->arg,"FAILURE:progress:arbiter"); return 0; }
  LockMutex(&Mutex);
  if( (p=FindArbiter(arb.cmd)) < 0 ) { 
    // The initial iset request should have registered this arbiter.
    // If not found, there is probably some kind of error.
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:progress:arbiter:not found"); 
    return 0; 
  }
  Arbiters[p].time = time(NULL);
  // Make sure the requested iset exists
  if( !(id=strtok_r(b,":",&s)) ) { UnlockMutex(&Mutex); sprintf(cmd->arg,"FAILURE:progress:parse:id"); return 0; }
  if( !(is=FindSeti(id)) ) { 
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:not found"); 
    return 0;
  }
  // Make sure the arbiter offering an update for an iset matches
  // the arbiter the iset lists as its owner.
  if( strcmp(arb.cmd,is->arbiter) ) {
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:not owner"); 
    return 0; 
  }
  UnlockMutex(&Mutex);

  // Parse progress update type
  switch( cmd->arg[0] ) {
  case 'p':
    ////////////////////////////////////////////////////////////
    // Progress update
    if( !(d=strtok_r(NULL,":",&s)) ) { sprintf(cmd->arg,"FAILURE:progress:parse:tok"); return 0; }
    if( sscanf(d,"%d",&p) != 1 )     { sprintf(cmd->arg,"FAILURE:progress:parse:pr"); return 0; }

    LockMutex(&Mutex);
    if( !(is=FindSeti(id)) ) { 
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:not found"); 
      return 0; 
    }
    is->ptime    = time(NULL);
    is->progress = p;
    UnlockMutex(&Mutex);
    break;

  case 'e':
    ////////////////////////////////////////////////////////////
    // Job has suffered an exception
    if( !(d=strtok_r(NULL,":",&s)) )  { sprintf(cmd->arg,"FAILURE:progress:parse"); return 0; }

    LockMutex(&Mutex);
    if( !(is=FindSeti(id)) ) { 
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:not found"); 
      return 0; 
    }
    // The iset needs to be marked as not-scheduled so it can be rescheduled
    is->ptime = is->stime = 0;
    is->arbiter[0] = '\0';
    strcpy(is->custom,d);
    UnlockMutex(&Mutex);
    break;

  case 'c':
    ////////////////////////////////////////////////////////////
    // Custom status line
    if( !(d=strtok_r(NULL,":",&s)) )  { sprintf(cmd->arg,"FAILURE:progress:parse"); return 0; }

    LockMutex(&Mutex);
    if( !(is=FindSeti(id)) ) { 
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:not found"); 
      return 0; 
    }
    is->ptime = time(NULL);
    strcpy(is->custom,d);
    UnlockMutex(&Mutex);
    break;

  case 'f':
    ////////////////////////////////////////////////////////////
    // Job is finished
    
    // Make sure the job exists
    LockMutex(&Mutex);
    if( !(is=FindSeti(id)) ) { 
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:not found"); 
      return 0; 
    }
    UnlockMutex(&Mutex);

    // Tell the arbiter the job is found and download the completed 
    // iset from the arbiter
    sprintf(rsp.arg,"SUCCESS");
    sprintf(buf,"%s.tar.gz",id);
    if( !SendCommand(client,&rsp) ) { sprintf(cmd->arg,"FAILURE:rv");   return 0; }
    if( !RecvFile(client,buf) )     { sprintf(cmd->arg,"FAILURE:file"); return 0; }

    // File is uploaded, verify that the job still exists and
    // finalize the upload / finished process.
    LockMutex(&Mutex);
    if( !(is=FindSeti(id)) || !(jb=FindJobofSeti(id)) ) { 
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:not found"); 
      return 0; 
    }
    // The job is still here after regaining the lock
    if( !System("mv %s %s/",buf,jb->id) ) { 
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:mv"); 
      return 0; 
    }
    is->progress = 100;
    is->etime    = time(NULL);
    is->ptime    = is->etime;
    // Update the iset's arbiter
    if( (p=FindArbiter(arb.cmd)) < 0 ) { 
      // This is an odd case... just ignore the arbiter update, as
      // the completed iset is more important here.  Return success.
      UnlockMutex(&Mutex);
      break;
    }
    Arbiters[p].isets++;
    Arbiters[p].time  =  time(NULL);
    Arbiters[p].speed += is->etime - is->stime;
    UnlockMutex(&Mutex);
    break;

  default:
    sprintf(cmd->arg,"FAILURE:progress:type");
    return 0;
  }

  return 1;
}

int CmdQueued(const int client, command_t *cmd)
{
  int j,s,i,n;  command_t arb;

  // Get additional arbiter data
  if( !RecvCommand(client,&arb) ) { sprintf(cmd->arg,"FAILURE:queued:arbiter"); return 0; }
  LockMutex(&Mutex);
  if( (i=FindArbiter(arb.cmd)) < 0 ) { 
    // Room for new arbiter?
    if( nArbiters >= MAX_ARBITERS ) {
      UnlockMutex(&Mutex);
      sprintf(cmd->arg,"FAILURE:request:arbiter:full"); 
      return 0; 
    }
    // Add a new arbiter
    i = nArbiters;
    nArbiters++;
    Arbiters[i].speed = 0;
    Arbiters[i].isets = 0;
    sprintf(Arbiters[i].name,"%s",arb.cmd);
    sprintf(Arbiters[i].cpu,"%s",arb.arg);
  }
  Arbiters[i].time = time(NULL);
  UnlockMutex(&Mutex);

  // Count number of isets left to be scheduled
  LockMutex(&Mutex);
  for(j=n=0; j<nJobs; j++) {
    for(s=0; s<Jobs[j].nsets; s++) {
      n += Jobs[j].runs-Jobs[j].sets[s].nisets;
      for(i=0; i<Jobs[j].sets[s].nisets; i++)
	if( !Jobs[j].sets[s].isets[i].stime )
	  n++;
    }
  }
  UnlockMutex(&Mutex);

  // Send number of isets left to arbier
  sprintf(cmd->arg,"%d",n);
  if( !SendCommand(client,cmd) ) {
    sprintf(cmd->arg,"FAILURE:send queued");
    return 0;
  }
  
  // Return success
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// This removes all checkpoint files and memory entries
// associated with the specified job.
int CmdCheckpointFlush(const int client, command_t *cmd)
{
  int i,j,k;

  LockMutex(&Mutex);

  // Search to see if we have a matching job
  if( (i=FindJob(cmd->arg)) != -1 ) {
    // Job found; strip all checkpoint files from isets
    for(j=0; j<Jobs[i].nsets; j++)
      for(k=0; k<Jobs[i].sets[j].nisets; k++)
	Jobs[i].sets[j].isets[k].chkpnt[0] = '\0';
    // Return success
    UnlockMutex(&Mutex);
    return 1;
  }

  // Job not found
  UnlockMutex(&Mutex);
  sprintf(cmd->arg,"FAILURE:not found");
  return 0;
}

// This will cause the specified run to be marked as not running.
// The arbiter running this run will try to update at some point in
// the future, at which point the update will fail, and the job will
// be terminated.
int CmdTerminate(const int client, command_t *cmd)
{
  iset_t *is;

  LockMutex(&Mutex);

  // Search to see if we have a matching iset
  if( (is=FindSeti(cmd->arg)) ) {
    // Found a match; reset the run
    is->ptime = is->stime = 0;
    is->arbiter[0] = '\0';
    UnlockMutex(&Mutex);
    return 1;
  }

  // Job not found
  UnlockMutex(&Mutex);
  sprintf(cmd->arg,"FAILURE:not found");
  return 0;
}

int CmdStatus(const int client, command_t *cmd)
{
  char buf[64];  // size: should be large enough for file-descriptor
  int  i;

  // Sync flags and save state
  LockMutex(&Mutex);
  for(i=0; i<nJobs; i++)
    SyncJob(&Jobs[i]);
  SaveState();
  System("cp checkpoint %d",client);
  UnlockMutex(&Mutex);

  // Send the state file
  sprintf(buf,"%d",client);
  if( !SendFile(client,buf) ) {
    sprintf(cmd->cmd,"FAILURE:file");
    System("rm -f %d",client);
    return 0;
  }

  // Return success
  System("rm -f %d",client);
  return 1;
}

////////////////////////////////////////////////////////////////////////////////

int CmdJob(int client, command_t *cmd)
{
  char buf[1024]; // size: sprintf(buf,"tar -ztf %s.%d.tar.gz | grep .tar.gz",job,client);
  int i;  char *job,*runs,*s;  FILE *f;  job_t j;

  // Parse the argument string
  memset(&j,0,sizeof(job_t));
  if( !(job=strtok_r(cmd->arg, ":",&s)) ) { sprintf(cmd->arg,"FAILURE:parse:job");  return 0; }
  if( !(runs=strtok_r(NULL,    ":",&s)) ) { sprintf(cmd->arg,"FAILURE:parse:runs"); return 0; }
  if( sscanf(runs,"%d",&j.runs) != 1 )    { sprintf(cmd->arg,"FAILURE:parse:runs"); return 0; }
  strcpy(j.id,job);

  // Give the job a default priority of '50'; people like percents
  j.priority = 50;

  // Download job file
  sprintf(buf,"%s.%d.tar.gz",job,client);
  if( !RecvFile(client, buf) ) { 
    sprintf(cmd->arg,"FAILURE:file"); 
    return 0; 
  }

  // Parse jobball for sets
  sprintf(buf,"tar -ztf %s.%d.tar.gz | grep .tar.gz",job,client);
  if( !(f=popen(buf,"r")) ) {
    sprintf(cmd->arg,"FAILURE:file"); 
    goto failure; 
  }
  while( fgets(buf, MAX_CMD, f) ) {
    // Strip any newlines from the end of the setball names
    while( (i=strlen(buf)) && buf[i-1] == '\n' )
      buf[i-1] = '\0';
    // Allocate space for a new set
    j.nsets++;
    if( !(j.sets=realloc(j.sets,j.nsets*sizeof(set_t))) ) {
      sprintf(cmd->arg,"FAILURE:realloc()");
      goto failure;
    }
    // Fill in the new set
    memset(&j.sets[j.nsets-1],0,sizeof(set_t));
    strcpy(j.sets[j.nsets-1].file,buf);
  }
  if( !j.nsets ) {
    sprintf(cmd->arg,"FAILURE:bad jobball");
    goto failure;
  }
  pclose(f);
  f = NULL;

  // Now add the job to the job queue
  LockMutex(&Mutex);

  // Make sure there is room and the ID is unique
  if( nJobs+1 >= MAX_JOBS ) {
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:max jobs");
    goto failure;
  }
  if( FindJob(j.id) != -1 ) {
    UnlockMutex(&Mutex);
    sprintf(cmd->arg,"FAILURE:job exists");
    goto failure;
  }

  // Install and expand tarball
  sprintf(buf,"%s.%d.tar.gz",job,client);
  if( !System("mkdir %s",j.id) )                 { sprintf(cmd->arg,"FAILURE:mkdir"); UnlockMutex(&Mutex); goto failure; }
  if( !System("mv %s %s",buf,j.id) )             { sprintf(cmd->arg,"FAILURE:mv");    UnlockMutex(&Mutex); goto failure; }
  if( !System("cd %s && tar -zxf %s",j.id,buf) ) { sprintf(cmd->arg,"FAILURE:cd");    UnlockMutex(&Mutex); goto failure; }
  if( !System("rm %s/%s",j.id,buf) )             { sprintf(cmd->arg,"FAILURE:rm");    UnlockMutex(&Mutex); goto failure; }

  // Add the new job to the queue
  j.stime = time(NULL);
  memcpy(&Jobs[nJobs], &j, sizeof(job_t));
  nJobs++;

  // Return success
  UnlockMutex(&Mutex);
  return 1;

 failure:
  // Fail case for after the job ball has been downloaded
  if( j.sets ) free(j.sets);
  if( f )      pclose(f);
  System("rm -f %s.%d.tar.gz",j.id,client);
  return 0;
}

int CmdRemove(int client, command_t *cmd)
{
  int i,j;  job_t job;

  // Search to see if we have a matching job
  LockMutex(&Mutex);
  if( (i=FindJob(cmd->arg)) != -1 ) {
    // Found a match; free
    for(j=0; j<Jobs[i].nsets; j++)
      free(Jobs[i].sets[j].isets);
    free(Jobs[i].sets);
    // Remove the job
    memcpy(&job,&Jobs[i],sizeof(job_t));
    nJobs--;
    memcpy(&Jobs[i],&Jobs[nJobs],sizeof(job_t));
    UnlockMutex(&Mutex);
    if( !System("rm -rf %s",job.id) ) {
      sprintf(cmd->arg,"FAILURE:rm");
      return 0;
    }
    return 1;
  }

  // Job not found
  UnlockMutex(&Mutex);
  sprintf(cmd->arg,"FAILURE:not found");
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int CmdPriority(int client, command_t *cmd)
{
  int i,priority;  char *job,*pri,*s;

  // Parse the argument string
  if( !(job=strtok_r(cmd->arg, ":",&s)) ) { sprintf(cmd->arg,"FAILURE:parse:job");      return 0; }
  if( !(pri=strtok_r(NULL,    ":",&s)) )  { sprintf(cmd->arg,"FAILURE:parse:priority"); return 0; }
  if( sscanf(pri,"%d",&priority) != 1 )   { sprintf(cmd->arg,"FAILURE:parse:priority"); return 0; }

  LockMutex(&Mutex);

  // Search to see if we have a matching job
  for(i=0; i<nJobs; i++) {
    if( !strcmp(Jobs[i].id,job) ) {
      // Found a match
      Jobs[i].priority = priority;
      UnlockMutex(&Mutex);
      return 1;
    }
  }

  // Job not found
  sprintf(cmd->arg,"FAILURE:not found");

  UnlockMutex(&Mutex);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int Pause(int client, command_t *cmd, int pause)
{
  int i;

  // Search to see if we have a matching job
  LockMutex(&Mutex);
  if( (i=FindJob(cmd->arg)) != -1 ) {
    // Found a match
    if( pause ) Jobs[i].flags |=  FLAG_PAUSED;
    else        Jobs[i].flags &= ~FLAG_PAUSED;
    UnlockMutex(&Mutex);
    return 1;
  }

  // Job not found
  UnlockMutex(&Mutex);
  sprintf(cmd->arg,"FAILURE:not found");
  return 0;
}

int CmdPause(int client, command_t *cmd)
{
  return Pause(client, cmd, 1);
}

int CmdResume(int client, command_t *cmd)
{
  return Pause(client, cmd, 0);
}

////////////////////////////////////////////////////////////////////////////////

int CmdData(int client, command_t *cmd)
{
  job_t job;
  char  buf[MAX_CMD+16]; // size: id + ".tar.gz"
  int   i;  

  // !!av:
  //       It would be nice if this would only create the tarball
  //       once if needed, and then send the existing file.

  LockMutex(&Mutex);

  // Search for a matching job
  if( (i=FindJob(cmd->arg)) != -1 ) {
    memcpy(&job,&Jobs[i],sizeof(job_t));
    UnlockMutex(&Mutex);
    // Tar the current state of the job 
    if( !System("rm -f %s.%d.tar.gz && tar -zcf %s.%d.tar.gz %s",
		job.id,client,job.id,client,job.id) ) {
      sprintf(cmd->arg,"FAILURE:tar");
      return 0;
    }
    // Tar is done; return success
    sprintf(cmd->arg,"SUCCESS");
    if( !SendCommand(client, cmd) ) { 
      sprintf(cmd->arg,"FAILURE:send ready"); 
      return 0; 
    }
    // Client ready for file
    sprintf(buf,"%s.%d.tar.gz",job.id,client);
    if( !SendFile(client, buf) ) { 
      sprintf(cmd->arg,"FAILURE:file"); 
      return 0; 
    }
    // Upload complete!
    System("rm -f %s.%d.tar.gz",job.id,client);
    return 1;
  }

  // Job not found
  sprintf(cmd->arg,"FAILURE:not found");

  UnlockMutex(&Mutex);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//
// The server maintains one thread for each connected client:
//
void* ClientThread(void *arg)
{
  command_t  cmd,ocmd;
  char      *name = ((client_t*)arg)->name;
  int        sock = ((client_t*)arg)->sock,cf,rv=0;

  // Read one command command from the client
  if( RecvCommand(sock, &cmd) ) {
    memcpy(&ocmd,&cmd,sizeof(command_t));
    cf = 0;
    
    ////////////////////////////////////////////////////////////////
    //  These commands are common to both clients and arbiters
    ////////////////////////////////////////////////////////////////
    /*
      A simple ping-like command, mostly for debugging.
    */
    if( !strcmp(ocmd.cmd,"hello") ) {
      cf = 1;
      // Just a hello; do nothing but respond success (below)
    }
    
    ////////////////////////////////////////////////////////////////
    //  These commands are sent from arbiters
    ////////////////////////////////////////////////////////////////
    /*
      Asks to be assigned a new work unit.
    */
    if( !strcmp(ocmd.cmd,"request") ) {
      cf = 1;
      rv = CmdRequest(sock, &cmd);
    }
    /*
      Tells server that there is new status imformation for a job.
    */
    if( !strcmp(ocmd.cmd,"progress") ) {
      cf = 1;
      rv = CmdProgress(sock, &cmd);
    }
    /*
      Asks for the number of nodes needed by currently known jobs.
    */
    if( !strcmp(ocmd.cmd,"queued") ) {
      cf = 1;
      rv = CmdQueued(sock, &cmd);
    }
    /*
      Provides checkpoint data for the arbiter's current job.
    */
    if( !strcmp(ocmd.cmd,"checkpoint") ) {
      cf = 1;
      rv = CmdCheckpoint(sock, &cmd);
    }
    
    ////////////////////////////////////////////////////////////////
    //  These commands are sent from clients
    ////////////////////////////////////////////////////////////////
    /*
      Asks for information about the current state of the server; the
      server's state will be checkpointed, and the checkpoint file
      will be sent to the client.
    */
    if( !strcmp(ocmd.cmd,"status") ) {
      cf = 1;
      rv = CmdStatus(sock, &cmd);
    }
    /*
      Schedules a new job on the server
    */
    if( !strcmp(ocmd.cmd,"job") ) {
      cf = 1;
      rv = CmdJob(sock, &cmd);
    }
    /*
      Tells server to set a new priority for a job.
    */
    if( !strcmp(ocmd.cmd,"priority") ) {
      cf = 1;
      rv = CmdPriority(sock, &cmd);
    }
    /*
      Asks for current results from a job.
    */
    if( !strcmp(ocmd.cmd,"data") ) {
      cf = 1;
      rv = CmdData(sock, &cmd);
    }
    /*
      Tells the server to remove a job.
    */
    if( !strcmp(ocmd.cmd,"remove") ) {
      cf = 1;
      rv = CmdRemove(sock, &cmd);
    }
    /*
      Tells the server to resume a job.
    */
    if( !strcmp(ocmd.cmd,"resume") ) {
      cf = 1;
      rv = CmdResume(sock, &cmd);
    }
    /*
      Tells the server to pause a job.
    */
    if( !strcmp(ocmd.cmd,"pause") ) {
      cf = 1;
      rv = CmdPause(sock, &cmd);
    }
    /*
      Tells the server to erase/flush all checkpoint files for a job.
    */
    if( !strcmp(ocmd.cmd,"ckflush") ) {
      cf = 1;
      rv = CmdCheckpointFlush(sock, &cmd);
    }
    /*
      Tells the server to "kill" a run.
    */
    if( !strcmp(ocmd.cmd,"terminate") ) {
      cf = 1;
      rv = CmdTerminate(sock, &cmd);
    }
    
    ////////////////////////////////////////////////////////////////
    //  Send a response to the client
    ////////////////////////////////////////////////////////////////
    if( cf ) {
      if( rv ) 
	sprintf(cmd.arg,"SUCCESS");
    } else {
      sprintf(cmd.arg,"FAILURE:unimplemented");
    }
    Log("%s: (%s|%s)\t -> (%s|%s)\n",name,ocmd.cmd,ocmd.arg,cmd.cmd,cmd.arg);
    SendCommand(sock, &cmd);
    close(sock);
    return NULL;
  }

  // Initial RecvCommand() failed
  Log("%s: bad protocol\n",name);
  close(sock);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//
// The server will wait for new connections indefinately
//
void AcceptConnections(char *name, int port)
{
  struct sockaddr_in  server, client;
  pthread_attr_t      a;
  pthread_t           t;  
  sigset_t            s;
  int                 serversock, clientsock,accept_retry=0;
  unsigned int        clientlen = sizeof(client);
  client_t           *cli;

  // It is important for write() to fail when writing data to a terminated
  // client; block sigpipe.
  if( sigemptyset(&s) )                  Die("StartThreads(): sigemptyset()");
  if( sigaddset(&s, SIGPIPE) )           Die("StartThreads(): sigaddset()");
  if( sigprocmask(SIG_BLOCK, &s, NULL) ) Die("StartThreads(): sigprocmask()");

  // Setup a serving socket
  if ( (serversock=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0 )
    Die("governor: socket()");
  memset(&server, 0, sizeof(server));
  server.sin_family      = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port        = htons(port);
  if ( bind(serversock,(struct sockaddr*)&server,sizeof(server)) < 0 )
    Die("governor: bind()");
  if ( listen(serversock,MAX_LISTEN) < 0 )
    Die("governor: listen()");
  
  // Start accepting connections
  if( pthread_attr_init(&a) )                                   Die("AcceptConnections(): pthread_attr_init()");
  if( pthread_attr_setscope(&a,PTHREAD_SCOPE_SYSTEM) )          Die("AcceptConnections(): pthread_attr_setscope()");
  if( pthread_attr_setdetachstate(&a,PTHREAD_CREATE_DETACHED) ) Die("AcceptConnections(): pthread_attr_setdetachstate()");
  while (1) {
    clientlen = sizeof(client);
    if ( (clientsock=accept(serversock,(struct sockaddr*)&client,&clientlen)) < 0 ) {
      if( (errno == ECONNABORTED) || (errno == EINTR) || (errno == ENFILE) ) {
	Yield(0,1);
	continue;
      }
      if( accept_retry++ > 1024 ) {
	Die("AcceptConnections(): accept()");
      }
    }
    if( !(cli=malloc(sizeof(client_t))) )
      Die("AcceptConnections(): malloc()");
    cli->sock = clientsock;
    strncpy(cli->name,inet_ntoa(client.sin_addr),sizeof(cli->name));
    cli->name[sizeof(cli->name)-1] = '\0';
    if( pthread_create(&t, &a, ClientThread, (void*)(cli)) )
      Die("AcceptConnections(): pthread_create(ClientThread)");
  }

}

void* MaitenanceThread(void *arg)
{
  int j,s,i;  job_t *job;

  while(1) {
    LockMutex(&Mutex);
    //
    // Examine job queue
    //
    for(j=0; j<nJobs; j++) {
      job = &Jobs[j];
      // Sync job flags
      SyncJob(job);
      for(s=0; s<job->nsets; s++)
	for(i=0; i<job->sets[s].nisets; i++) {
	  // Not interested in unscheduled jobs or completed jobs
	  if( !job->sets[s].isets[i].stime || job->sets[s].isets[i].etime )
	    continue;
	  // Check TTL, only if there is not an ongoing file transfer
	  if( !job->sets[s].isets[i].ftflags && ((time(NULL)-job->sets[s].isets[i].ptime) > ISET_TTL) ) {
	    // This iset hasn't been updated by an arbiter
	    // for a period of time longer than the time-to-live.
	    // Mark it for reschedule.
	    Log("MT: iset %s has been lost; marking for reschedule.\n",job->sets[s].isets[i].id);
	    job->sets[s].isets[i].ptime = 0;
	    job->sets[s].isets[i].stime = 0;
	    job->sets[s].isets[i].arbiter[0] = '\0';
	  }
	}
    }

    // Save the job queue and arbiter list to disk
    SaveState();
    UnlockMutex(&Mutex);

    Yield(MAINTENANCE_RATE,0);
  }

  return NULL;
}

void StartMaitenanceThread()
{
  pthread_attr_t      a;
  pthread_t           t;  

  // Start accepting connections
  if( pthread_mutex_init(&Mutex,NULL) )                         Die("pthread_mutex_init()");
  if( pthread_attr_init(&a) )                                   Die("pthread_attr_init()");
  if( pthread_attr_setscope(&a,PTHREAD_SCOPE_SYSTEM) )          Die("pthread_attr_setscope()");
  if( pthread_attr_setdetachstate(&a,PTHREAD_CREATE_DETACHED) ) Die("pthread_attr_setdetachstate()");
  if( pthread_create(&t, &a, MaitenanceThread, NULL) )          Die("pthread_create(MaitenanceThread)");
}

void UsageError()
{
  fprintf(stderr,"Usage:\n\tgovernor <port> <datadir>\n");
  exit(1);
}

void ParseCommandLine(int argc, char **argv, char *path, int *port)
{
  int p;

  // Check count
  if( argc != 3 )
    UsageError();
  
  // Copy path and check path length
  strncpy(path,argv[2],512);
  for(p=0; (p<512) && path[p]; p++);
  if( p >= 512 ) {
    fprintf(stderr,"Maximum path length: %d\n",512);
    exit(1);
  }

  // Convert and check port
  if( sscanf(argv[1],"%d",port) != 1 ) 
    UsageError();
}

void sig_hndlr(int sig)
{
  // !!av:
  //        Assume this is the only thread of execution for this process,
  //        and therefore, safe to state save?
  SaveState();
  exit(9);
}

int main(int argc, char **argv)
{
  int port;  char name[128],path[512];

  // !!av: 
  //       Make this a commandline option.
  //
  // Drop root
  setuid(3337);

  // Setup signal handler
  signal(SIGTERM, sig_hndlr);
   
  // Check command line args
  ParseCommandLine(argc,argv,path,&port);
  if( chdir(path) )
    Error("Cannot chdir to \"%s\".\n",path);
 
  // Get the machnine name and print out a starting message
  memset(name,0,sizeof(name));
  if( gethostname(name, sizeof(name)) )
    Die("main(): gethostname()");
  Log("Starting governor on \"%s\" with port %d.\n", name, port);

  // This doesn't free anything and thus, is only suited for
  // an initialization role.
  RestoreState();

  // Start the queue maintenance thread
  StartMaitenanceThread();

  // Start accepting connections
  AcceptConnections(name, port);
  
  return 0;
}
