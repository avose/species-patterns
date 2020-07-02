#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "parameters.h"
#include "random.h"
#include "niche.h"
#if _ARBITER
#include "arbiter.h"
#endif

#if _HABITAT
extern u64b     Habitat_logs;
extern FILE    *Habitatf;
#endif

#if _DISDATA
extern u64b     Disdata_logs;
extern FILE    *Disdataf;
int             dispersedata[12];
int             dispsqr[9];
int             matesqr[9];
#endif

/*
int             lessx  = 0;
int             morex  = 0;
int             samex  = 0;
int             lessy  = 0;
int             morey  = 0;
int             samey  = 0;
int             lessdx = 0;
int             moredx = 0;
int             samedx = 0;
int             lessdy = 0;
int             moredy = 0;
int             samedy = 0;
//int             newpopsize[8][8];
*/
RndState        R;

// Mutex and condition variables for thread syncronization
extern volatile pthread_cond_t  Master,Slaves;
extern volatile pthread_mutex_t Mutex;
extern volatile int             Go[_CPUS],Done[_CPUS];
extern volatile pThreadData     Threads[_CPUS];

// Random Seed, and starting space state.  Copied by the threads
extern          u64b            Seed;
extern          Space           Start;

// Array based parameters (to be coppied to threads) 
// Could these arrays be turned into constants?
//extern          double          Sigma_s[];
//extern          double          Sigma_c[];
//extern          double          a_i[];
extern          double          Xi[];

// The current and total generation numbers
extern          u64b            Gen;
extern          u64b            NGen;


// Code shared with niche.c
#include "shared.h"

/***************************************************************************************
 * Usefull "utility" functions
 ***************************************************************************************/

static inline void ArbiterProgress()
{
#if _ARBITER
  arbiter_progress((((double)Gen)/NGen)*100.0);
#endif
}

// Sets m to be the max value in the size l array d, and sets w to be the index of m in d
static void Maxint(int *d, u64b l, int *m, u64b *w)
{

  int i,c,t[1024];

  // Find the max and indicies
  for(i=c=0; i<l; i++) {
    if(d[i] > *m) {
      // New max; store it, and reset t and c
      *m = d[i];
      t[0] = i;
      c=1;
    } else if (d[i] == *m) {
      // Tie; record the tie
      t[c++] = i;
    }
  }

  // Break any ties with a random choice
  *w = t[rnd(&R, c)];

}


// Sets m to be the max value in the size l array d, and sets w to be the index of m in d
static void Max(double *d, u64b l, double *m, u64b *w)
{

  int i,c,t[1024];

  // Find the max and indicies
  for(i=c=0; i<l; i++) {
    if(d[i] > *m) {
      // New max; store it, and reset t and c
      *m = d[i];
      t[0] = i;
      c=1;
    } else if (d[i] == *m) {
      // Tie; record the tie
      t[c++] = i;
    }
  }

  // Break any ties with a random choice
  *w = t[rnd(&R, c)];

}

/*
// in random.c; Returns choice from poisson distribution with mean mu
static int Poisson(double mu)
{

  double p = exp( -mu ), q = 1;
  int    n=-1;

  do q *= U01(&R), n++;
  while( (q > p) || (q == p) );

  return n;

}
*/

/***************************************************************************************
 * Functions dealing with the creation and syncronization of threads
 ***************************************************************************************/

// Signals that the thread td has completed and then waits for a go (or exit) signal
static int SignalDone(pThreadData td) 
{
  // Signal done and wait for Go (or exit) singal
  pthread_mutex_lock((pthread_mutex_t*)&Mutex);
  Done[td->id] = 1;
  pthread_cond_signal((pthread_cond_t*)&Master);
  while( !Go[td->id] )
    pthread_cond_wait((pthread_cond_t*)&Slaves, (pthread_mutex_t*)&Mutex);
  Go[td->id] = 0;
  pthread_mutex_unlock((pthread_mutex_t*)&Mutex);

  // For now there is no exit condition
  return 1;
}


/***************************************************************************************
 * Functions dealing directly with maintenance of Individuals
 ***************************************************************************************/

// I'm commenting out these functions as they are not used:
// this will keep the compiler warnings to a minimun.
#if 0

// Returns the number of individuals used in the "next generation"
// Note: This count _will_ include wanderers if there are any.
static u64b nNextIndividuals()
{
  u64b i,m;

  for(m=i=0; i<_CPUS; i++)
    m += Threads[i]->nINext;
  return m;
}

// Returns the number of individuals used in the entire simulation.
// Note: This count _will_ include wanderers if there are any.
static u64b nIndividuals()
{
  u64b i,m;

  for(m=i=0; i<_CPUS; i++)
    m += Threads[i]->nICurrent + Threads[i]->nINext;
  return m;
}
#endif


// Fills in traits from characteristic array
static void SyncIndividual(pThreadData td, pIndividual i)
{

  u64b j,k;

  // Sync patch related traits from characteristic array
  for(j=0;j<_k;j++) {
    for(i->x[j]=0.,k=_Le*j; k<_Le*(j+1); k++)
      i->x[j] += ((i->x0[k/LPW_E]>>(_Ae*(k%LPW_E)))&MASK_E) + ((i->x1[k/LPW_E]>>(_Ae*(k%LPW_E)))&MASK_E);
    for(i->y[j]=0.,k=_Lp*j; k<_Lp*(j+1); k++)
      i->y[j] += ((i->y0[k/LPW_P]>>(_Ap*(k%LPW_P)))&MASK_P) + ((i->y1[k/LPW_P]>>(_Ap*(k%LPW_P)))&MASK_P);
    // Scale traits
    i->x[j] /= MASK_E*(_Le<<1);
    i->y[j] /= MASK_P*(_Lp<<1);
  }

#if _DISTINCT_SEXES
  // Sync and scale maker
  for(i->k=0.,k=0; k<_Lk; k++)
    i->k += ((i->k0[k/LPW_K]>>(_Ak*(k%LPW_K)))&MASK_K) + ((i->k1[k/LPW_K]>>(_Ak*(k%LPW_K)))&MASK_K);
  i->k /= MASK_K*(_Lk<<1);
  // Sync and scale mating (in interval [-1,1])
  for(i->m=0.,k=0; k<_Lm; k++)
    i->m += ((i->m0[k/LPW_M]>>(_Am*(k%LPW_M)))&MASK_M) + ((i->m1[k/LPW_M]>>(_Am*(k%LPW_M)))&MASK_M);
  i->m = (i->m/(MASK_M*_Lm)) - 1.0;
#if _SEXUAL_SELECTION
  // Sync and scale mating preference
  for(i->f=0.,k=0; k<_Lf; k++)
    i->f += ((i->f0[k/LPW_F]>>(_Af*(k%LPW_F)))&MASK_F) + ((i->f1[k/LPW_F]>>(_Af*(k%LPW_F)))&MASK_F);
  i->f /= MASK_F*(_Lf<<1);
#endif
#endif
}

// Installs individual i into patch p in a safe manner
static int InstallIndividual(pPatch p, pIndividual i)
{
  if(p->ni == _MAX_INDIVIDUALS) return 0;
  p->i[p->ni++] = i;            return 1;
}

// Returns a pointer to an available individual structure.  NULL for none.
static pIndividual GetNextIndividual(pThreadData td)
{
  // Record where this individual was allocated from
  td->INext[td->nINext]->id = td->nINext;
  return td->INext[td->nINext++];
}

// Releases an Individual back to the its individual array
static void ReleaseNextIndividual(pThreadData td, pIndividual i)
{
  // Swap last in use with i and decriment
  td->INext[i->id]      = td->INext[--td->nINext];
  td->INext[td->nINext] = i;

  // Update allocated from (id) field
  td->INext[i->id]->id   = i->id;
}

// Clears all individuals by restoring the initial sate of individual allocation
static void ReleaseAllCurrentIndividuals(pThreadData td)
{
  td->nICurrent = 0;
  memcpy(td->ICurrent, td->ICurrentCleared, MAX_THREAD_POP*sizeof(pIndividual));

  // this is to shuffle the ind in the memory (to prevent potential first-come-first-serve bias)
  {
    int         i;
    pIndividual ind;

    for(i=0; i<MAX_THREAD_POP; i++) {
      ind = td->ICurrent[(MAX_THREAD_POP-1)-i];
      td->ICurrent[(MAX_THREAD_POP-1)-i] = td->ICurrent[i];
      td->ICurrent[i] = ind;
    }
  }

}

// I'm commenting out these functions as they are not used:
// this will keep the compiler warnings to a minimun.
#if 0
// Clears all individuals by restoring the initial sate of individual allocation
static void ReleaseAllNextIndividuals(pThreadData td)
{
  td->nINext = 0;
  memcpy(td->INext, td->INextCleared, MAX_THREAD_POP*sizeof(pIndividual));
}
#endif

/***************************************************************************************
 * Functions which compute probabilities, properties, or preferences of individuals
 ***************************************************************************************/


static double Ne(pThreadData td, pIndividual i, pPatch p)
{
  int j,h;  double t,a=0.,c=0.;

  // Transform p (which is a pointer into next) into "current" version of p
  p = &(td->Current[p->lat][p->lon]);

  // printf("\npatch (%d, %d), N: %d with %d roaming neighbors", p->lat, p->lon, p->ni, p->rrne);

  // Go through the range of the individual
  for(j=0; j<(p->rrne); j++) {
     // printf("-roaming patch (%d) at (%d, %d), N: %d, others: %d\n", j, p->rne[j]->lat, p->rne[j]->lon, p->rne[j]->ni, p->rne[j]->nothers);

    /*
    // sanity print about "current" and "next"
    {
      int x,y,f;

      for(f=x=0; x<_WIDTH; x++) {
        for(y=0; y<_HEIGHT; y++) { 
          if( &(td->Current[x][y]) == p->rne[j] ) {
            printf("Part of current\n");
            f = 1;
          }
        }
      }
      if( !f ) {
        for(f=x=0; x<_WIDTH; x++) {
          for(y=0; y<_HEIGHT; y++) { 
            if( &(td->Next[x][y]) == p->rne[j] ) {
              printf("Part of next\n");
              f = 1;
            }
          }
        }
        if( !f ) {
          printf("Part of nothing\n");
        }
      }
    }
    */

    // printf("\nN is %d, at roaming patches: %d/%d", p->rne[j]->nothers, j, p->rrne);
    // go through all the individuals in that patch
    for(t=0.,h=0; h<p->rne[j]->nothers; h++){
      // printf("-ind is: %d, t is: %f\n", h, p->rne[j]->Other[h].timespent);
      t += p->rne[j]->Other[h].timespent;
    }
    // printf("time spent by focal individual at the patch: %f\ntotal time spent by others: %f\n", i->TRoam[j], t);
    a = (i->TRoam[j] * t);
    c += a;
    // printf("\nNe weighted by the time spent per patch: %f", a);
  }

   // printf("Ne weighted by the time spent: %f\n", c);
   // printf("patch (%d, %d), roaming neighbors: %d, N: %d, average Ne in the patch %f (ID: %d)\n",p->lat, p->lon, p->rrne, p->ni, c, p->SpeciesID);
   // printf("patch (%d, %d), average Ne in the patch %f (ID: %d)\n",p->lat, p->lon, c, p->SpeciesID);
  return c;
}


// The number of offspring to be produced, this is the seed for Poisson distribution
static double w(pThreadData td, pIndividual i, pPatch p)
{
  double r;

  r = Ne(td,i,p)/(((double)(_K0)) * K(i,p));

  // printf("\npart of BH W(%lf, %lf, %lf, %lf) is: %f (at %d, %d; Ne is: %f; actual W is: %f))", i->x[0], i->x[1], i->x[2], i->x[3], r, p->lat, p->lon, Ne(td,i,p), K(i,p));

#if _DISTINCT_SEXES
  /*
  // print for a sanity check
  double f;
  f = (1.0/(1.0+(((double)(_b))/2.0-1.0)*r));
  printf("\n'v' in beverton-holt is: %f\n",f);
  */
  return (1.0/(1.0+(((double)(_b))/2.0-1.0)*r));
#else
  return (1.0/(1.0+(((double)(_b))-1.0)*r));
#endif
}


#if _DISTINCT_SEXES
// The probability that female f and male m will mate based on preference
static double Pm(pIndividual f,  pIndividual m)
{
  double d;

  // Find the distance of their marker traits
#if _SEXUAL_SELECTION
#if _MAGIC_TRAIT                                /* mating with regard to 1st ecological trait */
  if( f->m > 0.0 ) d = f->f - m->x[0];          // Prefer similar mates
  else             d = f->f + m->x[0] - 1.0;    // Prefer different mates
#else                                           /* mating with regard to male trait */
  if( f->m > 0.0 ) d = f->f - m->k;             // Prefer similar mates (sex specific traits)
  else             d = f->f + m->k - 1.0;       // Prefer different mates (sex specific traits)
#endif
#else
#if _MAGIC_TRAIT                                /* mating with regard to 1st ecological trait */
  if( f->m > 0.0 ) d = f->x[0] - m->x[0];       // Prefer similar mates
  else             d = f->x[0] + m->x[0] - 1.0; // Prefer different mates  
#else                                           /* mating with regard to male trait */
  if( f->m > 0.0 ) d = f->k - m->k;             // Prefer similar mates (both based on same mathing trait)
  else             d = f->k + m->k - 1.0;       // Prefer different mates (both based on same mathing trait)
#endif
#endif

//   // Turn d into either d*d or (1-d)*(1-d) based on sign of f->m
//   if( f->m > 0 ) d = d*d;         // Prefer similar mates
//   else           d = (1-d)*(1-d); // Prefer different mates

  // Compute the probability of f choosing mate m
  return exp( -0.5 * (f->m*f->m) * d*d/(_SIGMA_A*_SIGMA_A) );
}
#endif

/***************************************************************************************
 * Functions which represent the conceptual steps required for the simulation (+ helpers)
 ***************************************************************************************/

#if SKIP_MUTATION
// Fills in mutation distributions (->MutateDist and ->MutateSkip)
static void MutateProb(pThreadData td, int w, int plength, int fields, double mu, int abits, u64b mask)
{
  int i,j,x,e,d;  double s,sk;

  // If no mutation, just skip all this mess
  if(!mu) return;

  // Setup ->MutateDist
  // Go through all possible 8-bit source patterns
  for(x=0; x<256; x++) {
    // Allocate space for 256 destinations and initialize their probabilities to 0
    td->MutateDist[w][x] = allocdist(256);
    memset(td->MutateDist[w][x]->p, 0, 256*sizeof(double));
    // Traverse all potential mutation results
    for(i=0,s=0.; i<256; i++) {
      // Consider all loci within this byte
      for (j=e=0; j < fields; j++){
	// If this destination is a non-neighbor, it is not a valid mutation destination.
	// Continue to next destination byte.
	if ((d = abs(((i>>(abits*j))&mask)-((x>>(abits*j))&mask))) > 1)
	  goto next_byte;
	// Valid destination, so increment the mutation event counter if d is 1
	e += d;
      }
      if (e) s += (td->MutateDist[w][x]->p[i] = pow(mu,e)*pow(1.-mu,fields-e));
    next_byte:
      continue;
    }
    // Call initdist on this distribution
    initdist(td->MutateDist[w][x],s);
  }

  // Setup ->MutateSkip
  // Allocate space for skip distribution: 
  // skip value of {0...,PLENGTH} bytes, where PLENGTH == no mutation
  td->MutateSkip[w] = allocdist(plength+1);
  // Initialize sk to the probability that a byte will be skipped
  sk = pow(1.-mu, (double)fields);
  // Consider all skip possibilities which will result in mutation and fill in the appropriate probability
  for(i=0,s=0.; i<plength; i++)
    s += td->MutateSkip[w]->p[i] = pow(sk,(double)i) * (1.-sk);
  td->MutateSkip[w]->p[plength] = 1.-s;
  // Call initdist on this distribution
  initdist(td->MutateSkip[w],1.);
}
#else
// Fills in mutation distribution ->MutateDist
static void MutateProb(pThreadData td, int w, int plength, int fields, double mu, int abits, u64b mask)
{
  int i,j,x,d;  double s,f,h,nf,nh;

  /* If no mutation, just skip all this mess */
  if(!mu) return;

  /* Setup ->MutateDist */
  /* Go through all possible 8-bit source patterns */
  for(x=0; x<256; x++) {
    /* Allocate space for 256 destinations and initialize their probabilities to 0 */
    td->MutateDist[w][x] = allocdist(256);
    memset(td->MutateDist[w][x]->p, 0, 256*sizeof(double));
    for(s=0.,j=0; j<256; j++) {                                                // traverse potential mutation result
      for (i = f = h = nf = nh = 0.; i < fields; i++){                         // consider the multi-bit loci wihtin a byte
	if ((d = abs(((j>>(abits*i))&mask)-((x>>(abits*i))&mask))) > 1)        // if mutate to non-neighbor...
	  goto e;                                                              // invalid mutation result
	if (abits > 1){                                                        // some alleles can mutate both directions
	  if ( (!((x>>(abits*i))&mask)) || (((x>>(abits*i))&mask)==mask) ) {   // if at boundary...
	    if (d) h++;                                                        // count mutation event
	    else   nh++;                                                       // count non-mutation event
	  } else {                                                             // not at boundary...
	    if (d) f++;                                                        // count mutation event
	    else   nf++;                                                       // count non-mutation event
	  }
	} else {                                                               // can only mutate one direction
	  f += d;                                                              // count mutation events
	}
      }
      if (abits> 1) s += (td->MutateDist[w][x]->p[j] = pow(mu,f)*pow(1.-mu,nf)*pow(mu/2.,h)*pow(1.-mu/2.,nh));
      else          s += (td->MutateDist[w][x]->p[j] = pow(mu,f)*pow(1.-mu,fields-f));
    e:                                                                         // consider next byte value
      continue;
    }
    initdist(td->MutateDist[w][x],s);
  }
}
#endif

/*
  Initializes neighbor mutation
*/
static void MutateInit(pThreadData td)
{
  // Setup skip and mutation distributions for ->e and ->p
  MutateProb(td, 0, PLENGTH_E, FIELDS_E, _mue, _Ae, MASK_E);
  MutateProb(td, 1, PLENGTH_P, FIELDS_P, _mup, _Ap, MASK_P);

#if _DISTINCT_SEXES
  // Setup skip and mutation distributions for ->m and ->k
  MutateProb(td, 2, PLENGTH_M, FIELDS_M, _mum, _Am, MASK_M);
  MutateProb(td, 3, PLENGTH_K, FIELDS_K, _muk, _Ak, MASK_K);
#if _SEXUAL_SELECTION
  // Setup skip and mutation distributions for ->f
  MutateProb(td, 4, PLENGTH_F, FIELDS_F, _muf, _Af, MASK_F);
#endif
#endif

#if _Ln
  // Setup skip and mutation distributions for ->z
  MutateProb(td, 5, PLENGTH_N, FIELDS_N, _mun, _An, MASK_N);
#endif
}

/*
  Returns the number of flips (0 <--> 1) in crossover mask x; clone event <--> x = 255
*/
static double CrossFlips(byte x) 
{
  /* initial 0 <--> previous cross point */
  int a = 1-(x&1), i = 7; 

  for ( ; i--; x >>= 1) a += (x&1)^((x>>1)&1);
  return (double) a;
}

/*
  Returns 1 if crossover mask x is valid (does not split a multi-bit loci), 0 on invalid
*/
static int CrossValid(byte x, int abits)
{
  int a,i;

  /* Move through all bits */
  for (i=a=0; x; x>>=1){
    /* Sum the number of ones */
    a += x&1;
    /* Check to see if we are done with a locus here */
    if (!((++i)%abits)){
      /* If this locus is split, return 0 */
      if (a%abits) 
	return 0;
      /* Reset a and keep searching */
      a = 0;
    }
  }

  /* Test last locus and return valid/not valid */
  return ((a%abits)? 0: 1);
}

// Fills in crosover distribution
static void CrossProb(pThreadData td, int w, int fields, int abits)
{
  int i;  double s;

  /* Allocate space for all possible crossover masks */
  td->CrossDist[w] = allocdist(256);
  memset(td->CrossDist[w]->p, 0, 256*sizeof(double));

  /* Consider all crossover masks */
  for (s=0.,i=0; i<256; i++)
    /* Make sure mask is valid and fill in it's prob */
    if (CrossValid(i,abits))
      s += td->CrossDist[w]->p[i] = pow(Xi[w],CrossFlips(i)) * pow(1.-Xi[w],fields-CrossFlips(i));

  /* Call initdist to initialize this distribution */
  initdist(td->CrossDist[w], s);
}

// Initialize crossover
static void CrossInit(pThreadData td)
{
  // Initialize distributions ->e and ->p
  CrossProb(td, 0, FIELDS_E, _Ae);
  CrossProb(td, 1, FIELDS_P, _Ap);

#if _DISTINCT_SEXES
  // Initialize distributions ->m and ->k
  CrossProb(td, 2, FIELDS_M, _Am);
  CrossProb(td, 3, FIELDS_K, _Ak);
#if _SEXUAL_SELECTION
  // Initialize distributions ->f
  CrossProb(td, 4, FIELDS_F, _Af);
#endif
#endif

#if _Ln
  // Initialize distributions ->z
  CrossProb(td, 5, FIELDS_N, _An);
#endif
}

// Returns a pointer to the patch ox,oy spaces from x,y, or NULL if out of bounds
// This is not a generic function. It is mostly specific to NeighboringPatches()
static pPatch ValidPatch(pThreadData td, u64b x, u64b y, int ox, int oy)
{
  // Width bounds (Only bound first left and last right)
  if( (x == 0)     && (ox < 0) && (!td->id) )               return NULL;
  if( (x == WIDTH-1) && (ox > 0) && (td->id == (_CPUS-1)) ) return NULL;
  // Height bounds
  if( (y == 0)        && (oy < 0) ) return NULL;
  if( (y == HEIGHT-1) && (oy > 0) ) return NULL;

  return &td->Next[x+ox][y+oy];
}

// Fills in the neighboring patches field of td->Next[x][y].
// This is not a generic function.  It is mostly specific to InitThread()
static void NeighboringPatches(pThreadData td, u64b x, u64b y)
{
  int i,j;

  for(td->Next[x][y].nne=0,i=-1; i<2; i++)
    for(j=-1; j<2; j++) {
      if( (!i) && (!j) ) continue;
      if( (td->Next[x][y].ne[td->Next[x][y].nne] = ValidPatch(td, x,y,i,j)) ) td->Next[x][y].nne++;
    }
}

// Returns a pointer to the patch ox,oy spaces from x,y, or NULL if out of bounds
// This is not a generic function. It is mostly specific to NeighboringDispersePatches()
static pPatch ValidDispersePatch(pThreadData td, u64b x, u64b y, int ox, int oy)
{

  // Width bounds (Only bound first left and last right)
  if( (x == 0)        && (ox < 0) )     return NULL;
  if( (x == WIDTH-1)  && (ox > 0) )     return NULL;
  if( ((x + ox) < 0) )                  return NULL;
  if( ((x + ox) > (WIDTH-1)) )          return NULL;

  // Height bounds
  if( (y == 0)        && (oy < 0) )     return NULL;
  if( (y == HEIGHT-1) && (oy > 0) )     return NULL;
  if( ((y + oy) < 0) )                  return NULL;
  if( ((y + oy) > (HEIGHT - 1)) )       return NULL;

  return &td->Next[x+ox][y+oy];
}

// Fills in the neighboring dispersal patches field of td->Next[x][y].
static void NeighboringDispersePatches(pThreadData td, u64b x, u64b y)
{
  int i,j; //,dist=0;

  for(td->Next[x][y].ddne=0,i=-((_DRANGE - 1.) / 2.); i<((_DRANGE - 1.) / 2.)+1.; i++){
    for(j=-((_DRANGE - 1.) / 2.); j<((_DRANGE - 1.) / 2.)+1.; j++) {

      //printf("\ni %d, j %d", abs(i), abs(j));
      if( abs(i) > abs(j) ){
        //dist = abs(i);
      } else {
        //dist = abs(j);
      }
      //printf("----dist= %d", dist);
      if( (td->Next[x][y].dne[td->Next[x][y].ddne] = ValidDispersePatch(td,x,y,i,j)) ){
        //td->Next[x][y].disdist[td->Next[x][y].ddne] = dist;
        td->Next[x][y].ddne++;
      }
    }
  }
  //printf("\nno of patches to disperse to: %d", td->Next[x][y].ddne);
}

// Returns a pointer to the patch ox,oy spaces from x,y, or NULL if out of bounds
// This is not a generic function. It is mostly specific to NeighboringRoamPatches()
static pPatch ValidRoamPatch(pThreadData td, u64b x, u64b y, int ox, int oy)
{
  // Width bounds
  if( (x == 0) && (ox < 0) )          return NULL;
  if( (x == WIDTH-1) && (ox > 0) )    return NULL;
  if( ((x + ox) < 0) )                return NULL;
  if( ((x + ox) > WIDTH-1) )          return NULL;
  // Height bounds
  if( (y == 0)        && (oy < 0) )   return NULL;
  if( (y == HEIGHT-1) && (oy > 0) )   return NULL;
  if( ((y + oy) < 0) )                return NULL;
  if( ((y + oy) > (HEIGHT - 1)))      return NULL;

  return &td->Next[x+ox][y+oy];
}

// Fills in the neighboring patches field of td->Next[x][y].
// This is not a generic function. It is mostly specific to InitThread()
static void NeighboringRoamPatches(pThreadData td, u64b x, u64b y)
{

  int i,j;

  for(td->Next[x][y].rrne=0,i=-((_RRANGE - 1.) / 2.); i<((_RRANGE - 1.) / 2.)+1.; i++)
    for(j=-((_RRANGE - 1.) / 2.); j<((_RRANGE - 1.) / 2.)+1.; j++) {
      if( (td->Next[x][y].rne[td->Next[x][y].rrne] = ValidRoamPatch(td,x,y,i,j)) ) td->Next[x][y].rrne++;
    }
}

int RecursiveSearch(int niche, Patch *current, int *count, int clusterno)
{
  int i;

  // Check to see if the currenty patch is one we care about
  if( niche != current->n ) return *count;
  if( current->included   ) return *count;

  // Add this patch to the current cluster
  current->included = 1;
  //current->cluster  = clusterno;
  (*count)++;

  // Recurse to neighboring patches
  for(i=0; i < current->nne; i++) {
    RecursiveSearch(niche,current->ne[i],count,clusterno);
  }

  // Return
  return *count;
}


#if _HABITAT
// Dumps habitat data to a ".habitat" file
void LogHabitat(int clusterno, int *noofpatches, int *patchtypeofc, double fragment)
{

  int           i,m,p,r,t,maxhabitat,clusterbytype[clusterno],aaa,totalp[clusterno]; //, latx, lony;
  u64b          w;
  aaa = 0;
  /*
  for( i=0; i<clusterno; i++) { 
    printf("\n%d", noofpatches[i]);
    printf("--%d", patchtypeofc[i]);
  }
  */
  memset(clusterbytype,0,sizeof(clusterbytype));

#if _HABITAT
  if( !(Gen%_HABITAT) )
    fprintf(Habitatf, "gen: ");
#endif
  fprintf(Habitatf, "%llu ",  Gen);

  int check = 0;
  for(i=0;i<clusterno; i++){
    totalp[i] = noofpatches[i];
    check += noofpatches[i];
  }

  int mm = 0;
  int maxp;
  u64b ww;
  Maxint(totalp, clusterno, &mm, &ww);
  maxp = ((int)mm);
  int totalhist[maxp+1];

  // building the histogram
  memset(totalhist,0,sizeof(totalhist));
  for(i=0; i<clusterno;i++) {
    totalhist[totalp[i]]++;
  }

  double tmeanh, tvarh, tvar, tsdh;
  int tsumh;
  int tcount=0;
  tsumh = 0;
  tmeanh = 0.;
  tvarh = 0.;
  tsdh = 0.;
  for( i=0; i<(maxp+1); i++ ){
      // printf("\n***habitathist is %d ", habitathist[i]);
    if( totalhist[i] ){
      tcount += totalhist[i];
      // printf("********habitathist is %d ", habitathist[i]);
      tsumh += i*totalhist[i];
    }
  }
  // printf("\n sum is %d\n", sumh);
  tmeanh = ((double)(tsumh)) / tcount;
  int l,n=0;
  for( i=0; i<(maxp+1); i++ ){
    if( totalhist[i] ){
      n = totalhist[i];
      for(l=0; l<n; l++){
        // printf("\n i is %d, n is %d; mean is %f, diff is %f\n", i,n,meanh,fabs((((double)(i))) - meanh));
        tvarh += fabs((((double)(i))) - tmeanh) * fabs((((double)(i))) - tmeanh);
      }
    }
  }
  // printf("\n%f, count is %d, and variance is %f", varh, count, var=varh/((double)(count)));
  tvar = tvarh/((double)(tcount));
  tsdh = sqrt(tvar);

  int s = 0;
  double relative[maxp+1];
  for( i=0; i<(maxp+1); i++ ){
    if( totalhist[i] == 0. ) continue;
    relative[s] = (((double)(totalhist[i])) / ((double)(tcount)));
    s++;
  }
  // int k;
  // for( k=0;k<maxp;k++ ) printf("\nrelative count is %f\n", relative[k]);

  double tsimpson; 
  double tshannon;
  tsimpson=0.,    // (1/sigma(p^2))*(1/S)
  tshannon=0.;    // (-sigma(pln(p))/ln(S))

  for( i=0; i<s; i++ ){
    tsimpson += (relative[i] * relative[i]);
    tshannon += (relative[i] * log(relative[i]));
  }
  // printf("\n\n si %lf, sha %lf\n", tsimpson, tshannon);

  tsimpson = ((1/tsimpson) * (1/((double)(s))));
  tshannon = (-tshannon/log((double)(s)));

  for( p=0; p<_k; p++ ){
    for ( t=r=0; r<clusterno; r++ ){
      if( patchtypeofc[r] == p ){
        clusterbytype[t] = noofpatches[r];
        t++;
      } else {
        continue;
      }
    }
    int a,aa=0;
    // printf("\n\nhabitat type is %d", p);
    for( a=0; a<t; a++ ){
      aa += clusterbytype[a]; 
      // printf("\n %d", clusterbytype[a]);
    }
    // printf("\n\nsum is %d", aa);
    m = 0;
    Maxint(clusterbytype, t, &m, &w);
    maxhabitat = ((int)m);
    int           habitathist[maxhabitat+1];

    // building the histogram
    memset(habitathist,0,sizeof(habitathist));
    for(i=0; i<t;i++) {
      habitathist[clusterbytype[i]]++;
    }

    double meanh, varh, var, sdh;
    int sumh;
    int count=0;
    sumh = 0;
    meanh = 0.;
    varh = 0.;
    sdh = 0.;
    for( i=0; i<(maxhabitat+1); i++ ){
      // printf("\n***habitathist is %d ", habitathist[i]);
      if( habitathist[i] ){
        count += habitathist[i];
        // printf("********habitathist is %d ", habitathist[i]);
        sumh += i*habitathist[i];
      }
    }
    // printf("\n sum is %d\n", sumh);
    meanh = ((double)(sumh)) / count;
    int l,n=0;
    for( i=0; i<(maxhabitat+1); i++ ){
      if( habitathist[i] ){
        n = habitathist[i];
        for(l=0; l<n; l++){
          // printf("\n i is %d, n is %d; mean is %f, diff is %f\n", i,n,meanh,fabs((((double)(i))) - meanh));
          varh += fabs((((double)(i))) - meanh) * fabs((((double)(i))) - meanh);
        }
      }
    }
    // printf("\n%f, count is %d, and variance is %f", varh, count, var=varh/((double)(count)));
    var = varh/((double)(count));
    sdh = sqrt(var);

    double relativecount[maxhabitat+1];
    // !!! could be problematic memset doesn't work for doubles!!!
    memset(relativecount,0.,sizeof(relativecount));
    int j=0;
    for( i=0; i<(maxhabitat+1); i++ ){
      if( habitathist[i] ){
        j++;
      }
    }
    // printf("\n---------habitathist is full in %d ", j);
    // printf("\ntotal count is %d \n", count);
    int k;
    for( k=0;k<j; ){
      for( i=0; i<(maxhabitat+1); i++ ){
        if( habitathist[i] == 0 ){
          continue;
        } else {
          relativecount[k] = (((double)(habitathist[i])) / ((double)(count)));
          // printf("\nhabitathist %d\n", habitathist[i]);
          // printf("\nrel %f\n", relativecount[k]);
          k++;
        }
      }
    }
    // for( k=0;k<j;k++ ) printf("\nrelative count is %f\n", relativecount[k]);
    // printf("\nj is %d\n", j);
    // printf("---------habitathist is %d ", habitathist[i]);
    // relativecount[j] = (((double)(habitathist[i])) / ((double)(count)));
    // initialize for diversity indices

    double simpson; 
    double shannon;
    simpson=0.,    // (1/sigma(p^2))*(1/S)
    shannon=0.;    // (-sigma(pln(p))/ln(S))

    for( i=0; i<j; i++ ){
      simpson += (relativecount[i] * relativecount[i]);
      shannon += (relativecount[i] * log(relativecount[i]));
    }

    simpson = ((1 / simpson) * (1 / ((double)(j))));
    shannon = (-shannon/log((double)(j)));

    fprintf(Habitatf, "%d ",    p);             // habitat type
    fprintf(Habitatf, "%d ",    count);         // number of clusters
    fprintf(Habitatf, "%d ",    j);             // diversity of patch sizes
    fprintf(Habitatf, "%d ",    maxhabitat);    // max patch size
    fprintf(Habitatf, "%lf ",   meanh);         // mean patch size
    fprintf(Habitatf, "%lf ",   sdh);           // standard deviation
    fprintf(Habitatf, "%lf ",   simpson);       // simpson's evenness index
    fprintf(Habitatf, "%lf ",   shannon);       // shannon's evenness index

  // printf("\n\n%d, %d, %d, %lf, %lf, %lf, %lf \n", count, j, maxhabitat, meanh, sdh, simpson, shannon);
  aaa += aa;
  }
  // printf("\n total is %d", aaa);
  // printf("\nf is %f\n", fragment);
  fprintf(Habitatf, "\n");
  fprintf(Habitatf, "%d ",    check);           // must be = width * height
  fprintf(Habitatf, "%d ",    tcount);          // number of clusters
  fprintf(Habitatf, "%d ",    maxp);            // max patch size
  fprintf(Habitatf, "%lf ",   tmeanh);          // mean patch size
  fprintf(Habitatf, "%lf ",   tsdh);            // standard deviation
  fprintf(Habitatf, "%lf ",   tsimpson);        // simpson's evenness index
  fprintf(Habitatf, "%lf ",   tshannon);        // shannon's evenness index
  fprintf(Habitatf, "%lf ",   fragment);        // fragment index % patches that are edges
  fprintf(Habitatf, "\n");
  fflush(Habitatf);

  // Increment counter
  Habitat_logs++;
}

#endif

// once per logging event (call inside of log()).
void ClusterIt(pThreadData td)
{
  int latx, longy;

  // initialize ->included to 0
  for( latx=0; latx<_WIDTH; latx++ ){
    for( longy=0; longy<_HEIGHT; longy++ ){
      td->Current[latx][longy].included = 0;
    }
  }

  ArbiterProgress();

  // Do the actual clustering
  int clusterno = 0;
  int noofpatches[_WIDTH * _HEIGHT];
  int patchtypeofc[_WIDTH * _HEIGHT];

  for( latx=0; latx<_WIDTH; latx++ ){
    for( longy=0; longy<_HEIGHT; longy++ ){
      int count = 0;
      if( RecursiveSearch(td->Current[latx][longy].n, &(td->Current[latx][longy]), &count, clusterno) ) {
        // At this place in the code, "clusterno" is the number of the current cluster and
        // "count" is the number of patches in the current cluster.
        // Move on to the next cluster
        noofpatches[clusterno]=count;
        patchtypeofc[clusterno]=td->Current[latx][longy].n;
        clusterno++;
      }
    }
  }

  // Print out results
  /*
  int i;
  for( i=0; i<clusterno; i++ ){
    printf("\ncluster (Cluster[i].clusterset) %d which has %d (Cluster[i].noofpatches) patches and habitat %d (Cluster[i].nichetype).", Cluster[i].clusterset, Cluster[i].noofpatches, Cluster[i].nichetype);
    printf("-----(clusterinfo[i]) %d", noofpatches[i]);
  }
  */
  int l,ss;
  for( ss=l=0; l<clusterno; l++){ 
    ss+=noofpatches[l];
    // printf("\nclusterno %d, noofp is %d, ptype is %d", l, noofpatches[l], patchtypeofc[l]);
  }
  // printf("\nsum is %d\n",ss);

  // calculates the precentage of patches that are edges
  // i.e. not surrounded by the same patch in 3x3 square
  int x, y, count;
  double fragment;

  // initialize ->edge to 0
  for( x=0; x<_WIDTH; x++ ){
    for( y=0; y<_HEIGHT; y++ ){
      td->Current[x][y].edge = 0;
    }
  }

  // new version for the calculation of %edges, the previous version has leaks and does not consider
  // when y == 0 and y == _HEIGHT-1 for different x values
  // trying with the already existing neighboring patches here
  int nep = 0;
  for( x=0; x<_WIDTH; x++ ){
    for( y=0; y<_HEIGHT; y++ ){
      for( nep = 0; nep < td->Current[x][y].nne; nep++){
        if( (td->Current[x][y].n != td->Current[x][y].ne[nep]->n) )
          td->Current[x][y].edge = 1;
      }
    }
  }

  count = 0;
  fragment = 0.;
  for( x=0; x<_WIDTH; x++ ){
    for( y=0; y<_HEIGHT; y++ ){
      if( td->Current[x][y].edge == 1 )
        count++;
    }
  }

  fragment = ((double)(count))/((double)(_WIDTH * _HEIGHT)) * 100.;
  //printf("\nfragment is %f\n",fragment);

  LogHabitat(clusterno, noofpatches, patchtypeofc, fragment);

}


// Swaps Current and Next generations and deps
static void SwapGenerations(pThreadData td)
{

  u64b t;  pIndividual *tp;  Space ts;  

  // Swap spaces
  ts = td->Current;
  td->Current = td->Next;
  td->Next = ts;

  // Swap Individual resources
  t = td->nICurrent;
  td->nICurrent = td->nINext;
  td->nINext = t;

  tp = td->ICurrentCleared;
  td->ICurrentCleared = td->INextCleared;
  td->INextCleared = tp;

  tp = td->ICurrent;
  td->ICurrent = td->INext;
  td->INext = tp;
}

// this function calculates the time spent in each patch by an individual
// time spent in a patch is calculated as a ratio of preference for the resource present in the current patch
// and to the sum pf preferences to the other patches 
static void TimeSpentRoam(pIndividual i, pPatch p)
{
  ArbiterProgress();
  // for roaming ranges
  int k,j; double sums=0.;

  for(k=0; k < (_RRANGE * _RRANGE); k++){
    i->TRoam[k] = 0.;
    //i->TRoamMax[k] = 0;
  }

  for(j=0; j < (p->rrne); j++){
    sums += (i->TRoam[j] = P(i,p->rne[j]));
  }
  for(j=0; j < (p->rrne); j++){
    i->TRoam[j] /= sums;
    // printf("\n%d/%d (%d, %d), %d, sum %f, tj %f",j, p->rrne, p->lat, p->lon, sums, i->TRoam[j]);
  }

  // after time spent by the focal ind is calculated, the info about its time spent in other patches is stored in those patches
  // see additional fields below to be added to structure patch in niche.h
  for(j=0; j < (p->rrne); j++){
    p->rne[j]->Other[p->rne[j]->nothers].ind = i;                   // while going through individuals this individual is saved as 'other' in another patch
    //p->rne[j]->Other[p->rne[j]->nothers].p = p;                     // the current coordinates, it is this individuals 'home' patch
    p->rne[j]->Other[p->rne[j]->nothers].timespent = i->TRoam[j];   // the time this individual spends in the other patches is recorded a property of those patches
    p->rne[j]->nothers++;                                           // increments the no of other individuals in the patch
    // printf("\n nothers is being calculated: %d at %d\n\n", p->rne[j]->nothers++, j);
    // if( Gen == 6) printf("\n%d/%d (%d,%d), time %f", j, p->rrne, p->lat, p->lon, i->TRoam[j]);
  }
}

// Initializes any data structures needed by a thread (mostly [if not all] local)
static void InitThread(pThreadData td)
//void InitThread(pThreadData td)
{
  u64b i,j,k,l;

  // Make a copy of array based parameters
  //ALLOC(td->Sigma_s,     _k*sizeof(double), "Could not allocate Sigma_s!\n");
  //ALLOC(td->Sigma_c,     _k*sizeof(double), "Could not allocate Sigma_c!\n");
  //ALLOC(td->a_i,         _k*sizeof(double), "Could not allocate a_i!\n");
  //ALLOC(td->log_Sigma_s, _k*sizeof(double), "Could not allocate log_Sigma_s!\n");
  //memcpy(td->Sigma_s,Sigma_s,_k*sizeof(double));
  //memcpy(td->Sigma_c,Sigma_c,_k*sizeof(double));
  //memcpy(td->a_i,    a_i,    _k*sizeof(double));

  // Precompute the log of Sigma_s
  //for(i=0; i<_k; i++)
  //  td->log_Sigma_s[i] = log(td->Sigma_s[i]);

  // Initialize Individuals and set up the "cleared" states
  ZALLOC(td->ICurrentData,   MAX_THREAD_POP*sizeof(Individual),  "Could not allocate IndividualData! (Cur)\n");
  ALLOC(td->ICurrent,        MAX_THREAD_POP*sizeof(pIndividual), "Could not allocate Individuals! (Cur)\n");
  ZALLOC(td->INextData,      MAX_THREAD_POP*sizeof(Individual),  "Could not allocate IndividualData! (Next)\n");
  ALLOC(td->INext,           MAX_THREAD_POP*sizeof(pIndividual), "Could not allocate Individuals! (Next)\n");
  ALLOC(td->ICurrentCleared, MAX_THREAD_POP*sizeof(pIndividual), "Could not allocate Cleared State! (Cur)\n");
  ALLOC(td->INextCleared,    MAX_THREAD_POP*sizeof(pIndividual), "Could not allocate Cleared State! (Next)\n");
  for(td->nICurrent=td->nINext=i=0; i<MAX_THREAD_POP; i++){
    td->ICurrent[i]      = td->ICurrentData+i;
    td->INext[i]         = td->INextData+i;
    LinkIndividual(td->ICurrent[i]);
    LinkIndividual(td->INext[i]);
  }
  memcpy(td->ICurrentCleared, td->ICurrent, MAX_THREAD_POP*sizeof(pIndividual));
  memcpy(td->INextCleared,    td->INext,    MAX_THREAD_POP*sizeof(pIndividual));

  // Malloc for our patch spaces
  ALLOC(td->Current,(WIDTH+2)*sizeof(Patch*),   "Could not allocate Current patch space!\n");
  for(i=0; i<(WIDTH+2); i++)
    ZALLOC(td->Current[i],HEIGHT*sizeof(Patch), "Could not allocate Current patch space!\n");
  ALLOC(td->Next,(WIDTH+2)*sizeof(Patch*),      "Could not allocate Next patch space!\n");
  for(i=0; i<(WIDTH+2); i++)
    ZALLOC(td->Next[i],HEIGHT*sizeof(Patch),    "Could not allocate Next patch space!\n");

  // Initialize the random number generator
  initrand(&R, Seed);

  // Figure out what patches to copy from start
#if _CPUS == 1
  // Only one thread:    Do not copy any wandering zones
  k = 1, l = WIDTH+1;
#else
  switch(td->id){
  case 0:         // Left-most thread:   Only copy right wandering zone
    k = 1, l = WIDTH+2;
    break;
  case (_CPUS-1): // Right-most thread:  Only  copy left wandering zone
    k = 0, l = WIDTH+1;
    break;
  default:        // Middle threads:     Copy both wandering zones
    k = 0, l = WIDTH+2;
  }
#endif

  // Actually copy them
  for(i=0; i<WIDTH; i++){
    // Copy into current
    memcpy(td->Current[i],    Start[i],          HEIGHT*sizeof(Patch));
    // Next matches current (no individuals)
    memcpy(td->Next[i],       td->Current[i],    HEIGHT*sizeof(Patch));
    for(j=0; j<HEIGHT; j++) td->Next[i][j].ni = 0;
  }

  // Copy extinction information
  /*
  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++) {
      td->Current[i][j].en = Start[i][j].en;
      ALLOC(td->Current[i][j].ei, td->Current[i][j].en*sizeof(ExtinctionInfo),
            "Could not allocate space for extinction information! (Wt)\n");
      memcpy(td->Current[i][j].ei, Start[i][j].ei,
             td->Current[i][j].en*sizeof(ExtinctionInfo));
      // Just make next point to current's extinction events
      td->Next[i][j].ei = td->Current[i][j].ei;
      td->Next[i][j].en = td->Current[i][j].en;
    }
  }*/

  // Fill in patch individuals
  for(i=0; i<WIDTH; i++){
    // *for(i=1; i<(WIDTH+1); i++) {
    // Copy individuals from start
    for(j=0; j<HEIGHT; j++){
      for(k=0; k<td->Current[i][j].ni; k++){
        //printf("\nenters the loop. start: %d, current: %d", Start[i][j].ni, td->Current[i][j].ni);
        CopyIndividual(td->Current[i][j].i[k]=GetCurrentIndividual(td),Start[i][j].i[k]);
        // Start[][]'s individuals are unsynced
        SyncIndividual(td,td->Current[i][j].i[k]);
      }
    }
  }

  // Now that patches are copied, pre-compute neighbors, but Since NeighboringPatches()
  // only fills in the Next generation, swap and process twice.
  SwapGenerations(td);
  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++){
      NeighboringPatches(td,i,j);
      NeighboringDispersePatches(td,i,j);
      NeighboringRoamPatches(td,i,j);
    }
  }

  SwapGenerations(td);
  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++){
      NeighboringPatches(td,i,j);
      NeighboringDispersePatches(td,i,j);
      NeighboringRoamPatches(td,i,j);
    }
  }

  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++) {
      td->Current[i][j].nothers=0;
      //td->Current[i][j].nothermates=0;
    }
  }

  // calculates who is using which patch for how long
  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++) {
      for(k=0,l=td->Current[i][j].ni; k < l; k++) {
        TimeSpentRoam(td->Current[i][j].i[k], &td->Current[i][j]);
        //TimeSpentMate(td->Current[i][j].i[k], &td->Current[i][j]);
      }
    }
  }

  // Setup recombination and mutation distributions
  MutateInit(td);
  CrossInit(td);
}

#if ENDIAN == NICHE_BIG_ENDIAN
// This macro will do an index translation that should be 
// functionally equvalent to byte-swapping.  
static int BSWAP[32] = {
 7,  6,  5,  4,  3,  2,  1,  0,
 15, 14, 13, 12, 11, 10, 9,  8,
 23, 22, 21, 20, 19, 18, 17, 16,
 31, 30, 29, 28, 27, 26, 25, 24
};
#define Endian(x) (BSWAP[x])
#else
#if ENDIAN == NICHE_LITTLE_ENDIAN
#define Endian(x) (x)
#else
#error ENDIAN _must_ be set to either NICHE_BIG_ENDIAN or NICHE_LITTLE_ENDIAN!
#endif
#endif

// Handles recombination and mutation for a single destinaation chromosome
static void RecombineEco(pThreadData td, byte *z, byte *x, byte *y)
{
  u64b i,r;  byte *t;

  // Swap sources
  if(rnd(&R, 2)){ t=x; x=y; y=t;}

#if SKIP_MUTATION
  // Move through each byte
  for(i=0,r=128; i<PLENGTH_E; i++){
    // Obtain a crossover mask for this byte and recombine
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[0]);
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
  }

#if _use_mue
  // Mutate all the bytes that should mutate
  for(i=0; (i+=drand(&R, td->MutateSkip[0])) < PLENGTH_E; i++)
    z[Endian(i)] = drand(&R, td->MutateDist[0][z[Endian(i)]]);
#endif
#else
  // Normal mutation
  for(i=r=0; i<PLENGTH_E; i++){
    // Obtain a crossover mask, recombine and mutate
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[0]);
#if _use_mue
    // Mutation
    z[Endian(i)] = drand(&R, td->MutateDist[0][(x[Endian(i)] & r)|(y[Endian(i)] & ~r)]);
#else
    // No mutation
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
#endif
  }
#endif
}


// Handles recombination and mutation for a single destinaation chromosome
static void RecombinePref(pThreadData td, byte *z, byte *x, byte *y)
{
  u64b i,r;  byte *t;

  // Swap sources
  if(rnd(&R, 2)) { t=x; x=y; y=t; }

#if SKIP_MUTATION
  // Move through each byte
  for(i=0,r=128; i<PLENGTH_P; i++){
    // Obtain a crossover mask for this byte and recombine
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[1]);
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
  }

#if _use_mup
  // Mutate all the bytes that should mutate
  for(i=0; (i+=drand(&R, td->MutateSkip[1])) < PLENGTH_P; i++)
    z[Endian(i)] = drand(&R, td->MutateDist[1][z[Endian(i)]]);
#endif
#else
  // Normal mutation
  for(i=r=0; i<PLENGTH_P; i++){
    // Obtain a crossover mask, recombine and mutate
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[1]);
#if _use_mup
    // Mutation
    z[Endian(i)] = drand(&R, td->MutateDist[1][(x[Endian(i)] & r)|(y[Endian(i)] & ~r)]);
#else
    // No mutation
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
#endif
  }
#endif
}


// Handles recombination and mutation for a single destinaation chromosome
static void RecombineNeutral(pThreadData td, byte *z, byte *x, byte *y)
{
  u64b i,r;  byte *t;

  // Swap sources
  if(rnd(&R, 2)) { t=x; x=y; y=t; }

#if SKIP_MUTATION
  // Move through each byte
  for(i=0,r=128; i<PLENGTH_N; i++){
    // Obtain a crossover mask for this byte and recombine
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[5]);
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
  }

#if _use_mun
  // Mutate all the bytes that should mutate
  for(i=0; (i+=drand(&R, td->MutateSkip[5])) < PLENGTH_N; i++)
    z[Endian(i)] = drand(&R, td->MutateDist[5][z[Endian(i)]]);
#endif
#else
  // Normal mutation
  for(i=r=0; i<PLENGTH_N; i++){
    // Obtain a crossover mask, recombine and mutate
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[5]);
#if _use_mun
    // Mutation
    z[Endian(i)] = drand(&R, td->MutateDist[5][(x[Endian(i)] & r)|(y[Endian(i)] & ~r)]);
#else
    // No mutation
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
#endif
  }
#endif
}


#if _DISTINCT_SEXES
// Handles recombination and mutation for a single destinaation chromosome
static void RecombineMating(pThreadData td, byte *z, byte *x, byte *y)
{
  u64b i,r;  byte *t;

  // Swap sources
  if(rnd(&R, 2)) { t=x; x=y; y=t; }

#if SKIP_MUTATION
  // Move through each byte
  for(i=0,r=128; i<PLENGTH_M; i++){
    // Obtain a crossover mask for this byte and recombine
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[2]);
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
  }

#if _use_mum
  // Mutate all the bytes that should mutate
  for(i=0; (i+=drand(&R, td->MutateSkip[2])) < PLENGTH_M; i++)
    z[Endian(i)] = drand(&R, td->MutateDist[2][z[Endian(i)]]);
#endif
#else
  // Normal mutation
  for(i=r=0; i<PLENGTH_M; i++){
    // Obtain a crossover mask, recombine and mutate
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[2]);
#if _use_mum
    // Mutation
    z[Endian(i)] = drand(&R, td->MutateDist[2][(x[Endian(i)] & r)|(y[Endian(i)] & ~r)]);
#else
    // No mutation
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
#endif
  }
#endif
}


// Handles recombination and mutation for a single destinaation chromosome
static void RecombineMarker(pThreadData td, byte *z, byte *x, byte *y)
{
  u64b i,r;  byte *t;

  // Swap sources
  if(rnd(&R, 2)) { t=x; x=y; y=t; }

#if SKIP_MUTATION
  // Move through each byte
  for(i=0,r=128; i<PLENGTH_K; i++) {
    // Obtain a crossover mask for this byte and recombine
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[3]);
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
  }

#if _use_muk
  // Mutate all the bytes that should mutate
  for(i=0; (i+=drand(&R, td->MutateSkip[3])) < PLENGTH_K; i++)
    z[Endian(i)] = drand(&R, td->MutateDist[3][z[Endian(i)]]);
#endif
#else
  // Normal mutation
  for(i=r=0; i<PLENGTH_K; i++) {
    // Obtain a crossover mask, recombine and mutate
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[3]);
#if _use_muk
    // Mutation
    z[Endian(i)] = drand(&R, td->MutateDist[3][(x[Endian(i)] & r)|(y[Endian(i)] & ~r)]);
#else
    // No mutation
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
#endif
  }
#endif
}


#if _SEXUAL_SELECTION
// Handles recombination and mutation for a single destinaation chromosome
static void RecombineFPref(pThreadData td, byte *z, byte *x, byte *y)
{

  u64b i,r;  byte *t;

  // Swap sources
  if(rnd(&R, 2)) { t=x; x=y; y=t; }

#if SKIP_MUTATION
  // Move through each byte
  for(i=0,r=128; i<PLENGTH_F; i++) {
    // Obtain a crossover mask for this byte and recombine
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[4]);
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
  }

#if _use_muf
  // Mutate all the bytes that should mutate
  for(i=0; (i+=drand(&R, td->MutateSkip[4])) < PLENGTH_F; i++)
    z[Endian(i)] = drand(&R, td->MutateDist[4][z[Endian(i)]]);
#endif
#else
  // Normal mutation
  for(i=r=0; i<PLENGTH_F; i++) {
    // Obtain a crossover mask, recombine and mutate
    r = (((r>>7)&1)-1) ^ drand(&R, td->CrossDist[4]);
#if _use_muf
    // Mutation
    z[Endian(i)] = drand(&R, td->MutateDist[4][(x[Endian(i)] & r)|(y[Endian(i)] & ~r)]);
#else
    // No mutation
    z[Endian(i)] = (x[Endian(i)] & r) | (y[Endian(i)] & ~r);
#endif
  }
#endif
}
#endif
#endif


// Combines parents m and f and fills in child c with correct recombination and mutation
static void FastRecombine(pThreadData td, pIndividual c, pIndividual m, pIndividual f)
{
  pIndividual t;
  ArbiterProgress();
  // Random parent swap
  if (rnd(&R, 2)) t = m, m = f, f = t;

  // Process ->x and ->y
  RecombineEco    (td, (byte*)(c->x0), (byte*)(m->x0), (byte*)(m->x1));
  RecombineEco    (td, (byte*)(c->x1), (byte*)(f->x0), (byte*)(f->x1));
  RecombinePref   (td, (byte*)(c->y0), (byte*)(m->y0), (byte*)(m->y1));
  RecombinePref   (td, (byte*)(c->y1), (byte*)(f->y0), (byte*)(f->y1));

#if _DISTINCT_SEXES
  // Process ->m and ->k
  RecombineMating (td, (byte*)(c->m0), (byte*)(m->m0), (byte*)(m->m1));
  RecombineMating (td, (byte*)(c->m1), (byte*)(f->m0), (byte*)(f->m1));
  RecombineMarker (td, (byte*)(c->k0), (byte*)(m->k0), (byte*)(m->k1));
  RecombineMarker (td, (byte*)(c->k1), (byte*)(f->k0), (byte*)(f->k1));
#if _SEXUAL_SELECTION
  // Process ->f
  RecombineFPref  (td, (byte*)(c->f0), (byte*)(m->f0), (byte*)(m->f1));
  RecombineFPref  (td, (byte*)(c->f1), (byte*)(f->f0), (byte*)(f->f1));
#endif
#endif

#if _Ln
  // Process ->z
  RecombineNeutral(td, (byte*)(c->z0), (byte*)(m->z0), (byte*)(m->z1));
  RecombineNeutral(td, (byte*)(c->z1), (byte*)(f->z0), (byte*)(f->z1));
#endif

}


// dispersal is done w.r.t. one of the neighboring patches of the mother's
// this could be modified to make the dispersal from the patch where mating took place
static void Disperse(pThreadData td, pIndividual i, pPatch p)
{
  int k;  static distribution *d = NULL;
  ArbiterProgress();

#if (!_PREF_DISPERSAL)
  // if there is no dispersal based on preference, then dispersal is random to one 
  // of the patches within the dispersal range
  if( !d ){
    // Only allocate the neighbor distribution the first time
    d = allocdist(_DRANGE * _DRANGE);
  }

  // Child will randomly to one of the patches within the range of the parent (female)
  d->n = p->ddne;
  k = rnd(&R, p->ddne);

  if(!InstallIndividual(p->dne[k],i)){
#if _CHILD_DISPERSAL
    ReleaseNextIndividual(td,i);
#else
    // ReleaseCurrent() here because SwapGen() has been called in Thread() between Mate(->Next) and Disperse(->Current)
    ReleaseCurrentIndividual(td,i);
#endif
  }

#else
  // preference based dispersal
  if( !d ){
    // Only allocate the neighbor distribution the first time
    d = allocdist(_DRANGE * _DRANGE);
  }

  double s;
  d->n = p->ddne;
  for( s=0.,k=0; k<p->ddne;k++ ){
    s += (d->p[k]=P(i,p->dne[k]));    // dispersal not affected by distance
  }

  if( !s ){
    k = rnd(&R, p->ddne);
    // printf("\ndestination is %d",k); 
  } else {
    initdist(d,s);
    k = drand(&R, d);
    //printf("\ndestination is %d/%d at (%d,%d)",k, p->ddne, p->dne[k]->lat, p->dne[k]->lon);
  }

  // the following is random dispersal
  //k = rnd(&R, p->ddne);

  /*
  {
    pPatch tp;

    for(tp=p->dne[k=0]; k<p->ddne; tp=p->dne[++k]) {
       if( p == tp ) {
         break;
       }
     }
     if( k >= p->ddne ) {
       printf("This should never happen!\n");
     }
  }
  */

  //newpopsize[p->dne[k]->lat][p->dne[k]->lon]++;

  //printf("\nfrom (%d,%d), to (%d,%d)", p->lat, p->lon, p->dne[k]->lat, p->dne[k]->lon);


  // sanity print
  // printf("\nfrom (%d,%d), to (%d,%d)", p->lat, p->lon, p->dne[k]->lat, p->dne[k]->lon);
  /*
  if( p->lat <  p->dne[k]->lat ) lessdx++;
  if( p->lat == p->dne[k]->lat ) samedx++;
  if( p->lat >  p->dne[k]->lat ) moredx++;
  if( p->lon <  p->dne[k]->lon ) lessdy++;
  if( p->lon == p->dne[k]->lon ) samedy++;
  if( p->lon >  p->dne[k]->lon ) moredy++;

  if( (p->lat <  p->dne[k]->lat) && (p->lon <  p->dne[k]->lon) ) dispsqr[0]++;
  if( (p->lat == p->dne[k]->lat) && (p->lon <  p->dne[k]->lon) ) dispsqr[1]++;
  if( (p->lat >  p->dne[k]->lat) && (p->lon <  p->dne[k]->lon) ) dispsqr[2]++;
  if( (p->lat <  p->dne[k]->lat) && (p->lon == p->dne[k]->lon) ) dispsqr[3]++;
  if( (p->lat == p->dne[k]->lat) && (p->lon == p->dne[k]->lon) ) dispsqr[4]++;
  if( (p->lat >  p->dne[k]->lat) && (p->lon == p->dne[k]->lon) ) dispsqr[5]++;
  if( (p->lat <  p->dne[k]->lat) && (p->lon >  p->dne[k]->lon) ) dispsqr[6]++;
  if( (p->lat == p->dne[k]->lat) && (p->lon >  p->dne[k]->lon) ) dispsqr[7]++;
  if( (p->lat >  p->dne[k]->lat) && (p->lon >  p->dne[k]->lon) ) dispsqr[8]++;
  */
  /*
  int    dd;
  double total = 0.;
  for( dd = 0; dd < 9; dd++){
    total += dispsqr[dd];
  }
  printf("\nDISPERSE\n%f - %f - %f\n%f - %f - %f\n%f - %f - %f", ((double)(dispsqr[0]))/total, ((double)(dispsqr[1]))/total, ((double)(dispsqr[2]))/total, ((double)(dispsqr[3]))/total, ((double)(dispsqr[4]))/total, ((double)(dispsqr[5]))/total, ((double)(dispsqr[6]))/total, ((double)(dispsqr[7]))/total, ((double)(dispsqr[8]))/total);
*/
  /*
  // sanity print
  if( (p->lat < _WIDTH/2)  && (p->lon < _WIDTH/2)  ){
    //printf("\nfrom 1");
    if( (p->dne[k]->lat < _WIDTH/2)   && (p->dne[k]->lon >= _WIDTH/2) ){
      dispersedata[1]++;
      //printf("- to 2");
    }
    if( (p->dne[k]->lat >= _WIDTH/2)  && (p->dne[k]->lon < _WIDTH/2) ){
      dispersedata[2]++;
      //printf("- to 3");
    }
  }
  if( (p->lat < _WIDTH/2)  && (p->lon >= _WIDTH/2) ){
    //printf("\nfrom 2");
    if( (p->dne[k]->lat < _WIDTH/2)  && (p->dne[k]->lon < _WIDTH/2)  ){
      dispersedata[4]++;
      //printf("- to 1");
    }
    if( (p->dne[k]->lat >= _WIDTH/2) && (p->dne[k]->lon >= _WIDTH/2) ){
      dispersedata[5]++;
      //printf("- to 4");
    }
  }
  if( (p->lat >= _WIDTH/2) && (p->lon < _WIDTH/2)  ){
    //printf("\nfrom 3");
    if( (p->dne[k]->lat < _WIDTH/2)  && (p->dne[k]->lon < _WIDTH/2)  ){
      dispersedata[7]++;
      //printf("- to 1");
    }
    if( (p->dne[k]->lat >= _WIDTH/2) && (p->dne[k]->lon >= _WIDTH/2) ){
      dispersedata[8]++;
      //printf("- to 4");
    }
  }
  if( (p->lat >= _WIDTH/2) && (p->lon >= _WIDTH/2) ){
    //printf("\nfrom 4");
    if( (p->dne[k]->lat < _WIDTH/2)   && (p->dne[k]->lon >= _WIDTH/2) ){
      dispersedata[10]++;
      //printf("- to 2");
    }
    if( (p->dne[k]->lat >= _WIDTH/2)  && (p->dne[k]->lon < _WIDTH/2) ){
      dispersedata[11]++;
      //printf("- to 3");
    }
  }
  */


  if(!InstallIndividual(p->dne[k],i)) {
#if _CHILD_DISPERSAL
    ReleaseNextIndividual(td,i);
    printf("release next\n");
#else
    // ReleaseCurrent() here because SwapGen() has been called in Thread() between Mate(->Next) and Disperse(->Current)
    ReleaseCurrentIndividual(td,i);
    printf("release current ------\n");
#endif
    Error("!! HITTING UPPER BOUND ON MAX_INDS\n");
  }

#endif

}


// mates m and f, and there is fertility selection
static void Mate(pThreadData td, pIndividual f, pIndividual m, pPatch p)
{
  ArbiterProgress();
  pIndividual i;
  int         j, rn;
  double      nc = 0.;

  nc = _b*w(td,f,p);
  rn = Poisson(&R, nc);

  // printf("no. of offspring seed (w) for Poisson: %f - no. of offspring: %d (%d, %d)\n", nc, rn, p->lat, p->lon);

  /*
  for (j = 0; j < 55; j++) printf("%llu ", R.rtab[j]);
  printf("%d ", R.rndx );
  printf("%d ", R.nflg );
  printf("%lf\n", R.nv );
  */

  // printf("\n");
  /*
  if( (p->lat < _WIDTH/2)  && (p->lon < _WIDTH/2)  ){
    dispersedata[0] += rn;
  }
  if( (p->lat < _WIDTH/2)  && (p->lon >= _WIDTH/2) ){
    dispersedata[3] += rn;
  }
  if( (p->lat >= _WIDTH/2) && (p->lon < _WIDTH/2)  ){
    dispersedata[6] += rn;
  }
  if( (p->lat >= _WIDTH/2) && (p->lon >= _WIDTH/2) ){
    dispersedata[9] += rn;
  }
  */

  for(j=0; j<((int)(rn)); j++){
    // Get an individual to use as a child
    i = GetNextIndividual(td);

#if _DISTINCT_SEXES
    // Set the child's sex
    i->s = rnd(&R, 2);
#endif

    // Recombine and mutate
    FastRecombine(td, i, m, f);

    // Sync, Handle dispersal and install child
    SyncIndividual(td,i);

    //printf("\nfemale: %d, male: %d, offspring: %d to ", f->s, m->s, i->s);

#if _CHILD_DISPERSAL
    Disperse(td, i, p);
#else
    p->i[p->ni++] = i;
#endif
  }

}

/*
// Checks for and handles an extinction event (returns 1 on extinction event)
static int HandleExtinction(pThreadData td, u64b x, u64b y)
{

  u64b k;

  // Handle extinction event
  if( (td->Current[x][y].ec < td->Current[x][y].en) && 
      (td->Current[x][y].ei[td->Current[x][y].ec].g == Gen) ){
    td->Current[x][y].ni = 0;
    // Fill in new niche triats and increment counter
    for(k=0; k<_k; k++){
      td->Current[x][y].l[k] = td->Next[x][y].l[k] = td->Current[x][y].ei[td->Current[x][y].ec].l[k];
    }

    td->Current[x][y].n = td->Next[x][y].n = td->Current[x][y].ei[td->Current[x][y].ec].n;
    td->Current[x][y].ec++;

    // Sync with next
    td->Next[x][y].ec = td->Current[x][y].ec;
    td->Next[x][y].en = td->Current[x][y].en;

    // Return that an event occurred
    return 1;
  }

  // Return no event
  return 0;

}
*/

/*
#if _CPUS > 1
// Handles extinction events in wandering zones
static void HandleWanderExtinction(pThreadData td)
{
  u64b i;

  switch(td->id) {
  case 0:         // Left thread:   Only do right buffer zone
    for(i=0; i<HEIGHT; i++) HandleExtinction(td,WIDTH,i);
    break;
  case (_CPUS-1): // Right thread:  Only do left buffer zone
    for(i=0; i<HEIGHT; i++) HandleExtinction(td,0,i);
    break;
  default:        // Middle thread: Do both buffer zones
    for(i=0; i<HEIGHT; i++) HandleExtinction(td,0,i);
    for(i=0; i<HEIGHT; i++) HandleExtinction(td,WIDTH,i);
  }
}
#endif
*/

// Does all the real work involved in the simulation.
void* THREAD(void *arg)
{

  ThreadData td; u64b h,i,j,k,l;  distribution *fdist;
  pIndividual matingmale;
  pIndividual matingmaleinrange;
  pPatch      matingpatchinrange;
  // Initialize this thread's portion of the simulation
  td.id = *((int*)arg);
  Threads[td.id] = &td;
  InitThread(&td);
  //printf("this is after InitThread, enters here\n");
  fdist = allocdist(_MAX_INDIVIDUALS);

  while(SignalDone(&td)){
    //memset(dispersedata,  0,  sizeof(dispersedata));
    //memset(matesqr,       0,  sizeof(matesqr));
    //memset(newpopsize,    0,  sizeof(newpopsize));
    for(i=0; i<WIDTH; i++){
      //printf("\n");
      for(j=0; j<HEIGHT; j++){

        //printf("%d %d %d %d (%d) -- ", td.Current[i][j].l[0], td.Current[i][j].l[1], td.Current[i][j].l[2], td.Current[i][j].l[3], td.Current[i][j].n);

        // printf("\npatch (%d, %d), N: %d", i,j, td.Current[i][j].ni);
        int fem=0; int mating=0;
        // loops through individuals in a patch
        for(h=0, l=td.Current[i][j].ni; h<l; h++){
          // Skip any males we encounter
          if( td.Current[i][j].i[h]->s == MALE ) continue;
          // if the individual is a female find a mate:
          fem++;
          // printf("\n(%d, %d) female count: %d (s=%d; ind no: %d/%d)", i,j, fem, td.Current[i][j].i[h]->s, h+1, td.Current[i][j].ni);
          // this is for Aaron's arbiter
          ArbiterProgress();
          // printf("\nfemale %d with range %d", fem, td.Current[i][j].mmne);
          int n,o=0,maleno=0;
          // initialize the sum of mating probabilities for initdist()
          double s=0.0;
          o=td.Current[i][j].ni;
          // printf("\nall %d", o);
          for(n=0; n<o; n++ ){
            // the following checks if it is a male, if so it calculates the mating probability
            if( td.Current[i][j].i[n]->s == MALE ){
              //printf("\nmale: %d (ind no: %d/%d)", td.Current[i][j].i[n]->s, n,o);
              maleno++;
              s += ( fdist->p[n] = Pm(td.Current[i][j].i[h], td.Current[i][j].i[n]) );
            } else {
              fdist->p[n] = 0.0;
            }
          }
          // printf("no. of males: %d", maleno);

          //----------------------------------------------------------------------------------------
          // this is the more complicated search for males in the entire roaming range...
          // if the sum is zero, or there are no males (these SHOULD mean the same thing) then
          // search for others in the range...
          if( !s || !maleno){
            // printf("\n*starting the mate search");
            // find the patch where she spends most of her time to find a mate
            // the loop is to find the patch where she spends her time secondmost, etc...
            // in case the patch she spends most of her time doesn't have any males in it
            int mr; u64b w; double m;
            // success is to make sure that we don't force the female to
            // mate with maleintr = 0 if the search is not successful
            // since we are mating them after exiting the search loop once the male is found
            // (to make the TRoam's positive again)
            int success = 0, maleintr = 0;
            for(mr=0, m=0.; mr < (td.Current[i][j].rrne); mr++ ){
              // printf("\n round %d", mr);
              Max(td.Current[i][j].i[h]->TRoam, td.Current[i][j].rrne, &m, &w);
              // initialize the TRoam to -ve so that if a male is not found,
              // this patch is not picked again...
              td.Current[i][j].i[h]->TRoam[w] = (td.Current[i][j].i[h]->TRoam[w] * -1.);
              //printf("\nno. of patches in mating range: %d, and max is %f at range %d; and the time spent is now set at: %f", td.Current[i][j].rrne, m, w, td.Current[i][j].i[h]->TRoam[w]);

              // if no ind. here skip it...
              if( !td.Current[i][j].rne[w]->ni){
                continue;
              }

              // there is someone in the patch, loop through individuals
              int n,others=0,males=0;
              double sr=0.0;
              others=td.Current[i][j].rne[w]->ni;
              //printf("\n no. of others in the patch (%d, %d) where the female spends most of her time is: %d", td.Current[i][j].rne[w]->lat, td.Current[i][j].rne[w]->lon, others);
              for(n=0; n<others; n++ ){
              // the following checks if it is a male, if so it calculates the mating probability
                if( td.Current[i][j].rne[w]->i[n]->s == MALE ){
                  //printf("\n this ind's s is: %d", td.Current[i][j].rne[w]->Other[n].ind->s);
                  matingmaleinrange = td.Current[i][j].rne[w]->i[n];
                  males++;
                  sr += ( fdist->p[n] = Pm(td.Current[i][j].i[h], matingmaleinrange) );
                } else {
                  sr += ( fdist->p[n] = 0. );
                }
              }
              //printf("\n pref mating - N = %d, no. of males %d, sr(fdist) %f", o, males, sr);

              // if there are no males, or sr == 0. after searching this patch, go to the next patch
              if( !males || !sr ){
                //printf("\nNO males here, need to search other pateches within the range");
                continue;
              }

              // this is the new version, if there are males in this patch,
              // break the search in the entire range, then reset the TRoam to +ve values
              // then mate...
              if( sr != 0. ){
                maleintr = 0;
                matingpatchinrange = td.Current[i][j].rne[w];
                fdist->n = td.Current[i][j].rne[w]->ni;
                initdist(fdist,sr);
                maleintr = drand(&R, fdist);
                // initialize the success to 1, so that we don't force mating with maleintr = 0;
                success = 1;
                break;
              }
            }

            // after we are out of the loop for search (with a male or without a male!),
            // the TRoams are initialized to +ve values again
            for(mr=0; mr < (td.Current[i][j].rrne); mr++ ){
              if( td.Current[i][j].i[h]->TRoam[mr] < 0. ){
                td.Current[i][j].i[h]->TRoam[mr] = (td.Current[i][j].i[h]->TRoam[mr] * -1.);
              } else {
                continue;
              }
              // printf("\nthe time spent is now set at: %f", td.Current[i][j].i[h]->TRoam[mr]);
            }

            // the mating is moved here, outside the range search...
            if( success != 0 ){
              Mate(&td,td.Current[i][j].i[h],matingpatchinrange->i[maleintr], &td.Next[i][j]);
              mating++;
            }
            // printf("\nOK, picks a mate by preference\n");
            // the search in the entire roaming range should be over by now...
            //--------------------------------------------------------------------------------------
          } else {
            int maleint=0;
            fdist->n = td.Current[i][j].ni;
            initdist(fdist,s);
            maleint = drand(&R, fdist);
            matingmale = td.Current[i][j].i[maleint];
            Mate(&td, td.Current[i][j].i[h], matingmale, &td.Next[i][j]);
            // printf("\n*mating between female %d(%d) & male %d (%d)\n\n", td.Current[i][j].i[h]->s, h, matingmale->s, maleint);
            mating++;

          }
        }
        // if( fem !=0 && mating == 0 ) printf("\nfemale no:%d mating:%d!\n", fem, mating);
      }
    }

    for(i=0; i<WIDTH; i++){
      for(j=0; j<HEIGHT; j++){
        td.Current[i][j].ni = 0;
      }
    }

    // Free and advnace to next generation
    ReleaseAllCurrentIndividuals(&td);
    SwapGenerations(&td);

    for(i=0; i<WIDTH; i++){
      for(j=0; j<HEIGHT; j++){
        td.Current[i][j].nothers=0;
      }
    }

    // calculates who is using which patch for how long
    for(i=0; i<WIDTH; i++){
      for(j=0; j<HEIGHT; j++){
        for(k=0,l=td.Current[i][j].ni; k < l; k++) {
          TimeSpentRoam(td.Current[i][j].i[k], &td.Current[i][j]);
        }
      }
    }

    /*
    // sanity print
    for(i=0; i<WIDTH; i++){
      for(j=0; j<HEIGHT; j++){
        printf("\nend of generation, nothers (at %d,%d) is: %d", i, j, td.Current[i][j].nothers);
      }
    }
    */

  } // while signal done
  return NULL;
} // void main
