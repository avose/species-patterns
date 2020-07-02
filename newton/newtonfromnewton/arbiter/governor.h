#ifndef GOVERNOR_H
#define GOVERNOR_H

// These are used for statically declared arrays
#define MAX_JOBS       128
#define MAX_LISTEN     32
#define MAX_ARBITERS   1024

// Time in seconds between maintenance thread checks and iset TTL
#define MAINTENANCE_RATE 10
#define ISET_TTL         300

// These flags are used to tag jobs/isets with extra information.
#define FLAG_SCHEDULED 1
#define FLAG_PAUSED    2
#define FLAG_B0RKED    4
#define FLAG_ZOMBIE    8
#define FLAG_COMPLETE  16

// These flags aid in scheduling and maintenance; they flag isets 
// with in progress file transfers.
#define FFLAG_CHECKPOINT 1
#define FFLAG_SCHEDULE   2

typedef struct {
  char  name[MAX_CMD];
  short sock;
} client_t;

typedef struct {
  char   name[MAX_CMD];       // IP/FQ-Hostname for the arbiter
  char   cpu[MAX_CMD];        // CPU vendor (GenuineIntel,AuthenticAMD,etc)
  time_t speed;               // Average iset processing time
  time_t time;                // Last time this arbiter was heard from
  int    isets;               // Number of processed isets
} arbiter_t;

typedef struct {
  char      id[MAX_CMD];      // ID in the form: "job_id:set#:run#"
  time_t    stime;            // Start time
  time_t    etime;            // End time; 0 == not ended yet
  time_t    ptime;            // Last time the state was checked
  int       progress;         // (optional) Progress as a percent
  char      arbiter[MAX_CMD]; // IP/FQ-Hostname for the arbiter
  char      custom[MAX_CMD];  // (optional) Custom iset status
  char      chkpnt[MAX_CMD];  // (optional) name of last checkpoint file
  int       ftflags;          // File transfer flags.
} iset_t;

typedef struct {
  char    file[MAX_CMD];
  iset_t *isets;
  int     nisets;
} set_t;

typedef struct {
  char    id[MAX_CMD];
  int     priority;
  int     runs;
  set_t  *sets;
  int     nsets;
  time_t  stime;
  time_t  etime;
  int     flags;
} job_t;

#endif
