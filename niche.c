#ifndef __NICHE_C
#define __NICHE_C

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

// Fill in an array of thread start functions depending on _CPUs
#if    _CPUS == 1
extern void* Thread_0(void*);
WorkThread WorkThreads[_CPUS] = {Thread_0};
/*
#elif  _CPUS == 2
extern void* Thread_0(void*);
extern void* Thread_1(void*);
WorkThread WorkThreads[_CPUS] = {Thread_0, Thread_1};
#elif  _CPUS == 4
extern void* Thread_0(void*);
extern void* Thread_1(void*);
extern void* Thread_2(void*);
extern void* Thread_3(void*);
WorkThread WorkThreads[_CPUS] = {Thread_0, Thread_1, Thread_2, Thread_3};
#elif  _CPUS == 8
extern void* Thread_0(void*);
extern void* Thread_1(void*);
extern void* Thread_2(void*);
extern void* Thread_3(void*);
extern void* Thread_4(void*);
extern void* Thread_5(void*);
extern void* Thread_6(void*);
extern void* Thread_7(void*);
WorkThread WorkThreads[_CPUS] = {Thread_0, Thread_1, Thread_2, Thread_3,
                                  Thread_4, Thread_5, Thread_6, Thread_7};

*/
#else
#error !! This code needs to be modified to support _CPUS != 1.
#endif

RndState    R;

//extern void LogHabitat(int clusterno, int *clusterinfo);
//extern void InitThread(pThreadData td);

// Muticies, condition variables, and flags for thread syncronization
volatile    pthread_cond_t  Master,Slaves;
volatile    pthread_mutex_t Mutex;
volatile    int             Go[_CPUS],Done[_CPUS];

// Each thread gets a ThreadData structure which will hold information
// about the thread as well as a copy of some parameters.
// This is the main() thread's ThreadData structure:
ThreadData  Mtd;
pThreadData Mt = &Mtd;

// Array of pointers to the work threads ThreadData strcutures.
// These are malloc'd by the threads themselves.
volatile    pThreadData     Threads[_CPUS];

// Random Seed, and starting space state.  These are filled in by the main
// thread and copied by the work threads.
u64b        Seed;
Space       Start;

// Current generation number and the number of generations to run for
u64b        Gen,NGen;

// Array of all possible niches
Patch       Niches[NICHES];

double      Xi[] = _Xi;
double      X[_WIDTH+1][_HEIGHT+1];
int         pmtwo[(_WIDTH+1)][(_WIDTH+1)];      // new 'map' with assigned resources

// log files
#if _SYS
u64b        Sys_logs;
FILE       *Sysf;
#endif
#if _DEME
u64b        Deme_logs;
FILE       *Demef;
#endif
#if _HABITAT
u64b        Habitat_logs;
FILE       *Habitatf;
#endif
#if _RANGE
u64b        Range_logs;
FILE       *Rangef;
#endif
#if _HISTO
u64b        Histo_logs;
FILE       *Histof;
#endif
#if _XVSS
u64b        Xvss_logs;
FILE       *Xvssf;
#endif
#if _TRANS
u64b        Trans_logs;
FILE       *Transf;
#endif
#if _NEWRESOURCED
u64b        Newrd_logs;
FILE       *Newrdf;
#endif
#if _IND
u64b        Ind_logs;
FILE       *Indf;
#endif
// Benchmark counters
#if _BENCHMARK
u64b        ProcessTime,nProcessed;
#endif

// Code shared with thread.c
#include "shared.h"

// !!av: hack, should be cleaner elsewhere
extern void ClusterIt(pThreadData td);

static void error( int x, char *s, ... )
{
  va_list args;
  va_start( args, s );
  vfprintf( stderr, s, args );
  if(x)
    exit(x);
}

// A number of functions are placed here which are very similar to 
// those in shared.h.  However, they are only needed in niche.c
// and this will keep the compiler quiet.  You'll find these below:

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
    //printf("\n\n\nj, %d, sum %f, tj %f\n\n\n",j, sums, i->TRoam[j]);
  }

  //for(k=0; k < (_RRANGE * _RRANGE); k++){
  //  i->TRoamMax[k] = i->TRoam[k];
  //}
  // after time spent by the focal ind is calculated, the info about its time spent in other patches is stored in those patches
  // see additional fields below to be added to structure patch in niche.h
  for(j=0; j < (p->rrne); j++){
    p->rne[j]->Other[p->rne[j]->nothers].ind = i;                   // while going through individuals this individual is saved as 'other' in another patch
    //p->rne[j]->Other[p->rne[j]->nothers].p = p;                     // the current coordinates, it is this individuals 'home' patch
    p->rne[j]->Other[p->rne[j]->nothers].timespent = i->TRoam[j];   // the time this individual spends in the other patches is recorded a property of those patches
    p->rne[j]->nothers++;                                           // increments the no of other individuals in the patch
    // printf("\nnothers is being calculated: %d at %d", p->rne[j]->nothers++, j);
  }
}

// Returns the number of individuals used in the current
// Note: This count _will_ include wanderers if there are any.
static u64b nCurrentIndividuals()
{
  u64b i,m;

  for(m=i=0; i<_CPUS; i++)
    m += Threads[i]->nICurrent;
  return m;
}


// Syncs the "niche value" of patch p from it's traits
static void SyncPatch(pPatch p)
{
  u64b k;

  for(p->n=k=0; k<_k; k++){
    p->n |= ((u64b)p->l[k])<<k;
  //printf("\n %d---", p->n);
  }
  p->n = log(p->n)/log(2);
  //printf("%d", p->n);
}


/*
// Releases an Individual back to the it's individual array
static void ReleaseCurrentIndividual(pThreadData td, pIndividual i)
{
  // Swap last in use with i and dec 
  td->ICurrent[i->id]         = td->ICurrent[--td->nICurrent];
  td->ICurrent[td->nICurrent] = i;

  // Update allocated from (id) field 
  td->ICurrent[i->id]->id  = i->id;
}
*/


// Stop-Watch for temporary timing:
// I could be wrong, but I think all this stopwatch code is only used by the 
// benchmarking code.  (see _BENCHMARK)
struct timeval sw_st,sw_et;

#if 0
// Starts the stopwatch
static void swStart()
{
  gettimeofday(&sw_st,NULL);
}

// Returns the amount of time that has passed since the stopwatch was started.
// So the name of swStop() isn't really the best I suppose.
static double swStop()
{
  gettimeofday(&sw_et,NULL);
  return ((sw_et.tv_sec*((u64b)1000000))+sw_et.tv_usec) -
         ((sw_st.tv_sec*((u64b)1000000))+sw_st.tv_usec);
}
#endif


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


// Sets m to be the max value in the size l array d, and sets w to be the index of m in d
static void Maxint(int *d, u64b l, int *m, u64b *w)
{
  int i,c,y[1024];

  // Find the max and indicies
  for(i=c=0; i<l; i++) {
    if(d[i] > *m) {
      //New max; store it, and reset t and c
      *m = d[i];
      y[0] = i;
      c=1;
    } else if (d[i] == *m) {
      // Tie; record the tie
      y[c++] = i;
    }
  }

  // Break any ties with a random choice
  *w = y[rnd(&R, c)];
}


// calculates ranges of different species
int RecursiveRangeSearchID(int speciesid, Patch *current, int *count, int rangeno, int *popsize)
{
  int i;

  // Check to see if the currenty patch is one we care about
  if( speciesid == 0 )                          return *count;
  if( !current->ni)                             return *count;
  if( current->SRangeIncluded[speciesid] == 1 ) return *count;
  if( current->AllSpecies[speciesid] == 0 )     return *count;
  if( current->speciespops[speciesid] == 0 )    return *count;

  // Add this patch to the current cluster
  current->SRangeIncluded[speciesid] = 1;
  current->rangeid  = rangeno;
  (*count)++;
  (*popsize)  += current->speciespops[speciesid];

  // Recurse to neighboring "Roaming" patches
  for(i=0; i < current->rrne; i++){
    if( current->rne[i]->AllSpecies[speciesid] == 0 )  continue;
    if( !current->rne[i]->ni )                         continue;
    if( current->rne[i]->speciespops[speciesid] == 0 ) continue;
    RecursiveRangeSearchID(speciesid,current->rne[i],count,rangeno,popsize);
  }

  return *count;
  return *popsize;
}


// calculates species ranges
void CalculateRangeID(int rangeno, int *noofpatchesinr, int *speciesidinr, int *speciespop)
{
  int           i,m,p,r,t,maxrange,clusterbysid[rangeno],aaa,finalpop;
  u64b          w;

#if _RANGE
  if( !(Gen%_RANGE ) )
    fprintf(Rangef, "gen: ");
#endif

  aaa = 0;
  /*
  for( i=0; i<rangeno; i++) { 
    printf("\n%d",  noofpatchesinr[i]);
    printf("--%d",  speciesidinr[i]);
    printf("--%lf", speciesmatch[i]);
  }
  */

  // the following seperates the ranges according to species id and does the calculations
  for( p=0; p<((int)(pow(2.,(double)(_k))))+1; p++ ){
    finalpop = 0;
    memset(clusterbysid,0,sizeof(clusterbysid));
    for( t=r=0; r<rangeno; r++ ){
      if( speciesidinr[r] == 0 ) continue;
      if( ((int)(speciesidinr[r])) == p ){
        clusterbysid[t]   = noofpatchesinr[r];
        noofpatchesinr[r] = 0;
        speciesidinr[r]   = 0;
        finalpop         += speciespop[r];
        // printf("\n************************ pop is %d\n", finalpop);
        t++;
      } else {
        continue;
      }
    }

    int a,aa=0;
    for( a=0; a<t; a++ ){
      aa += clusterbysid[a];
      // printf("\n %d", clusterbysid[a]);
    }
    // printf("\n\nsum is %d", aa);
    m = 0;
    maxrange = 0;
    Maxint(clusterbysid, t, &m, &w);
    maxrange = ((int)(m));
    int       speciesrangehist[maxrange+1], sumh, count;
    double    meanh, varh, var, sdh;
    count =   0;
    sumh  =   0;
    meanh =   0.;
    varh  =   0.;
    sdh   =   0.;

    if( maxrange == 0 ){
      fprintf(Rangef, "%llu ",  Gen);
      fprintf(Rangef, "%d ",    0);       // species id
      fprintf(Rangef, "%d ",    0);       // number of ranges (isolated ranges)
      fprintf(Rangef, "%d ",    0);       // diversity of range sizes
      fprintf(Rangef, "%d ",    0);       // max range size
      fprintf(Rangef, "%lf ",   0.);      // mean range size
      fprintf(Rangef, "%lf ",   0.);      // standard deviation
      fprintf(Rangef, "%lf ",   0.);      // simpson's evenness index
      fprintf(Rangef, "%lf ",   0.);      // shannon's evenness index
      fprintf(Rangef, "%d ",    0);       // total pop size of species
      fprintf(Rangef, "\n");
      continue;

    } else {
      // printf("\n\n\n\n****************** sm is %lf\n\n\n\n", sm);
      // printf("\n\nspecies id is %d", p);
      // printf("\n***************** sp %d's max is %d\n",p,maxrange);
      // building the histogram
      memset(speciesrangehist,0,sizeof(speciesrangehist));
      for(i=0; i<t;i++) {
        speciesrangehist[clusterbysid[i]]++;
      }

      for( i=0; i<(maxrange+1); i++ ){
        // printf("\n***speciesrangehist is %d ", speciesrangehist[i]);
        if( speciesrangehist[i] ){
          count += speciesrangehist[i];
          // printf("********speciesrangehist is %d ", speciesrangehist[i]);
          sumh += i*speciesrangehist[i];
        }
      }
      // printf("\n sum is %d\n", sumh);
      meanh = ((double)(sumh)) / count;
      int l,n=0;
      for( i=0; i<(maxrange+1); i++ ){
        if( speciesrangehist[i] ){
          n = speciesrangehist[i];
          for(l=0; l<n; l++){
            // printf("\n i is %d, n is %d; mean is %f, diff is %f\n", i,n,meanh,fabs((((double)(i))) - meanh));
            varh += fabs((((double)(i))) - meanh) * fabs((((double)(i))) - meanh);
          }
        }
      }
      // printf("\n%f, count is %d, and variance is %f", varh, count, var=varh/((double)(count)));
      var = varh/((double)(count));
      sdh = sqrt(var);

      double relativecount[maxrange+1];
      memset(relativecount,0.,sizeof(relativecount));
      int j=0;
      for( i=0; i<(maxrange+1); i++ ){
        if( speciesrangehist[i] ){
          j++;
        }
      }
      // printf("\n---------speciesrangehist is full in %d ", j);
      // printf("\ntotal count is %d \n", count);
      int k;
      for( k=0;k<j; ){
        for( i=0; i<(maxrange+1); i++ ){
          if( speciesrangehist[i] == 0 ){
            continue;
          } else {
            relativecount[k] = (((double)(speciesrangehist[i])) / ((double)(count)));
            // printf("\nspeciesrangehist %d\n", speciesrangehist[i]);
            // printf("\nrel %f\n", relativecount[k]);
            k++;
          }
        }
      }
      // for( k=0;k<j;k++ ) printf("\nrelative count is %f\n", relativecount[k]);
      // printf("\nj is %d\n", j);
      // printf("---------speciesrangehist is %d ", speciesrangehist[i]);
      // relativecount[j] = (((double)(speciesrangehist[i])) / ((double)(count)));
      // initialize for diversity indices

      double    simpson;
      double    shannon;
      simpson = 0.,    // (1/sigma(p^2))*(1/S)
      shannon = 0.;    // (-sigma(pln(p))/ln(S))

      if( j == 1 ){
        simpson = 1.;
        shannon = 1.;
      } else {
        for( i=0; i<j; i++ ){
          simpson += (relativecount[i] * relativecount[i]);
          shannon += (relativecount[i] * log(relativecount[i]));
        }
        simpson = ((1 / simpson) * (1 / ((double)(j))));
        shannon = (-shannon/log((double)(j)));
      }

      fprintf(Rangef, "%llu ",  Gen);
      fprintf(Rangef, "%d ",    p);             // species id
      fprintf(Rangef, "%d ",    count);         // number of ranges (isolated ranges)
      fprintf(Rangef, "%d ",    j);             // diversity of range sizes
      fprintf(Rangef, "%d ",    maxrange);      // max range size
      fprintf(Rangef, "%lf ",   meanh);         // mean range size
      fprintf(Rangef, "%lf ",   sdh);           // standard deviation
      fprintf(Rangef, "%lf ",   simpson);       // simpson's evenness index
      fprintf(Rangef, "%lf ",   shannon);       // shannon's evenness index
      fprintf(Rangef, "%d ",    finalpop);      // pop size of species
      // printf("%d, %d, %d, %lf, %lf, %lf, %lf, %lf\n", count, j, maxrange, meanh, sdh, simpson, shannon, sm);
      fprintf(Rangef, "\n");
      aaa += aa;

    }
  }
  // printf("\n total is %d", aaa);
  fflush(Rangef);

  // Incriment counter
  Range_logs++;
}


void ClusterByID(pThreadData td)
{

  int latx, longy, s;

  // initialize ->SRangeIncluded[] to 0
  for( latx=0; latx<_WIDTH; latx++ ){
    for( longy=0; longy<_HEIGHT; longy++ ){
      for( s=0; s<16; s++){
        td->Current[latx][longy].SRangeIncluded[s] = 0;
      }
    }
  }

  // Do the actual clustering
  int count, popsize, rangeno = 0;
  int noofpatchesinr [_WIDTH * _HEIGHT];
  int speciesidinr   [_WIDTH * _HEIGHT];
  int speciespop     [_WIDTH * _HEIGHT];

  memset(noofpatchesinr, 0, sizeof(noofpatchesinr));
  memset(speciesidinr,   0, sizeof(speciesidinr));
  memset(speciespop,     0, sizeof(speciespop));

  for( latx=0; latx<_WIDTH; latx++ ){
    for( longy=0; longy<_HEIGHT; longy++ ){
      count = 0;
      popsize = 0;
      for( s=0; s<16; s++){
        if( td->Current[latx][longy].AllSpecies[s] == 0 ) continue;
        if( RecursiveRangeSearchID(s, &(td->Current[latx][longy]), &count, rangeno, &popsize) ) {
          noofpatchesinr [rangeno] = count;
          speciesidinr   [rangeno] = s;
          speciespop     [rangeno] = popsize;
          rangeno++;
          // printf("\n(%d, %d) count %d, rangeno is %d, species is %d, pop is %d", latx, longy, count, rangeno, s, popsize);
        }
      }
    }
  }

  // Print out results to check for the total range size
  int l, sumr;
  for( sumr=l=0; l<rangeno; l++){
    sumr += noofpatchesinr[l];
    // printf("\ntotal count %d, noofranges is %d, and species id is %d", rangeno, noofpatchesinr[l], speciesidinr[l]);
  }
  // printf("\n sum is %d", sumr);
  CalculateRangeID(rangeno, noofpatchesinr, speciesidinr, speciespop);

}


void SpeciesPopHist(pThreadData td)
{

  int     latx, longy, k, t;
  int     speciestag[32];
  int     tagpopsize[32];

  memset(speciestag,0,sizeof(speciestag));
  memset(tagpopsize,0,sizeof(tagpopsize));
  t = ((int)(pow((double)(2),(double)(_k))));

  // initialize ->included to 0
  for( latx=0; latx<_WIDTH; latx++ ){
    for( longy=0; longy<_HEIGHT; longy++ ){
      for( k=0; k<t; k++){
        if( td->Current[latx][longy].AllSpecies[k] == 1 ){
          speciestag[k] = k;
          tagpopsize[k] += td->Current[latx][longy].speciespops[k];
        }
      }
    }
  }

#if _HISTO
  if( !(Gen%_HISTO ) )
    fprintf(Histof, "gen: ");

  for( k=0; k<t; k++){
    fprintf(Histof, "%llu ",  Gen);
    fprintf(Histof, "%d ",    k);                 // species id
    fprintf(Histof, "%d ",    speciestag[k]);     // species id
    fprintf(Histof, "%d ",    tagpopsize[k]);     // species pop size
    //printf("%d, %d, %d \n", k, speciestag[k], tagpopsize[k]);
    fprintf(Histof, "\n");

  }
  fflush(Histof);
  // Increment counter
  Histo_logs++;
#endif

}


void TransectData(pThreadData td)
{

  int     i, j, k, l, r, n;
  r = -1;
  double  ymax = 0.;
  double  tda[_WIDTH][_k*3+1];
  double  tdb[_WIDTH][_k*3+1];
  double  tdc[_WIDTH][_k*3+1];
  double  tdd[_WIDTH][_k*3+1];

  // initialize
  for( i=0; i<_WIDTH; i++ ){
    for( k=0; k<(_k*3+1); k++){
      tda[i][k] = 0.;
      tdb[i][k] = 0.;
      tdc[i][k] = 0.;
      tdd[i][k] = 0.;
    }
  }

  for( i=0; i<_WIDTH; i++ ){
    for( j=0; j<_HEIGHT; j++ ){
      n = td->Current[i][j].ni;
      for( l=0; l<n; l++){
        for( k=0; k<_k; k++){
          if( td->Current[i][j].i[l]->y[k] > ymax )
            ymax = td->Current[i][j].i[l]->y[k];
        }
      }
    }
  }
  //printf("\nymax:%f", ymax);

  for( i=0; i<_WIDTH; i++ ){
    for( j=0; j<_HEIGHT; j++ ){
      n = td->Current[i][j].ni;

      // transect C here
      if( i == j ){
        for( k=0; k<_k; k++){
          if (td->Current[i][j].l[k] != 0){
            r = k;
          }
        }
        tdc[i][r]++;
        if( n > 0 ){
          for( l=0; l<n; l++){
            tdc[i][12] += K(td->Current[i][j].i[l],&td->Current[i][j]);
            for( k=0; k<_k; k++){
              tdc[i][k+4] += td->Current[i][j].i[l]->x[k];
              if( ymax ){
                tdc[i][k+8] += td->Current[i][j].i[l]->y[k]/ymax;
              } else {
                tdc[i][k+8] += td->Current[i][j].i[l]->y[k];
              }
            }
          }
          for( k=4; k<(_k*3+1); k++){
            tdc[i][k] /= n;
          }
        }
      }

      // transect D here
      if( i == (_HEIGHT - j) ){
        for( k=0; k<_k; k++){
          if (td->Current[i][j].l[k] != 0){
            r = k;
          }
        }
        tdd[i][r]++;
        if( n > 0 ){
          for( l=0; l<n; l++){
            tdd[i][12] += K(td->Current[i][j].i[l],&td->Current[i][j]);
            for( k=0; k<_k; k++){
              tdd[i][k+4] += td->Current[i][j].i[l]->x[k];
              if( ymax ){
                tdd[i][k+8] += td->Current[i][j].i[l]->y[k]/ymax;
              } else {
                tdd[i][k+8] += td->Current[i][j].i[l]->y[k];
              }
            }
          }
          for( k=4; k<(_k*3+1); k++){
            tdd[i][k] /= n;
          }
        }
      }

      // transect B here
      if( i == (_WIDTH/2) ){
        for( k=0; k<_k; k++){
          if (td->Current[i][j].l[k] != 0){
            r = k;
          }
        }
        tdb[j][r]++;
        if( n > 0 ){
          for( l=0; l<n; l++){
            tdb[j][12] += K(td->Current[i][j].i[l],&td->Current[i][j]);
            for( k=0; k<_k; k++){
              tdb[j][k+4] += td->Current[i][j].i[l]->x[k];
              if( ymax ){
                tdb[i][k+8] += td->Current[i][j].i[l]->y[k]/ymax;
              } else {
                tdb[i][k+8] += td->Current[i][j].i[l]->y[k];
              }
            }
          }
          for( k=4; k<(_k*3+1); k++){
            tdb[j][k] /= n;
          }
        }
      }

      // transect A here
      if( j == (_HEIGHT/2) ){
        for( k=0; k<_k; k++){
          if (td->Current[i][j].l[k] != 0){
            r = k;
          }
        }
        tda[i][r]++;
        if( n > 0 ){
          for( l=0; l<n; l++){
            tda[i][12] += K(td->Current[i][j].i[l],&td->Current[i][j]);
            for( k=0; k<_k; k++){
              tda[i][k+4] += td->Current[i][j].i[l]->x[k];
              if( ymax ){
                tda[i][k+8] += td->Current[i][j].i[l]->y[k]/ymax;
              } else {
                tda[i][k+8] += td->Current[i][j].i[l]->y[k];
              }
            }
          }
          for( k=4; k<(_k*3+1); k++){
            tda[i][k] /= n;
          }
        }
      }
    }
  }

  //if( !(Gen%_TRANS) )
  //  fprintf(Transf, "gen: ");

  //fprintf(Transf, "%llu ",  Gen);
  for( i=0; i<_WIDTH; i++ ){
    fprintf(Transf, "%d ", i);
    for( k=0; k<(_k*3+1); k++){
      fprintf(Transf, "%f ", tda[i][k]);
    }
    for( k=0; k<(_k*3+1); k++){
      fprintf(Transf, "%f ", tdb[i][k]);
    }
    for( k=0; k<(_k*3+1); k++){
      fprintf(Transf, "%f ", tdc[i][k]);
    }
    for( k=0; k<(_k*3+1); k++){
      fprintf(Transf, "%f ", tdd[i][k]);
    }
    fprintf(Transf, "\n"); 
  }

  fflush(Transf);
  // Increment counter
  Trans_logs++;


}

// the following are copied from somewhere else
// midpoint displacement and successive random additions in 2 dimensions
// Saupe (1988)
// plus fractal realizer
double FThree(double delta, double x0, double x1, double x2)
{
  double y = 0.0;
  y = (x0 + x1 + x2)/3. + delta*normal(&R, 0.,_SIGMAR);
  return y;
}

double FFour(double delta, double x0, double x1, double x2, double x3)
{
  double y = 0.0;
  y = (x0 + x1 + x2 + x3)/4. + delta*normal(&R, 0.,_SIGMAR);
  return y;
}

void Midpoint()
{
  int     i, j, N, stage, DD, d;
  double  delta;

  delta = _SIGMAR;
  N     = ((int)(pow(2.,((double)(_MAXLEVEL)))));

  if(((int)(N)) != ((int)(_WIDTH)))
       printf("--- landscape size does not match!!!\n");

  DD  = N;
  d   = N/2;

  // loop for number of iterations
  for(stage = 0; stage <_MAXLEVEL; stage++){
    // going from grid type I to type II
    delta = delta * pow(0.5, (0.5 * _H));
    // interpolate and offset points
    for(i=d; i<(N-d)+1; i+=DD) {
      for(j=d; j<(N-d)+1; j+=DD){
        X[i][j]=FFour(delta,X[i+d][j+d],X[i+d][j-d],X[i-d][j+d],X[i-d][j-d]);
      }
    }

#if _ADDITION
    for(i=0; i<N+1; i+=DD){
      for(j=0; j<N+1; j+=DD){
        X[i][j]=X[i][j] + delta * normal(&R, 0.,_SIGMAR);
      }
    }
#endif

    // going from grid type I to type II
    delta = delta * pow(0.5, (0.5 * _H));
    // interpolate and offset points
    for(i=d; i<(N-d)+1; i+=DD){
      X[i][0] = FThree(delta, X[i+d][0],X[i-d][0],X[i][d]);
      X[i][N] = FThree(delta, X[i+d][N],X[i-d][N],X[i][N-d]);
      X[0][i] = FThree(delta, X[0][i+d],X[0][i-d],X[d][i]);
      X[N][i] = FThree(delta, X[N][i+d],X[N][i-d],X[N-d][i]);
    }

    // interpolate and offset points
    for(i=d; i<(N-d)+1; i+=DD){
      for(j=DD; j<(N-d)+1; j+=DD){
        X[i][j]=FFour(delta,X[i][j+d],X[i][j-d],X[i+d][j],X[i-d][j]);
      }
    }
    // interpolate and offset points (differs from above by the start conditions!)
    for(i=DD; i<(N-d)+1; i+=DD){
      for(j=d; j<(N-d)+1; j+=DD){
        X[i][j]=FFour(delta,X[i][j+d],X[i][j-d],X[i+d][j],X[i-d][j]);
      }
    }

#if _ADDITION
    for(i=0; i<N+1; i+=DD){
      for(j=0; j<N+1; j+=DD){
        X[i][j]=X[i][j] + delta * normal(&R, 0.,_SIGMAR);
        }
    }
    for(i=d; i<(N-d)+1; i+=DD){
      for(j=d; j<(N-d)+1; j+=DD){
        X[i][j]=X[i][j] + delta * normal(&R, 0.,_SIGMAR);
        }
    }
#endif

    DD /= 2;
    d  /= 2;
  }

}


static int dblCmp( const void *a, const void *b )
{
  double aa = *(double*)a;
  double bb = *(double*)b;
  return ( aa > bb ? -1: ( aa < bb ? 1 : 0 ) );
}


void ProbabilityMap(pThreadData td)
{
  int     k, iv, npainted, nblank, ntied, i, j, s, v, n, w, ihighest, ilowest, count;
  double  idummy, imin, p, mx, mn, prange, x, y; //,m;
  double  delta;

  delta   = _SIGMAR;
  p       = ((double)(1./((double)(_K))));
  n       = _WIDTH+1;
  w       = _WIDTH;

  double  rp       [_K];                          // occupancy probability
  double  hslice   [_K];                          // prob cutoff determined by occupancy p (eg 25%)
  int     patchsum [_K];                          // no of patches assigned by the specific resource
  double  percent  [_K];
  //int     done     [_K];                          // to check whether they exceed their occupancy p
  int     newdone  [_K];                          // to check whether they exceed their occupancy p
  double  pm       [(_WIDTH+1)*(_WIDTH+1)][_K+3]; // probability matrix after Midpoint(),
  double  idispvec [(_WIDTH+1)*(_WIDTH+1)];       // one column of pm

  // initializes the landscape and calculates the fractal map with MidPoint() for each resource
  for(k=0; k<_K; k++){
    rp[k] = p;                  // equal probability distribution for each resource
    // set the initial conditions for MidPoint()
#if _RGRADIENT
    if(k == 0){
      X[0][0] = 1.;             // this is southwest
      X[0][w] = 0.;             // this is southeast
      X[w][0] = 0.;             // this is northwest
      X[w][w] = 0.;             // this is northeast
    }
    if(k == 1){
      X[0][0] = 0.;             // this is southwest
      X[0][w] = 1.;             // this is southeast
      X[w][0] = 0.;             // this is northwest
      X[w][w] = 0.;             // this is northeast
    }
    if(k == 2){
      X[0][0] = 0.;             // this is southwest
      X[0][w] = 0.;             // this is southeast
      X[w][0] = 1.;             // this is northwest
      X[w][w] = 0.;             // this is northeast
    }
    if(k == 3){
      X[0][0] = 0.;             // this is southwest
      X[0][w] = 0.;             // this is southeast
      X[w][0] = 0.;             // this is northwest
      X[w][w] = 1.;             // this is northeast
    }
    if(k > 3){
      X[0][0] = delta * normal(&R, 0.,_SIGMAR);   // this is southwest
      X[0][w] = delta * normal(&R, 0.,_SIGMAR);   // this is SUPPOSED TO BE southeast
      X[w][0] = delta * normal(&R, 0.,_SIGMAR);   // this is SUPPOSED TO BE northwest
      X[w][w] = delta * normal(&R, 0.,_SIGMAR);   // this is northeast
    }
#else
    X[0][0] = delta * normal(&R, 0.,_SIGMAR);   // this is southwest
    X[0][w] = delta * normal(&R, 0.,_SIGMAR);   // this is SUPPOSED TO BE southeast
    X[w][0] = delta * normal(&R, 0.,_SIGMAR);   // this is SUPPOSED TO BE northwest
    X[w][w] = delta * normal(&R, 0.,_SIGMAR);   // this is northeast
#endif

    // the fractal map for the resource k is created with this
    // covert to 64x64 before all
    Midpoint();

    // while still in the loop for the r, get max&min to calculate the p range for occupancy cut-off
    mx=mn=0.;
    for(i=0; i<n; i++){
      for(j=0; j<n; j++){
        // printf("%f ", X[i][j]);
        if(X[i][j] > mx){
          // New max; store it
          mx = X[i][j];
        }
        if(X[i][j] < mn){
          // New min; store it
          mn = X[i][j];
        }
      }
      // printf("\n");
    }
    // printf("\n\n\n");

    // calculate the relative probabilities and
    // convert the X[][] for each resource into a single array pm
    iv = 0;
    prange = mx - mn;
    for(i=0; i<n; i++){
      for(j=0; j<n; j++){
        iv = (i*n)+j;
        pm[iv][k]    =  (X[i][j] - mn)/prange;    // this is the relative probability for the resource
        pm[iv][_K]   =  ((double)(i));            // coordinates
        pm[iv][_K+1] =  ((double)(j));            // coordinates
        pm[iv][_K+2] =  0.;                       // no resource has been assigned yet
      }
    }
  }
  // done here with creating the fractal maps and putting them into a single array

  // calculate probability cut-offs for each resource
  for(k=0; k<_K; k++){
    for(i=0; i<(n*n); i++){
      idispvec[i] = pm[i][k];
    }
    s = n*n;
    // printf("\n- - - *%d* \n", s);
    qsort(idispvec, s, sizeof(double), dblCmp);
    // for(i=0; i<(n*n); i++) printf( "%lf ", idispvec[i] );
    v = ((int)(floor(rp[k]*((double)(n*n)))));
    hslice[k]=idispvec[n*n-v];
    //done[k]         = 0;
    patchsum[k]     = 0;
    // printf("\n%f---------------%d, - %d", hslice[k], patchsum[k], done[k]);
  }
  // printf("\n");

  // find the ties, blanks, etc...
  npainted = nblank = ntied = 0;
  for(i=0; i<(n*n); i++){
    idummy    = 0.;
    ihighest  = 0;
    x         = 0.;
    y         = 0.;
    for(k=0; k<_K; k++){
      // before finding the highest probability resource, make sure that it is < occupancy probability
      // if(done[k] == 1) continue;
      // printf("\n%f ---- %f\n", pm[i][k], hslice[k]);
      if(pm[i][k] > hslice[k]){
        // printf("*******ok\n");
        // this is a 'ok' patch, increment the last column by one, tie = (last col>1)
        pm[i][_K+2] += 1.;
        if(pm[i][k] > idummy){
          ihighest  = k;
          idummy    = pm[i][ihighest];
        }
      }
      // printf("\n%f=tie; %d=k; %f=idummy; %f=p; %d=ihighest\n", pm[i][_K+2], k, idummy, pm[i][k], ihighest);
    }
    // printf("\n** done picking %d\n", ihighest);

    // printf("%f\n", pm[i][_K+2]);
    // after going through all the resources for that patch, assign the resource if there is no tie
    if(pm[i][_K+2] == 1.){
      // printf("%f***\n",pm[i][_K+2]);
      // check if patchsum > occupancy
      // this is OK
      // printf("%d is patchsum of %d\n", patchsum[ihighest], ihighest);
      // printf("there is no tie here, highest is %d\n", ihighest);
      if( patchsum[ihighest] >= ((int)(((double)((n-1)*(n-1)))/4.*.50)) ){ 
        //done[ihighest] = 1;
        // printf("\n*(1152) r is %d, total is %d, and done %d\n", ihighest, patchsum[ihighest], done[ihighest]);
      } else {
        //done[ihighest] = 0;
      }
      // if( done[ihighest] == 0 ){
        npainted += 1;
        x = pm[i][_K];
        y = pm[i][_K+1];
        pmtwo[((int)(x))][((int)(y))] = ihighest;   // assign the resource to the 'map'
        patchsum[ihighest]++;                                   // increment the patch sum
        // this following printf is ok...
        // printf("\n\n\n\n**** ok, one resource with high prob. it is %d, total is %d\n", ihighest, patchsum[ihighest]);
      //} else {
        // printf("\n done here? %d, %d\n", ihighest, done[ihighest]);
        // printf("\nthe highest p resourse %d has already exceeded it occupancy limits %d\n", ihighest, patchsum[ihighest]);
      //}
    } else if (pm[i][_K+2] == 0.){
      // count the blank if noone wants this as of now
      nblank += 1;
      // printf("\n\n-_^_^_%d\n\n", nblank);
      // printf("\n this is what is going on with blanks\n %f, %f, %f, %f, %f, %f, %f\n", pm[i][0], pm[i][1], pm[i][2], pm[i][3], pm[i][4], pm[i][5], pm[i][6]);
    } else {
      // count the tie (if not 1, or blank, it is a tie
      ntied += 1;
    }
  }
  // this following printf is ok...
  // printf("\n\n---\n %d painted, %d tied, %d blank\n", npainted, ntied, nblank); 

  // series of sanity checks
  // if(nblank > 0)
  //  printf("***\n\nThere are some blank patches.\n\n");
  if(npainted + ntied > (n*n))
    printf("***\n\n\nSomething's wrong, there are more painted & tied patches than the landscape!!!\n\n");
  // this is to make sure that the resources 'roughly' follow occupancy probabilities
  for(k=0; k<_K; k++){
    // printf("%d---",patchsum[k]);
    if(patchsum[k] > ((int)(((double)((n-1)*(n-1)))/4.*.50)) ){
      //done[k] = 1;
      // printf("\n\n\n\n*(905) r is %d, and total of that r is %d, and done %d\n", ihighest, patchsum[ihighest], done[ihighest]);
      printf("THERE IS A PROBLEM, MORE RESOURCES THAN THEIR OCCUPANCY PROBABILITIES!!!\n");
    //} else {
      // printf("ok\n");
    }
  }
  // printf("\n\n");

  // loop through the landscape again and find blanks and ties
  for(i=0; i<(n*n); i++){
    // first find the blanks and assign them to the highest probability bidder
    idummy    = 0.;
    ihighest  = 0;
    x         = 0.;
    y         = 0.;
    if( pm[i][_K+2] == 0. ){
      // loop through resources to find the highest probability
      for(k=0; k<_K; k++){
        // make sure k < occupancy
        // if(done[k] == 1) continue;
        if(pm[i][k] > idummy){
          ihighest  = k;
          idummy    = pm[i][ihighest];
        }
      }
      // assign the resource
      // if(done[ihighest] == 1) continue;
      x = pm[i][_K];
      y = pm[i][_K+1];
      pmtwo[((int)(x))][((int)(y))]= ihighest;
      // printf("\n\nresolving blanks: %f, %f, %d\n", x, y, ihighest);
      // increment the sum
      patchsum[ihighest]++;
      // check if the k is assigned more than occupancy
      if( patchsum[ihighest] >= ((int)(((double)((n-1)*(n-1)))/4.*.50)) ){ 
        //done[ihighest] = 1;
        // printf("\n\n\n\n*(1227) r is %d, total is %d, and done %d\n", ihighest, patchsum[ihighest], done[ihighest]);
      }
      npainted++;
      // ntied--;
      nblank--;
    }
    // now the tied ones
    idummy    = 0.;
    ihighest  = 0;
    x         = 0.;
    y         = 0.;
    if( pm[i][_K+2] == 2. ){
      for(k=0; k<_K; k++){
        x = pm[i][_K];
        y = pm[i][_K+1];
        // if(done[k] == 1) continue;
        if(pm[i][k] > idummy){
          ihighest  = k;
          idummy    = pm[i][ihighest];
        }
      }
      // assign the resource
      // if(done[ihighest] == 1) continue;
      x = pm[i][_K];
      y = pm[i][_K+1];
      // mark to resolve the tie
      pm[i][_K+2] = 1.;
      pmtwo[((int)(x))][((int)(y))]= ihighest;
      patchsum[ihighest]++;
      // printf("\n\n\n\n**** ok2, one resource with high prob. it is %d, and total of that r is %d\n", ihighest, patchsum[ihighest]);
      // check if the k is assigned more than occupancy
      if(patchsum[ihighest] >= ((int)(((double)((n-1)*(n-1)))/4.*.50)) ){ 
        //done[ihighest] = 1;
        // printf("\n*(1260) r is %d, and total of that r is %d, and done %d\n", ihighest, patchsum[ihighest], done[ihighest]);
      }
      npainted++;
      ntied--;
    }
    idummy    = 0.;
    ihighest  = 0;
    x         = 0.;
    y         = 0.;
    if( pm[i][_K+2] > 1. ){
      for(k=0; k<_K; k++){
        // the following "if" is to make sure
        x = pm[i][_K];
        y = pm[i][_K+1];
        // if(pmtwo[x][y] != 0)
        //   printf("\n\n**************\n\nThere is a tie, but resource is already assigned to the patch. check!\n\n****\n");*/
        // if the resource is already assigned according to the occupanct probability, skip that one 
        // if(done[k] == 1) continue;
        if(pm[i][k] > idummy){
          ihighest  = k;
          idummy    = pm[i][ihighest];
        }
      }
      // assign the resource
      // if(done[ihighest] == 1) continue;
      x = pm[i][_K];
      y = pm[i][_K+1];
      // mark to resolve the tie
      pm[i][_K+2] = 1.;
      pmtwo[((int)(x))][((int)(y))]= ihighest;
      patchsum[ihighest]++;
      // printf("\n\n\n\n**** ok2, one resource with high prob. it is %d, and total of that r is %d\n", ihighest, patchsum[ihighest]);
      // check if the k is assigned more than occupancy
      if(patchsum[ihighest] >= ((int)(((double)((n-1)*(n-1)))/4.*.50)) ){ 
        //done[ihighest] = 1;
        // printf("\n*(974) r is %d, and total of that r is %d, and done %d\n", ihighest, patchsum[ihighest], done[ihighest]);
      }
      npainted++;
      ntied--;
    }
  }

  if( ntied > 0 || nblank > 0 )
    printf("\n ***THERE IS A PROBLEM HERE: %d painted, %d tied, %d blank\n", npainted, ntied, nblank); 

  // the following part to resolve the fractal-tie issue if _K > _k
  if( _K > _k ){
    imin = 0.;
    //m    = 0.;
    for(k=0; k<_K; k++){
      percent[k] = ((double)(patchsum[k]))/((double)(n*n))*100.;
      //m = percent[k];
      // printf("%f-*-%d\n",percent[k], done[k]);
      newdone[k] = 0;
    }

    count = 0;
    for(k=0; k<_K; k++){
      if( percent[k] > 20. ){
        newdone[k] = 1;
        count++;
      }
      // printf("\npercent[%d]: %lf; newdone[%d]: %d", k, percent[k], k, newdone[k]);
    }
    // printf("\n\nno. of r with percent dist> 20 is --%d-- (i.e. a large fractal!)\n", count);

    int round;
    if( count == 1){ for(k=0; k<_K; k++) newdone[k] = 0; }
    if( (count == 0) || (count == 1) ){
      // printf("%d\n", count);
      for( round=0; round<(_K/2); round++){
        imin     = 100.;
        idummy   = 0.;
        ihighest = ilowest = -1;
        for(k=0; k<_K; k++){
          // printf("\nidummy: %f, k: %d, newdone[%d]: %d", idummy, k, k, newdone[k]);
          if( newdone[k] == 0 ){
            // printf("\nnewdone[%d]=0, so idummy: %f, k: %d, newdone[%d]: %d",k, idummy, k, k, newdone[k]);
            if( percent[k] > idummy ){
              ihighest = k;
              idummy   = percent[k];
            }
          }
        }
        newdone[ihighest] = 1;
        // for(k=0; k<_K; k++) printf("\nnewdone[%d]: %d", k, newdone[k]);
        for(k=0; k<_K; k++){
          if( newdone[k] == 1 ) continue;
          if( percent[k] < imin ){
            ilowest = k;
            imin    = percent[k];
          }
        }
        newdone[ilowest]  = 1;
        // for(k=0; k<_K; k++) printf("\nnewdone[%d]: %d", k, newdone[k]);
        // printf("\n round %d: highest is %f (%d), lowest is %f (%d)", round, idummy, ihighest, imin, ilowest);
        for(i=0; i<n; i++){
          for(j=0; j<n; j++){
            if( pmtwo[i][j] == ilowest ) pmtwo[i][j] = ihighest;
          }
        }
        // printf("\n\n %d becomes %d", ilowest, ihighest);
      }
      // for(k=0; k<_K; k++) printf("\npercent [%d]: %lf - newdone[%d]:%d", k, percent[k], k, newdone[k]);
    }

    // if count == 2, that means I can only make 2 rounds to pick the resources...
    if( count == 2 ){
      double news[2];
      double newmin   = 99.;
      double newmax   =  0.;
      int finalhigh   =  0;
      int finallowest =  0;
      // printf("%d\n", count);
      for( round=0; round<((_K-count-2)/2); round++){
        idummy = 0.;
        ihighest = ilowest = 0;
        for(k=0; k<_K; k++){
          if( newdone[k] == 1 ) continue;
          if( percent[k] > idummy ){
            ihighest = k;
            idummy   = percent[k];
          }
          imin     = idummy;
        }
        for(k=0; k<_K; k++){
          if( newdone[k] == 1 ) continue;
          if( percent[k] < imin ){
            ilowest  = k;
            imin     = percent[k];
          }
        }
        // printf("\n round %d: highest is %f (%d), lowest is %f (%d)", round, idummy, ihighest, imin, ilowest);
        newdone[ihighest] = 1;
        newdone[ilowest]  = 1;
        news[round] = idummy + imin;
        if( news[round] > newmax ){
          finalhigh = ihighest;
          newmax    = news[round];
        }
        if( news[round] < newmin ){
          finallowest = ihighest; // counterintuitive but correct, the low will bemade this high
          newmin      = news[round];
        }
        // printf("\n\n%d becomes %d", ilowest, ihighest);
        // printf("\nsum of highest and lowest is %f\n", news[round]);
        for(i=0; i<n; i++){
          for(j=0; j<n; j++){
            if( pmtwo[i][j] == ilowest ) pmtwo[i][j] = ihighest; 
          }
        }
      }
      // printf("\n among sums, new lowest is %f (%d)", newmin, finallowest);
      // printf("\n among sums, new highest is %f (%d)", newmax, finalhigh);

      idummy = 0.;
      ihighest = ilowest = 0;
      for(k=0; k<_K; k++){
        if( newdone[k] == 1 ) continue;
        if( percent[k] > idummy ){
          ihighest = k;
          idummy   = percent[k];
        }
        imin     = idummy;
      }
      for(k=0; k<_K; k++){
        // printf("\n%f - %d", percent[k], newdone[k]);
        if( newdone[k] == 1 ) continue;
        if( percent[k] < imin ){
          ilowest  = k;
          imin     = percent[k];
        }
      }
      // printf("\n LAST round: highest is %f (%d), lowest is %f (%d)", idummy, ihighest, imin, ilowest);
      newdone[ihighest] = 1;
      newdone[ilowest] = 1;
      if( ihighest == ilowest){
        // printf("\nthey are equal (ihighest: %d ilowest: %d)", ihighest, ilowest);
        newdone[ihighest] = 1;
      }
      // printf("\nthey might still be equal (ihighest: %d ilowest: %d) but newdone[%d]=%d", ihighest, ilowest, ihighest, newdone[ihighest]);
      for(k=0; k<_K; k++){
        // printf("\n%f - %d", percent[k], newdone[k]);
        if( newdone[k]==0 )
        ilowest = k;
      }
      // printf("\nilowest: %d", ilowest);
      newdone[ilowest] = 1;
      // printf("\nnow they should be different (ihighest: %d ilowest: %d) but newdone[%d]=%d", ihighest, ilowest, ihighest, newdone[ihighest]);
      // printf("\n CORRECTED LAST round: highest is %f (%d), lowest is %f (%d)\n", idummy, ihighest, imin, ilowest);
      for(i=0; i<n; i++){
        for(j=0; j<n; j++){
          if( pmtwo[i][j] == ilowest )  pmtwo[i][j] = finalhigh;
          if( pmtwo[i][j] == ihighest ) pmtwo[i][j] = finallowest; 
        }
      }
      // printf("\n\n%d becomes %d", ilowest, finalhigh);
      // printf("\n\n%d becomes %d", ihighest, finallowest);
    }

    double newsum = 0.;
    if( count == 3 ){
      // printf("%d\n", count);
      for(k=0; k<_K; k++){
        if( newdone[k] == 1 ){
          // printf("big k: %d - ", k);
        } else {
          newdone[k] = 1;
          newsum += percent[k];
          // printf("small k: %d - ", k);
          for(i=0; i<n; i++){
            for(j=0; j<n; j++){
              if( pmtwo[i][j] == k ) pmtwo[i][j] = 10; 
            }
          }
          // printf("\n\n%d becomes 10\n", k);
        }
      }
      // printf("\nsum of small ones is: %f\n", newsum);
    }

    if( count == 4 ) printf("\n%d - and I really don't know what to do (there are more than 4 r with dist>20percent)!!!\n\n", count);

    for(k=0; k<_K; k++){
      if( newdone[k] == 0 ){
        Error("\n\n----There is problem in picking the mins and maxes (probably a tie with mins and maxes), there are some patches without resources----%d\n\n", k);
      }
    }

  }
  /*
  // this part is left in case i use the minimum requirement again
  // should be some ties left unresolved here...
  printf("\n %d painted, %d tied, %d blank\n", npainted, ntied, nblank); 
  for(k=0; k<_K; k++){
    printf("%f-*-%d\n",((double)(patchsum[k]))/((double)(n*n))*100., done[k]);
  }
  printf("\n\n");
  // redo the ties after they meet the minimum
  // this shouldn't be necessary if there is no minimum
  for(i=0; i<(n*n); i++){
    idummy    = 0.;
    ihighest  = 0;
    x         = 0.;
    y         = 0.;
    if( pm[i][_K+2] > 1. ){
      for(k=0; k<_K; k++){
        // the following "if" is to make sure
        x = pm[i][_K];
        y = pm[i][_K+1];
        // if the resource is already assigned according to the occupanct probability, skip that one 
        if(pm[i][k] > idummy){
          ihighest  = k;
          idummy    = pm[i][ihighest];
        }
      }
      // assign the resource
      x = pm[i][_K];
      y = pm[i][_K+1];
      pmtwo[((int)(x))][((int)(y))]= ((double)(ihighest));
      patchsum[ihighest]++;
      npainted++;
      ntied--;
    }
  }
  */

  int res [4];
  int c;
  count = 0;
  for(k=0; k<20; k++){
    for(i=0; i<n; i++){
      for(j=0; j<n; j++){
        if( pmtwo[i][j] == k ){
          res[count] = k;
          count++;
          k++;
          break;
        }
      }
    }
  }

  /*
  // sanity print
  printf("\n");
  for(i=0; i<n; i++){
    for(j=0; j<n; j++){
      printf("%d ", pmtwo[i][j]);
    }
    printf("\n");
  }
  // printf("\n\n*%d\n", count);
  for(c=0; c<count; c++){
    printf("\n%d ", res[c]);
  }
  printf("\n");
  */
  if( count != 4 ) printf("\nthere is a problem with number of resources, resource count(_k) = %d, not _k in parameters\n", count);

  for(c=0; c<count; c++){
    for(i=0; i<n; i++){
      for(j=0; j<n; j++){
        if( pmtwo[i][j] == res[c] ){
          pmtwo[i][j] = 100 + c;
        }
      }
    }
  }

  for(i=0; i<n; i++){
    for(j=0; j<n; j++){
      pmtwo[i][j] -= 100;
    }
  }

  // sometimes this error is printed, make sure...
  for(i=0; i<n; i++){
    for(j=0; j<n; j++){
      if( pmtwo[i][j] > 3 ){
        printf("\nthere is problem with resource conversions: -%d-\n",k);
      }
    }
  }
}


#if _NEWRESOURCED
void WriteMap(pThreadData td)
{

  int i,j,n;
  n = _WIDTH + 1;

  // if( !(Gen%_NEWRESOURCED) )
    // fprintf(Newrdf, "gen: ");

  // fprintf(Newrdf, "%llu ",  Gen);
  // fprintf(Newrdf, "\n");

  for(i=0; i<n; i++){
    for(j=0; j<n; j++){
      fprintf(Newrdf, "%d ", pmtwo[i][j]);
    }
    // printf("%d, %d, %d \n", k, speciestag[k], tagpopsize[k]);
    fprintf(Newrdf, "\n");
  }
  fprintf(Newrdf, "\n");

  fflush(Newrdf);
  // Incriment counter
  Newrd_logs++;
}
#endif


// Returns a string n that is the same as the input string f, except that:
// %s   Is replaced by the current seed (Seed)
// %g   Is replaced by the requested number of generations (command line arg: NGen)
// %k   Is replaced by the number of characters (_k)
// %K   Is replaced by the maximum carying capacity (_K0)
// I think this is only used to generate a file name based on some 
// parameter/command-line variables.
char *FormatName(char *f)
{
  static char n[2048],b[2048+64];

  for(n[0]=0; *f; f++) {
    if( *f == '%' ) {
      // Special format info follows
      switch(*(++f)) {
      case 's':
        sprintf(b,"%s%llu",n,Seed);
        break;
      case 'g':
        sprintf(b,"%s%llu",n,NGen);
        break;
      case 'k':
        sprintf(b,"%s%d",n,_k);
        break;
      case 'K':
        sprintf(b,"%s%.4lf",n,((double)_K0));
        break;
      default:
        Error("Invalid name/directory format!\n");
      }
    } else {
      // No format info: just copy
      sprintf(b,"%s%c",n,*f);
    }
    // Move b back into n
    memcpy(n,b,sizeof(char)*2048);
    n[2048-1] = '\0';
  }

  // n is static, so it is "safe" to return here
  return n;
}

/***************************************************************************************
 * Functions dealing with the creation and syncronization of threads
 ***************************************************************************************/

// Creates worker threads and deps, and returns when they have initialized themselves
static void CreateThreads()
{

  int i,ids[_CPUS];  pthread_t t;  pthread_attr_t a;

  // Create some condition variables and muticies for thread management
  pthread_cond_init((pthread_cond_t*)&Master,NULL);
  pthread_cond_init((pthread_cond_t*)&Slaves,NULL);
  pthread_mutex_init((pthread_mutex_t*)&Mutex,NULL);

  // Set up the thread attributes
  pthread_attr_init(&a);
  pthread_attr_setdetachstate(&a,PTHREAD_CREATE_DETACHED);
  pthread_attr_setscope(&a,PTHREAD_SCOPE_SYSTEM);

  // Create worker threads
  for(i=0; i<_CPUS; i++) {
    ids[i] = i;
    // !! It would be nice to add the thread affinity code from sched.h here, but 
    // that will only work on 2.5.8 and newer kernels...
    // WorkThreads[i] here takes it back to THREAD in main in threads.c
    if( pthread_create(&t, &a, WorkThreads[i], (void*)(ids+i)) )
      Error("Could not create thread #%d.",i);
  }

  // Wait for them to initialize
  pthread_mutex_lock((pthread_mutex_t*)&Mutex);
  for(i=0; i<_CPUS; i++) {
    while( !Done[i] )
      pthread_cond_wait((pthread_cond_t*)&Master,(pthread_mutex_t*)&Mutex);
    Done[i] = 0;
  }
  pthread_mutex_unlock((pthread_mutex_t*)&Mutex);
}


// Signals threads with flag f, and waits for them to complete
static void SignalWork(int f)
{

  int i;
#if _BENCHMARK
  struct timeval st,et;

  gettimeofday(&st,NULL);
  nProcessed += nCurrentIndividuals();
#endif

  // Signal worker threads and wait for them to finish
  pthread_mutex_lock((pthread_mutex_t*)&Mutex);
  for(i=0; i<_CPUS; i++)
    Go[i] = f;
  pthread_cond_broadcast((pthread_cond_t*)&Slaves);
  for(i=0; i<_CPUS; i++) {
    while( !Done[i] )
      pthread_cond_wait((pthread_cond_t*)&Master,(pthread_mutex_t*)&Mutex);
    Done[i] = 0;
  }
  pthread_mutex_unlock((pthread_mutex_t*)&Mutex);
#if _BENCHMARK
  gettimeofday(&et,NULL);
  ProcessTime += ((et.tv_sec*((u64b)1000000))+et.tv_usec) - ((st.tv_sec*((u64b)1000000))+st.tv_usec);
#endif
}

/***************************************************************************************
 * Functions which compute probibilities, properties, or prefrences of individuals
 ***************************************************************************************/

/*
  Returns the value of the locus l in allele array c in individual i
  Individuals have 4 allele arrays:
  ->x0  // Characteristic alleles 1
  ->x1  // Characteristic alleles 2
  ->y0  // Preference alleles 1
  ->y1  // Preference alleles 2

  w stands for 'which'.  This identifies either:
  ecological traits: w == 0
  preference traits: w == 1
  mating     traits: w == 2
  marker     traits: w == 3
  fpref      traits: w == 4
  neutral    traits: w == 5
*/
static u64b GetLocus(u64b *c, u64b l, int w)
{
  switch(w) {
  case 0:  return (c[l/LPW_E] & (MASK_E<<(_Ae*(l%LPW_E)))) >> (_Ae*(l%LPW_E));
  case 1:  return (c[l/LPW_P] & (MASK_P<<(_Ap*(l%LPW_P)))) >> (_Ap*(l%LPW_P));
  case 2:  return (c[l/LPW_M] & (MASK_M<<(_Am*(l%LPW_M)))) >> (_Am*(l%LPW_M));
  case 3:  return (c[l/LPW_K] & (MASK_K<<(_Ak*(l%LPW_K)))) >> (_Ak*(l%LPW_K));
  case 4:  return (c[l/LPW_F] & (MASK_F<<(_Af*(l%LPW_F)))) >> (_Af*(l%LPW_F));
  case 5:  return (c[l/LPW_N] & (MASK_N<<(_An*(l%LPW_N)))) >> (_An*(l%LPW_N));
  default: return 0;
  }
}

/*
  Sets the locus l in allele array c to the value v
  Individuals have 4 allele arrays:
  ->x0  // Characteristic alleles 1
  ->x1  // Characteristic alleles 2
  ->y0  // Preference alleles 1
  ->y1  // Preference alleles 2

  w stands for 'which'.  This identifies either:
  ecological traits: w == 0
  preference traits: w == 1
  mating     traits: w == 2
  marker     traits: w == 3
  fpref      traits: w == 4
  neutral    traits: w == 5

  To set a specific locus to a specific value, use SetLocus() like so:
  SetLocus(i->x0, 3, 0);
  This will set the third locus in the first characteristic allele array to 0
*/
static void SetLocus(u64b *c, u64b l, u64b v, int w)
{
  // Clear out the individual's current locus and or-in the new locus
  switch(w) {
  case 0:
    c[l/LPW_E] &= ~(MASK_E<<(_Ae*(l%LPW_E)));
    c[l/LPW_E] |= (v<<(_Ae*(l%LPW_E))) & (MASK_E<<(_Ae*(l%LPW_E)));
    break;
  case 1:
    c[l/LPW_P] &= ~(MASK_P<<(_Ap*(l%LPW_P)));
    c[l/LPW_P] |= (v<<(_Ap*(l%LPW_P))) & (MASK_P<<(_Ap*(l%LPW_P)));
    break;
  case 2:
    c[l/LPW_M] &= ~(MASK_M<<(_Am*(l%LPW_M)));
    c[l/LPW_M] |= (v<<(_Am*(l%LPW_M))) & (MASK_M<<(_Am*(l%LPW_M)));
    break;
  case 3:
    c[l/LPW_K] &= ~(MASK_K<<(_Ak*(l%LPW_K)));
    c[l/LPW_K] |= (v<<(_Ak*(l%LPW_K))) & (MASK_K<<(_Ak*(l%LPW_K)));
    break;
  case 4:
    c[l/LPW_F] &= ~(MASK_F<<(_Af*(l%LPW_F)));
    c[l/LPW_F] |= (v<<(_Af*(l%LPW_F))) & (MASK_F<<(_Af*(l%LPW_F)));
    break;
  case 5:
    c[l/LPW_N] &= ~(MASK_N<<(_An*(l%LPW_N)));
    c[l/LPW_N] |= (v<<(_An*(l%LPW_N))) & (MASK_N<<(_An*(l%LPW_N)));
    break;
  }
}


// Degree of matching between individual i and patch p with regard to fitness
static double Qf(pIndividual i, pPatch p)
{
  int k;  double s;

  for(s=0.,k=0; k<_k; k++)
    s += fabs(i->x[k] - p->l[k]);
  return 1.0-(s/_k);
}

// Degree of matching between individual i and patch p with regard to migration
static double Qm(pIndividual i, pPatch p)
{
  int k;  double s;

  for(s=0.,k=0; k<_k; k++){
    s += fabs(i->y[k] - p->l[k]);
    //printf("\nQm(s) is being calculated: %f\n", s);
  }
  //printf("\n\n--------Qm(s) is: %f\n\n", s);
  return 1.0-(s/_k);
}


// Sets *n to the niche most prefered by individual i with respect to fitness
// and sets *m to be the magnitude of that preference
static void PreferedNicheFitness(pIndividual i, u64b *n, double *m)
{
  int j;  double v[NICHES];

  for(j=0,*m = 0.; j<NICHES; j++)
    v[j] = Qf(i,Niches+j);
  Max(v, NICHES, m, n);
}


// Sets *n to the niche most prefered by individual i with respect to migration
// and sets *m to be the magnitude of that preference
static void PreferedNicheMigration(pIndividual i, u64b *n, double *m)
{
  int j;  double v[NICHES];

  for(j=0,*m = 0.; j<NICHES; j++)
    v[j] = Qm(i,Niches+j);
  Max(v, NICHES, m, n);
}


// Returns the Cramer's V value for the current population,
// -1 indicates a "not applicable" value
static double CramersV()
{
  u64b   c,r,min,i,j,k,pf,pm,s[NICHES][NICHES];
  double t,E,X,R[NICHES],C[NICHES];
  pPatch p;

  // Fill in bucket matrix
  memset(s, 0, sizeof(s));
  for(c=0; c<_CPUS; c++){
    for(i=0; i<WIDTH; i++){
      for(j=0; j<HEIGHT; j++) {
        p = &Threads[c]->Current[i][j];
        // Find most prefered niches
        for(k=0; k<p->ni; k++) {
          PreferedNicheFitness  (p->i[k], &pf, &t);
          PreferedNicheMigration(p->i[k], &pm, &t);
          s[pf][pm]++;
        }
      }
    }
  }

  // Create Row and Column counts
  memset(R,0,sizeof(R));
  memset(C,0,sizeof(C));
  for(i=0; i<NICHES; i++){
    for(j=0; j<NICHES; j++){
      R[i] += s[i][j];
      C[j] += s[i][j];
    }
  }

  // Add up non-zero rows and cols
  for(r=c=i=0; i<NICHES; i++) {
    if(R[i]) r++;
    if(C[i]) c++;
  }

  // Compute and return Cramer's V
  for(t=0.,i=0; i<NICHES; i++) {
    if(!R[i]) continue;
    for(j=0; j<NICHES; j++) {
      if(!C[j]) continue;
      E = R[i]*C[j]/nCurrentIndividuals();
      X = s[i][j]- E;
      t += X*X/E;
    }
  }
  if( (min=((c<r)?(c):(r))) > 1 ) {
    return sqrt( t*(((double)1)/(nCurrentIndividuals()*(min-1))) );
  } else {
    if (c==r) return 1;
    else      return 0;
  }
}


/***************************************************************************************
 * Functions which represent the conceptual steps required for the simulation (+ helpers)
 ***************************************************************************************/

/*
// Syncs an ExtinctionEvent's niche value from it's traits
static void SyncExtinctionInfo(pExtinctionInfo e)
{

  u64b k;

  for(e->n=k=0; k<_k; k++){
    e->n |= ((u64b)e->l[k])<<k;
  }
  e->n=log(e->n)/log(2);
}
*/

// Does any application initialization needed before starting the simulation
static void Init()
{

  u64b c,i,j,k,l,res;  char fn[4096],n[2048];  struct timeval tv;  pIndividual t;  FILE *Parf;  void *bk;  double b;

  // Do a few sanity checks
  if( _INIT_INDIVIDUALS > _MAX_INDIVIDUALS )
    Error("_INIT_INDIVIDUALS exceeds _MAX_INDIVIDUALS! Try increaseing _MAX_INDIVIDUALS.\n");
  if( ((double)_WIDTH)/_CPUS  != _WIDTH/_CPUS )
    Error("_WIDTH is not divisible by _CPUS!\n");

  // Use this to track memory useage
  bk = sbrk(0);

  // Allocate a random number generator (and other's) for our main thread
  //ALLOC(Mt->Sigma_s, sizeof(Sigma_s),       "Could not allocate Sigma_s (init)!\n");
  //ALLOC(Mt->Sigma_c, sizeof(Sigma_c),       "Could not allocate Sigma_c (init)!\n");
  //ALLOC(Mt->a_i,     sizeof(a_i),           "Could not allocate a_i     (init)!\n");
  //ALLOC(Mt->log_Sigma_s, _k*sizeof(double), "Could not allocate log_Sigma_s!\n");
  //memcpy(Mt->Sigma_s,Sigma_s,sizeof(Sigma_s));
  //memcpy(Mt->Sigma_c,Sigma_c,sizeof(Sigma_c));
  //memcpy(Mt->a_i,    a_i,    sizeof(a_i));
  // Precompute the log of Sigma_s
  //for(i=0; i<_k; i++)
  //  Mt->log_Sigma_s[i] = log(Mt->Sigma_s[i]);

  // modified so that there are only 100, 010, 001, no 101, etc...
  // Fill in the Niches[] array for use by SyncIndividual() (for Log())
  for(k=0; k<_k; k++){
    //Niches[k].n = k; the line below fixes the problem with k=0 not corresponding to a niche (e.g. pm=0)
    Niches[k].n = k;
    for(i=0; i<_k; i++)
      if( (((u64b)1)<<k)&(((u64b)1)<<i) )     Niches[k].l[i] = 1;
    else                                      Niches[k].l[i] = 0;
  }

  // Set the global seed, and initialize the main thread's random number generator
  if (_RAND_SEED)
    Seed = _RAND_SEED;
  else {
    gettimeofday(&tv,NULL);
    Seed = (u64b)(tv.tv_usec|7);
  }
  initrand(&R, Seed);
  // Just go ahead and print out the seed
  printf("   Seed:           %llu\n",Seed);
#if _NEWRESOURCED
  ProbabilityMap(Threads[0]);
#endif
  // resources are distribute either via fractal algorithm, along blocks or a gradient
  // Fill in the Start[][] array
  ALLOC(Start,_WIDTH*sizeof(Patch*), "Could not allocate starting patch space!\n");
  for(i=0; i<_WIDTH; i++) {
    ZALLOC(Start[i],_HEIGHT*sizeof(Patch), "Could not allocate starting patch space!\n");
    for(j=0; j<_HEIGHT; j++) {
      Start[i][j].lat = i;
      Start[i][j].lon = j;
      /*
      // squares
      if( (i<_WIDTH/2) && (j<_WIDTH/2) ){
        Start[i][j].l[0] = (byte)(1);
        Start[i][j].l[1] = (byte)(0);
        Start[i][j].l[2] = (byte)(0);
        Start[i][j].l[3] = (byte)(0);
      }
      if( (i<_WIDTH/2) && (j>=_WIDTH/2) ){
        Start[i][j].l[0] = (byte)(0);
        Start[i][j].l[1] = (byte)(1);
        Start[i][j].l[2] = (byte)(0);
        Start[i][j].l[3] = (byte)(0);
      }
      if( (i>=_WIDTH/2) && (j<_WIDTH/2) ){
        Start[i][j].l[0] = (byte)(0);
        Start[i][j].l[1] = (byte)(0);
        Start[i][j].l[2] = (byte)(1);
        Start[i][j].l[3] = (byte)(0);
      }
      if( (i>=_WIDTH/2) && (j>=_WIDTH/2) ){
        Start[i][j].l[0] = (byte)(0);
        Start[i][j].l[1] = (byte)(0);
        Start[i][j].l[2] = (byte)(0);
        Start[i][j].l[3] = (byte)(1);
      }
      */
#if _NEWRESOURCED
      res = pmtwo[i][j];
      // printf("%d ", res);
      // Start[i][j].n = k;
      if( res == 0 ){
        Start[i][j].l[0] = (byte)(1);
        Start[i][j].l[1] = (byte)(0);
        Start[i][j].l[2] = (byte)(0);
        Start[i][j].l[3] = (byte)(0);
      }
      if( res == 1){
        Start[i][j].l[0] = (byte)(0);
        Start[i][j].l[1] = (byte)(1);
        Start[i][j].l[2] = (byte)(0);
        Start[i][j].l[3] = (byte)(0);
      }
      if( res == 2 ){
        Start[i][j].l[0] = (byte)(0);
        Start[i][j].l[1] = (byte)(0);
        Start[i][j].l[2] = (byte)(1);
        Start[i][j].l[3] = (byte)(0);
      }
      if( res == 3 ){
        Start[i][j].l[0] = (byte)(0);
        Start[i][j].l[1] = (byte)(0);
        Start[i][j].l[2] = (byte)(0);
        Start[i][j].l[3] = (byte)(1);
      }
#endif

      SyncPatch(&Start[i][j]);

      if( (i<1) && (j<1) )  Start[i][j].ni = _INIT_INDIVIDUALS;
      else                  Start[i][j].ni = 0;

      if(Start[i][j].ni) {
        ZALLOC(t, _INIT_INDIVIDUALS*sizeof(Individual), "Could not allocate starting individuals!\n");
        for(k=0; k<Start[i][j].ni; k++) {
          LinkIndividual(Start[i][j].i[k] = t+k);
          // Give this individual it's sex and other traits

#if _DISTINCT_SEXES
          (t+k)->s = rnd(&R, 2);
#endif


#if _SPECIALIST
          for(l=0; l<_k; l++){
            if( l==0 ){
              for(c=0; c<_Le/2; c++)   SetLocus((t+k)->x0, l*_Le+c,(u64b)((((((u64b)1) <<_Ae)-1)/2.)+.5),0);
              for(c=_Le/2; c<_Le; c++) SetLocus((t+k)->x0, l*_Le+c,(u64b)((((((u64b)1)<<_Ae)-1)/2.)+.5), 0);
              for(c=0; c<_Le/2; c++)   SetLocus((t+k)->x1, l*_Le+c,(u64b)((((((u64b)1) <<_Ae)-1)/2.)+.5),0);
              for(c=_Le/2; c<_Le; c++) SetLocus((t+k)->x1, l*_Le+c,(u64b)((((((u64b)1)<<_Ae)-1)/2.)+.5), 0);
            } else {
              for(c=0; c<_Le/2; c++)   SetLocus((t+k)->x0, l*_Le+c,(u64b)(((((u64b)1) <<_Ae)-1)/2.),     0);
              for(c=_Le/2; c<_Le; c++) SetLocus((t+k)->x0, l*_Le+c,(u64b)(((((u64b)1)<<_Ae)-1)/2.),      0);
              for(c=0; c<_Le/2; c++)   SetLocus((t+k)->x1, l*_Le+c,(u64b)(((((u64b)1) <<_Ae)-1)/2.),     0);
              for(c=_Le/2; c<_Le; c++) SetLocus((t+k)->x1, l*_Le+c,(u64b)(((((u64b)1)<<_Ae)-1)/2.),      0);
            }

            if( l==0 ){
              for(c=0; c<_Lp/2; c++)   SetLocus((t+k)->y0, l*_Lp+c,(u64b)((((((u64b)1) <<_Ap)-1)/2.)+.5),0);
              for(c=_Lp/2; c<_Lp; c++) SetLocus((t+k)->y0, l*_Lp+c,(u64b)((((((u64b)1)<<_Ap)-1)/2.)+.5), 0);
              for(c=0; c<_Le/2; c++)   SetLocus((t+k)->y1, l*_Lp+c,(u64b)((((((u64b)1) <<_Ap)-1)/2.)+.5),0);
              for(c=_Lp/2; c<_Lp; c++) SetLocus((t+k)->y1, l*_Lp+c,(u64b)((((((u64b)1)<<_Ap)-1)/2.)+.5), 0);
            } else {
              for(c=0; c<_Lp/2; c++)   SetLocus((t+k)->y0, l*_Lp+c,(u64b)(((((u64b)1) <<_Ap)-1)/2.),     0);
              for(c=_Lp/2; c<_Lp; c++) SetLocus((t+k)->y0, l*_Lp+c,(u64b)(((((u64b)1)<<_Ap)-1)/2.),      0);
              for(c=0; c<_Lp/2; c++)   SetLocus((t+k)->y1, l*_Lp+c,(u64b)(((((u64b)1) <<_Ap)-1)/2.),     0);
              for(c=_Lp/2; c<_Lp; c++) SetLocus((t+k)->y1, l*_Lp+c,(u64b)(((((u64b)1)<<_Ap)-1)/2.),      0);
            }
          }

#else
          for(l=0; l<T_LENGTH; l++)
            Start[i][j].i[k]->d[l] = 0;
#endif

#if _DISTINCT_SEXES
          for(c=0; c<_Lm/2; c++)   SetLocus((t+k)->m0, c,(u64b)(((((u64b)1) <<_Am)-1)/2.),     2);
          for(c=_Lm/2; c<_Lm; c++) SetLocus((t+k)->m0, c,(u64b)((((((u64b)1)<<_Am)-1)/2.)+.5), 2);
          for(c=0; c<_Lm/2; c++)   SetLocus((t+k)->m1, c,(u64b)(((((u64b)1) <<_Am)-1)/2.),     2);
          for(c=_Lm/2; c<_Lm; c++) SetLocus((t+k)->m1, c,(u64b)((((((u64b)1)<<_Am)-1)/2.)+.5), 2);
          for(c=0; c<_Lk/2; c++)   SetLocus((t+k)->k0, c,(u64b)(((((u64b)1) <<_Ak)-1)/2.),     3);
          for(c=_Lk/2; c<_Lk; c++) SetLocus((t+k)->k0, c,(u64b)((((((u64b)1)<<_Ak)-1)/2.)+.5), 3);
          for(c=0; c<_Lk/2; c++)   SetLocus((t+k)->k1, c,(u64b)(((((u64b)1) <<_Ak)-1)/2.),     3);
          for(c=_Lk/2; c<_Lk; c++) SetLocus((t+k)->k1, c,(u64b)((((((u64b)1)<<_Ak)-1)/2.)+.5), 3);
#if _SEXUAL_SELECTION
          for(c=0; c<_Lf/2; c++)   SetLocus((t+k)->f0, c,(u64b)(((((u64b)1) <<_Af)-1)/2.),     4);
          for(c=_Lf/2; c<_Lf; c++) SetLocus((t+k)->f0, c,(u64b)((((((u64b)1)<<_Af)-1)/2.)+.5), 4);
          for(c=0; c<_Lf/2; c++)   SetLocus((t+k)->f1, c,(u64b)(((((u64b)1) <<_Af)-1)/2.),     4);
          for(c=_Lf/2; c<_Lf; c++) SetLocus((t+k)->f1, c,(u64b)((((((u64b)1)<<_Af)-1)/2.)+.5), 4);
#endif
#endif
#if _Ln
          for(c=0; c<_Ln/2; c++)   SetLocus((t+k)->z0, c,(u64b)(((((u64b)1) <<_An)-1)/2.),     5);
          for(c=_Ln/2; c<_Ln; c++) SetLocus((t+k)->z0, c,(u64b)((((((u64b)1)<<_An)-1)/2.)+.5), 5);
          for(c=0; c<_Ln/2; c++)   SetLocus((t+k)->z1, c,(u64b)(((((u64b)1) <<_An)-1)/2.),     5);
          for(c=_Ln/2; c<_Ln; c++) SetLocus((t+k)->z1, c,(u64b)((((((u64b)1)<<_An)-1)/2.)+.5), 5);
#endif
        }
      }
    }
  }

  /*
  // sanity print
  for(i=0; i<_WIDTH; i++){
    for(j=0; j<_HEIGHT; j++){
      printf("%d ", Start[i][j].n);
    }
    printf("\n");
  }
  */

  // Create worker threads and dependencies and wait for them to initialize
  CreateThreads();

  // Start[][] is not needed anymore, go ahead and free it
  for(i=0; i<_WIDTH; i++) {
    for(j=0; j<_HEIGHT; j++) {
      //if(Start[i][j].ei)
      //  free(Start[i][j].ei);
      free(*Start[i][j].i);
    }
    free(Start[i]);
  }
  free(Start);

  // Parse our file/dir name format strings from the parameters file
  n[0] = 0;
  sprintf(n,"%s",FormatName(_FILE_NAME));

  // Copy parameters file and append Seed there
  sprintf(fn,"cp parameters.h %s.par",n);
  system(fn);
  sprintf(fn,"%s.par",n);
  if( !(Parf=fopen(fn,"a")) ) 
    Error("Could not open file \"%s\"!\n",fn);
  fprintf(Parf,"\n//Actual Seed:           %llu\n",Seed);
  fclose(Parf);

  // Compute a value for _b which will allow view to scale properly
#if _CHILD_DISPERSAL
  //b = 1.0;  //original line
  b = _b/2.;
#else
#if _DISTINCT_SEXES
  b = _b/2.;
#else
  b = _b;
#endif
#endif

#if _SYS
  // Open sys file
  sprintf(fn,"%s.sys",n);
  if( !(Sysf=fopen(fn,"w")) ) 
    Error("Could not open file \"%s\"!\n",fn);
  // Write the header
  fprintf(Sysf,"%d %lf %lf %llu %llu\n",_k,((double)_K0),b,(u64b)(_WIDTH),(u64b)(_HEIGHT));
#endif
#if _DEME
  // Open deme file
  sprintf(fn,"%s.deme",n);
  if( !(Demef=fopen(fn,"w")) ) 
    Error("Could not open file \"%s\"!\n",fn);
  // Write the header
  fprintf(Demef,"%d %lf %lf %llu %llu\n",_k,((double)_K0),b,(u64b)(_WIDTH),(u64b)(_HEIGHT));
#endif
#if _HABITAT
  // Open habitat file
  sprintf(fn,"%s.habitat",n);
  if( !(Habitatf=fopen(fn,"w")) ) 
    Error("Could not open file \"%s\"!\n",fn);
#endif
#if _RANGE
  // Open range file
  sprintf(fn,"%s.range",n);
  if( !(Rangef=fopen(fn,"w")) ) 
    Error("Could not open file \"%s\"!\n",fn);
  // Write the header
  fprintf(Rangef,"%d %lf %lf %llu %llu\n",_k,((double)_K0),b,(u64b)(_WIDTH),(u64b)(_HEIGHT));
#endif
#if _HISTO
  // Open histo file
  sprintf(fn,"%s.histo",n);
  if( !(Histof=fopen(fn,"w")) ) 
    Error("Could not open file \"%s\"!\n",fn);
  // Write the header
  fprintf(Histof,"%d %lf %lf %llu %llu\n",_k,((double)_K0),b,(u64b)(_WIDTH),(u64b)(_HEIGHT));
#endif
#if _XVSS
  // Open xvss file
  sprintf(fn,"%s.xvss",n);
  if( !(Xvssf=fopen(fn,"w")) ) 
    Error("Could not open file \"%s\"!\n",fn);
#endif
#if _TRANS
  // Open file
  sprintf(fn,"%s.trans",n);
  if( !(Transf=fopen(fn,"w")) ) 
    Error("Could not open file \"%s\"!\n",fn);
  // Write the header
  //fprintf(Transf,"%d %lf %lf %llu %llu\n",_k,((double)_K0),b,(u64b)(_WIDTH),(u64b)(_HEIGHT));
#endif
#if _NEWRESOURCED
  // Open Newrd file
  sprintf(fn,"%s.newrd",n);
  if( !(Newrdf=fopen(fn,"w")) ) 
    Error("Could not open file \"%s\"!\n",fn);
  // Write the header
  //fprintf(Newrdf,"%d %lf %lf %llu %llu\n",_k,((double)_K0),b,(u64b)(_WIDTH),(u64b)(_HEIGHT));
#endif
#if _IND
  // Open individual file
  sprintf(fn,"%s.ind",n);
  if( !(Indf=fopen(fn,"w")) ) 
    Error("Could not open file \"%s\"!\n",fn);
  // Write the header
  // fprintf(Indf,"%d %lf %lf %llu %llu\n",_k,((double)_K0),b,(u64b)(_WIDTH),(u64b)(_HEIGHT));
#endif

  // How much memory are we using now?  
  printf("   Mem:            %lluK\n",(((u64b)sbrk(0))-((u64b)bk))/(1<<10));
}

#if (_SYS || _DEME )
// Writes system level data to a ".sys" and a ".deme" file
static void LogSysDeme()
{

  u64b   c,i,j,k,l,pf,pm,max_i,sys=1;
  u64b   s[NICHES][NICHES],mvk[MVK_X][MVK_Y],newsm[2<<_k][2<<_k],spvy[2<<_k][SPVY_Y],newrm[4*_k][4];
  double gcc,gcs,ipf,ipm,match_pref[2],match_cur[2],pref[2][NICHES],count[NICHES];
  double cc=0.,match[2],X,Y,x[_k],y[_k],x2[_k],y2[_k],f[_k],t=0.,mt=0.,kt=0.,ft=0.;
  double pr=0.,gpr=0.; // average preference per deme and per system
  double amc, ymax=0.;
  pPatch p;
  int    counts = 0, total = 0, lat, lon, m;

  // counts 'sexual species' per 'ecological species' rather than per niche as in kpn
  u64b   spn[2<<_k][MVK_Y];
  u64b   sfpn[2<<_k][MVK_Y];
  // Data only computed when in sexual mode
#if _DISTINCT_SEXES
  u64b   kpn[NICHES][MVK_Y];
#if _SEXUAL_SELECTION
  u64b   fpn[NICHES][MVK_Y];
#endif
#endif

  // Every generation in the log files is now preceded with the tag 'gen: '.
  // This should help to solve some of the problems with data file "skewing".
#if _SYS
  if( !(Gen%_SYS ) )
    fprintf(Sysf, "gen: ");
#endif
#if _DEME
  if( !(Gen%_DEME) )
    fprintf(Demef, "gen: ");
#endif

#if _SYS
  if( !(Gen%_SYS ) ){
    /* If there are no surviving individuals, just print a header and move on */
    if(!nCurrentIndividuals()) {
      fprintf(Sysf, "%llu 0\n", Gen);
      Sys_logs++;
      sys = 0;
    } else {
      // Init generation level vars
      amc = 0.0;
      memset(s,                 0, sizeof(s));
      memset(newsm,             0, sizeof(newsm));
      memset(spn,               0, sizeof(spn));
      memset(sfpn,              0, sizeof(sfpn));
#if _DISTINCT_SEXES
      memset(kpn,               0, sizeof(kpn));
#if _SEXUAL_SELECTION
      memset(fpn,               0, sizeof(fpn));
#endif
#if _MVK
      memset(mvk,               0, sizeof(mvk));
#endif
      memset(spvy,              0, sizeof(spvy));
#if _NEWRM
      memset(newrm,             0, sizeof(newrm));
#endif
#endif
      memset(count,             0, sizeof(count));
      memset(match_cur,         0, sizeof(match_cur));
      memset(match_pref,        0, sizeof(match_pref));
      memset(pref,              0, sizeof(pref));
    }
  }
#endif

  // this is to scale preference trait with max for figures
  for(lat=0; lat<WIDTH; lat++){
    for(lon=0; lon<HEIGHT; lon++){
      p = &Threads[0]->Current[lat][lon];
      for(k=0; k<p->ni; k++){
        for(l=0; l<_k; l++){
          if( p->i[k]->y[l] > ymax ){
            ymax = p->i[k]->y[l];
          }
        }
      }
    }
  }
  //printf("\n%f ", ymax);

  //--------------------------------------------------------------------------------------------------
  //--------------------------------------------------------------------------------------------------
  // this part is new, and probably a very inefficient way to determine the species in the patch
  // be careful, assumes _k=4 only!!!

  //int ttt;
  int pall;
  // mean for all x traits
  double meanx[8], sumx[8], varx[8], sdx[8];
  int nox[8];
  for(m=0; m<8; m++){
    meanx[m] = 0.;
    sumx[m]  = 0.;
    varx[m]  = 0.;
    sdx[m]   = 0.;
    nox[m]   = 0;
  }

  for(lat=0; lat<WIDTH; lat++){
    for(lon=0; lon<HEIGHT; lon++){
      p = &Threads[0]->Current[lat][lon];
      for(pall=0; pall<16; pall++){
        p->AllSpecies[pall] = 0;
        p->speciespops[pall] = 0;
      }
      int rr  = 0;
      int pop = 0;
      // loop through individuals
      for(k=0; k<p->ni; k++){
        // calculates the sum of all x traits (2* because <>0.5) for mean and variance
        for(m=0; m<_k; m++){
          if( p->i[k]->x[m] < 0.5){
            sumx[2*m] += p->i[k]->x[m];
            nox[2*m]++;
          } else {
            sumx[2*m+1] += p->i[k]->x[m];
            nox[2*m+1]++;
          }
        }
        // increment the speciespops to find the most abundant species
        if( (p->i[k]->x[0] >= 0.8) && (p->i[k]->x[1] < 0.5) && (p->i[k]->x[2] < 0.5) && (p->i[k]->x[3] < 0.5) ){
          p->AllSpecies[1] = 1;
          p->speciespops[1]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n1(%d): %d", p->AllSpecies[1], p->speciespops[1]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[1][((l*_k) + rr)]++;
          }
          spn[1][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[1][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[1] >= 0.8) && (p->i[k]->x[0] < 0.5) && (p->i[k]->x[2] < 0.5) && (p->i[k]->x[3] < 0.5) ){
          p->AllSpecies[2] = 1;
          p->speciespops[2]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n2(%d): %d", p->AllSpecies[2], p->speciespops[2]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[2][((l*_k) + rr)]++;
          }
          spn[2][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[2][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[2] >= 0.8) && (p->i[k]->x[1] < 0.5) && (p->i[k]->x[0] < 0.5) && (p->i[k]->x[3] < 0.5) ){
          p->AllSpecies[4] = 1;
          p->speciespops[4]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n4(%d): %d", p->AllSpecies[4], p->speciespops[4]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[4][((l*_k) + rr)]++;
          }
          spn[4][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[4][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[3] >= 0.8) && (p->i[k]->x[1] < 0.5) && (p->i[k]->x[2] < 0.5) && (p->i[k]->x[0] < 0.5) ){
          p->AllSpecies[8] = 1;
          p->speciespops[8]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n8(%d): %d", p->AllSpecies[8], p->speciespops[8]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[8][((l*_k) + rr)]++;
          }
          spn[8][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[8][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[2] < 0.5) && (p->i[k]->x[3] < 0.5) && (((p->i[k]->x[0] >= 0.8) && (p->i[k]->x[1] >= 0.5)) || ((p->i[k]->x[1] >= 0.8) && (p->i[k]->x[0] >= 0.5)) ) ){
          p->AllSpecies[3] = 1;
          p->speciespops[3]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n3(%d): %d", p->AllSpecies[3], p->speciespops[3]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[3][((l*_k) + rr)]++;
          }
          spn[3][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[3][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[1] < 0.5) && (p->i[k]->x[3] < 0.5) && (((p->i[k]->x[0] >= 0.8) && (p->i[k]->x[2] >= 0.5)) || ((p->i[k]->x[2] >= 0.8) && (p->i[k]->x[0] >= 0.5)) ) ){
          p->AllSpecies[5] = 1;
          p->speciespops[5]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n5(%d): %d", p->AllSpecies[5], p->speciespops[5]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[5][((l*_k) + rr)]++;
          }
          spn[5][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[5][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[0] < 0.5) && (p->i[k]->x[3] < 0.5) && (((p->i[k]->x[1] >= 0.8) && (p->i[k]->x[2] >= 0.5)) || ((p->i[k]->x[2] >= 0.8) && (p->i[k]->x[1] >= 0.5)) ) ){
          p->AllSpecies[6] = 1;
          p->speciespops[6]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n6(%d): %d", p->AllSpecies[6], p->speciespops[6]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[6][((l*_k) + rr)]++;
          }
          spn[6][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[6][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[1] < 0.5) && (p->i[k]->x[2] < 0.5) && (((p->i[k]->x[0] >= 0.8) && (p->i[k]->x[3] >= 0.5)) || ((p->i[k]->x[3] >= 0.8) && (p->i[k]->x[0] >= 0.5)) ) ){
          p->AllSpecies[9] = 1;
          p->speciespops[9]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n9(%d): %d", p->AllSpecies[9], p->speciespops[9]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[9][((l*_k) + rr)]++;
          }
          spn[9][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[9][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[0] < 0.5) && (p->i[k]->x[2] < 0.5) && (((p->i[k]->x[1] >= 0.8) && (p->i[k]->x[3] >= 0.5)) || ((p->i[k]->x[3] >= 0.8) && (p->i[k]->x[1] >= 0.5)) ) ){
          p->AllSpecies[10] = 1;
          p->speciespops[10]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n10(%d): %d", p->AllSpecies[10], p->speciespops[10]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[10][((l*_k) + rr)]++;
          }
          spn[10][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[10][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[0] < 0.5) && (p->i[k]->x[1] < 0.5) && (((p->i[k]->x[2] >= 0.8) && (p->i[k]->x[3] >= 0.5)) || ((p->i[k]->x[3] >= 0.8) && (p->i[k]->x[2] >= 0.5)) ) ){
          p->AllSpecies[12] = 1;
          p->speciespops[12]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n12(%d): %d", p->AllSpecies[12], p->speciespops[12]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[12][((l*_k) + rr)]++;
          }
          spn[12][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[12][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[3] < 0.5) && (p->i[k]->x[0] >= 0.5) && (p->i[k]->x[1] >= 0.5) && (p->i[k]->x[2] >= 0.5) && ((p->i[k]->x[0] >= 0.8) || (p->i[k]->x[1] >= 0.8) || (p->i[k]->x[2] >= 0.8)) ){
          p->AllSpecies[7] = 1;
          p->speciespops[7]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n7(%d): %d", p->AllSpecies[7], p->speciespops[7]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[7][((l*_k) + rr)]++;
          }
          spn[7][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[7][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[2] < 0.5) && (p->i[k]->x[0] >= 0.5) && (p->i[k]->x[1] >= 0.5) && (p->i[k]->x[3] >= 0.5) && ((p->i[k]->x[0] >= 0.8) || (p->i[k]->x[1] >= 0.8) || (p->i[k]->x[3] >= 0.8)) ){
          p->AllSpecies[11] = 1;
          p->speciespops[11]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n11(%d): %d", p->AllSpecies[11], p->speciespops[11]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[11][((l*_k) + rr)]++;
          }
          spn[11][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[11][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[1] < 0.5) && (p->i[k]->x[0] >= 0.5) && (p->i[k]->x[2] >= 0.5) && (p->i[k]->x[3] >= 0.5) && ((p->i[k]->x[0] >= 0.8) || (p->i[k]->x[2] >= 0.8) || (p->i[k]->x[3] >= 0.8)) ){
          p->AllSpecies[13] = 1;
          p->speciespops[13]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n13(%d): %d", p->AllSpecies[13], p->speciespops[13]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[13][((l*_k) + rr)]++;
          }
          spn[13][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[13][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[0] < 0.5) && (p->i[k]->x[1] >= 0.5) && (p->i[k]->x[2] >= 0.5) && (p->i[k]->x[3] >= 0.5) && ((p->i[k]->x[1] >= 0.8) || (p->i[k]->x[2] >= 0.8) || (p->i[k]->x[3] >= 0.8)) ){
          p->AllSpecies[14] = 1;
          p->speciespops[14]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n14(%d): %d", p->AllSpecies[14], p->speciespops[14]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[14][((l*_k) + rr)]++;
          }
          spn[14][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[14][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
        if( (p->i[k]->x[0] >= 0.5) && (p->i[k]->x[1] >= 0.5) && (p->i[k]->x[2] >= 0.5) && (p->i[k]->x[3] >= 0.5) && ((p->i[k]->x[0] >= 0.8) || (p->i[k]->x[1] >= 0.8) || (p->i[k]->x[2] >= 0.8) || (p->i[k]->x[3] >= 0.8)) ){
          p->AllSpecies[15] = 1;
          p->speciespops[15]++;
          pop++;
          // for(ttt=0; ttt<_k; ttt++) printf("%lf ", p->i[k]->x[ttt]);
          // printf("\n15(%d): %d", p->AllSpecies[15], p->speciespops[15]);
          for(l=0; l<_k; l++){
            if( p->i[k]->y[l]/ymax < 0.25 )  rr = 0;
            if( p->i[k]->y[l]/ymax >= 0.25 && p->i[k]->y[l] < 0.5 )  rr = 1;
            if( p->i[k]->y[l]/ymax >= 0.5  && p->i[k]->y[l] < 0.75 ) rr = 2;
            if( p->i[k]->y[l]/ymax >= 0.75 ) rr = 3;
            spvy[15][((l*_k) + rr)]++;
          }
          spn[15][(int)(p->i[k]->k*(MVK_Y-1))]++;
          sfpn[15][(int)(p->i[k]->f*(MVK_Y-1))]++;
          continue;
        }
      }

      if( pop > p->ni ) printf("\nno. of inds assigned to a species is: %d, but N in patch is: %llu", pop, p->ni);

      if( pop == 0 ){
        p->SpeciesID = 0;
      } else {
        // set the species id of the patch to the most abundant species
        int m = 0;
        u64b wh;
        Maxint(p->speciespops, 16, &m, &wh);
        p->SpeciesID = wh;
        // printf("\n(%d, %d) species id is set to: %d (%d/%d)", lat, lon, wh, p->speciespops[wh], p->ni);
      }
    }
  }

  // the following calculates the mean x trait
  for(m=0; m<8; m++){
    if( nox[m] == 0 ){
      meanx[m] = 0.;
      //printf("%lf-", meanx[m]);
      continue;
    } else {
      meanx[m] = sumx[m]/nox[m];
      //printf("%lf-", meanx[m]);
    }
  }
  //printf("\n");

  for(lat=0; lat<WIDTH; lat++){
    for(lon=0; lon<HEIGHT; lon++){
      p = &Threads[0]->Current[lat][lon];
      for(k=0; k<p->ni; k++){
        for(m=0; m<_k; m++){
          if( p->i[k]->x[m] < 0.5){
            varx[2*m] += fabs(p->i[k]->x[m]-meanx[2*m]) * fabs(p->i[k]->x[m]-meanx[2*m]);
          } else {
            varx[2*m+1] += fabs(p->i[k]->x[m]-meanx[2*m+1]) * fabs(p->i[k]->x[m]-meanx[2*m+1]);
          }
        }
      }
    }
  }
  for(m=0; m<8; m++){
    sdx[m] = sqrt(varx[m]);
    //printf("%lf-", sdx[m]);
  }

#if _XVSS
  //if( !(Gen%_XVSS ) )
  //  fprintf(Xvssf, "gen: ");
  fprintf(Xvssf, "%llu ", Gen);
  for(m=0; m<8; m++){
    fprintf(Xvssf, "%lf ", meanx[m]);
    fprintf(Xvssf, "%lf ", sdx[m]);
  }
  fprintf(Xvssf, "\n");
  fflush(Xvssf);
  // Increment counter
  Xvss_logs++;
#endif

  //---------------------------------------------

  for(gpr=gcc=gcs=0.,max_i=c=0; c<_CPUS; c++){
    for(i=0; i<WIDTH; i++){
      for(j=0; j<HEIGHT; j++){
        p = &Threads[c]->Current[i][j];

#if _DEME
        // Init deme-level vars
        if( !(Gen%_DEME) ) {
          cc = 0.;
          pr = 0.;
          memset(x,     0, sizeof(x));
          memset(y,     0, sizeof(y));
          memset(x2,    0, sizeof(x2));
          memset(y2,    0, sizeof(y2));
#if _DISTINCT_SEXES
          mt = kt = 0.0;
#if _SEXUAL_SELECTION
          ft = 0.0;
#endif
#endif
          memset(f,     0, sizeof(f));
          memset(match, 0, sizeof(match));
        }
#endif
#if _SYS
        if( !(Gen%_SYS) && sys ) {
          // Find max individual count per patch
          if( p->ni > max_i )
            max_i = p->ni;
          // Sum niche counts
          count[p->n] += p->ni;
        }
#endif
        for(k=0; k<p->ni; k++) {
#if _DEME
          if( !(Gen%_DEME) ){
            // Sum for most prefered niche
            // CHANGE THIS!!!
            //PreferedNicheFitness(p->i[k], &l, &t);
            PreferedNicheMigration(p->i[k], &l, &t);
            f[l]++;    // counts the number of individuals using which of the niches in Niche[]
            // Sum over all fitness (carrying capacities) and preference
            cc += K(p->i[k],p);
            pr += P(p->i[k],p);
            // Sum over all matching fit/mig
            // NOT relevant
            match[0] += Qf(p->i[k],p);
            match[1] += Qm(p->i[k],p);
            // Sum over all traits
            for(l=0; l<_k; l++) {
              X = p->i[k]->x[l];
              Y = p->i[k]->y[l];
              x[l] += X;
              y[l] += Y;
              x2[l] += X*X;
              y2[l] += Y*Y;
            }
#if _DISTINCT_SEXES
            amc += fabs(p->i[k]->m);
            mt  += p->i[k]->m;
            kt  += p->i[k]->k;
#if _SEXUAL_SELECTION
            ft  += p->i[k]->f;
#endif
#endif
          }
#endif

#if _SYS
          if( !(Gen%_SYS) && sys ) {
            // Compute fitness (carrying capacity) and preference
            gcc += K(p->i[k],p);
            gpr += P(p->i[k],p);
            // printf("\n k: %lld", k);
            // Find most prefered niches fit/mig + cur/pref
            // CHANGE THIS!!!
            match_cur[0] += Qf(p->i[k],p);
            match_cur[1] += Qm(p->i[k],p);
            // CHANGE THIS!!!
            PreferedNicheFitness  (p->i[k], &pf, &ipf);
            PreferedNicheMigration(p->i[k], &pm, &ipm);
            //printf("\npf %llx, &pf %llx, ipf %f, &ipf %f", pf, &pf, ipf, &ipf);
            //printf("\npm %llx, &pm %llx, ipm %f, &ipm %f", pm, &pm, ipm, &ipm);
            match_pref[0] += ipf;
            match_pref[1] += ipm;
            // Compute niche preference matrix and histogram markers
            // CHANGE THIS!!!
            pref[0][pf]++;     // counts the no of individuals it is 2xk
            pref[1][pm]++;
            s[pf][pm]++;
            //printf("\n -- s: %llu", s[pf][pm]++);
#if _DISTINCT_SEXES
#if _MAGIC_TRAIT
            // Sum for kpn
            kpn[p->n][(int)(p->i[k]->x[0]*(X0_Y-1))]++;
#else
            // Sum for kpn
            kpn[p->n][(int)(p->i[k]->k*(MVK_Y-1))]++;
#endif
#if _SEXUAL_SELECTION
            // Sum for fpn
            fpn[p->n][(int)(p->i[k]->f*(MVK_Y-1))]++;
#endif
#if _MVK
#if _MAGIC_TRAIT
            // Build Mating vs. Marker matrix
            mvk[(int)(((p->i[k]->m+1)/2)*(MVK_X-1))][(int)(p->i[k]->x[0]*(X0_Y-1))]++;
#else

            // Build Mating vs. Marker matrix
            mvk[(int)(((p->i[k]->m+1)/2)*(MVK_X-1))][(int)(p->i[k]->k*(MVK_Y-1))]++;
#endif
#endif
#endif
          }
#endif
        }

#if _DEME
        if( !(Gen%_DEME) ) {
          // Find most prefered niche
          if ( (k = p->ni) ){ t = 0; Max(f, NICHES, &t, &l); t = l; }
          // Divide by nIndividuals for average and find variances
          k        = p->ni;
          cc      /= k;
          pr      /= k;
          match[0]/= k;
          match[1]/= k;
          for(l=0; l<_k; l++) {
            x[l]  /= k;
            y[l]  /= k;
            x2[l]  = x2[l]/k - x[l]*x[l];
            y2[l]  = y2[l]/k - y[l]*y[l];
          }
#if _DISTINCT_SEXES
          mt /= k;
          kt /= k;
#if _SEXUAL_SELECTION
          ft /= k;
#endif
#endif
#endif

#if _DEME
          // Write this patch's stats
          fprintf(Demef, "%llu %llu %llu %lf %lf %lf %lf %llu ",
                  Gen, p->ni, ((u64b)t), cc, pr, match[0], match[1], p->n);
          // Write niche type
          for(l=0; l<_k; l++) fprintf(Demef, "%d ", p->l[l]);
          // Write average traits and variances
          for(l=0; l<_k; l++) fprintf(Demef, "%lf ", x[l]);
          for(l=0; l<_k; l++) fprintf(Demef, "%lf ", y[l]);
          for(l=0; l<_k; l++) fprintf(Demef, "%lf ", x2[l]);
          for(l=0; l<_k; l++) fprintf(Demef, "%lf ", y2[l]);
          // In an attempt to make the viewer compatible with both sexual versions
          // as well as the non-sexual case, I will embed a flag for each option.
#if _DISTINCT_SEXES
          fprintf(Demef, "1 %lf %lf ", mt, kt);
#if _SEXUAL_SELECTION
          fprintf(Demef, "1 %lf ", ft);
#else
          fprintf(Demef, "0 ");
#endif
#else
          fprintf(Demef, "0 ");
#endif
          fprintf(Demef, "1 ");
          // other info could be present, but if this is 0, NOT a species!!!
          fprintf(Demef, "%llu ", p->SpeciesID);

          // Terminate with a newline
          fprintf(Demef, "\n");
        }
#endif


#if _SYS

#if _NEWRM
        int r = 0;
        int res = 0;
        int ll = 0;
        for(l=0; l<_k; l++){
          // printf("\n%d ", p->l[l]);
          if( p->l[l] == 1 ) res = l;
        }
        // printf("\nresourse is at: %d\n", res);

        for( k=0; k<p->ni; k++ ){
          for(ll=0; ll<_k; ll++){
            if( p->i[k]->x[ll] < 0.25 )  r = 0;
            if( p->i[k]->x[ll] >= 0.25 && p->i[k]->x[ll] < 0.5 )  r = 1;
            if( p->i[k]->x[ll] >= 0.5  && p->i[k]->x[ll] < 0.75 ) r = 2;
            if( p->i[k]->x[ll] >= 0.75 ) r = 3;
            // printf("\nx[%d]:%f,r is %d\n", ll, x[ll],r); 
            newrm[4*res+ll][r]++;
          }
        }

#endif

#if NEWFITNESS
        for( k=0; k<p->ni; k++ ){
          total++;
          // printf("\ntotal: %d", total);
          counts++;
          gcs += K(p->i[k],p);
        }
        //printf("\n%d = %d + %d\n\n", total, counts, counth);

#endif
#endif
      }
    }
  }


#if _SYS
  if( !(Gen%_SYS) && sys ) {
    /* Find the number of purebred individuals */
    for(c=i=0; i<NICHES; i++)
      c += s[i][i];
    // Divide by nIndividuals() to find averages
    i = nCurrentIndividuals();
    gcc           /= i;
#if NEWFITNESS
    gcs           /= counts;
#endif
    gpr           /= i;
    match_cur[0]  /= i;
    match_cur[1]  /= i;
    match_pref[0] /= i;
    match_pref[1] /= i;
    amc           /= i;

    /* Print out system stats */
    fprintf(Sysf, "%llu %llu %.1lf %lf %llu %.3lf %llu %.4lf %.4lf %.4lf %.4lf %.4lf ",
            Gen, i, gcc, gpr, max_i, CramersV(), c, match_cur[0], match_cur[1], match_pref[0], match_pref[1], amc);
    /* Niche counts and most prefered niches */
    for(i=0; i<NICHES; i++)
      fprintf(Sysf, "%llu ", ((u64b)count[i]));
    gcc = 0.; Max(pref[0], NICHES, &gcc, &pf);    // finds the max no ind in pref matrix
    gcc = 0.; Max(pref[1], NICHES, &gcc, &pm);    // ditto.
    fprintf(Sysf, "%llu %llu ", pf, pm);
    /* Preference matrix */
    for(i=0; i<NICHES; i++)
      for(j=0; j<NICHES; j++) 
      fprintf(Sysf,"%llu ",s[i][j]);

#if _DISTINCT_SEXES
    // Print out kpn matrix
    // For compatability with other parameters, insert a 1 if this data is dumped:
    // Also dump the matrix size, so view doesn't need to hard-code the value.
    fprintf(Sysf, "1 %llu %llu ", ((long long unsigned int)NICHES), MVK_Y);
    // Dump the kpn matrix
    for(i=0; i<NICHES; i++){
      for(j=0; j<MVK_Y; j++){ 
        fprintf(Sysf, "%llu ", kpn[i][j]);
      }
    }
#else
    // For compatability with other parameters, insert a 0 if this data is not dumped
    fprintf(Sysf, "0 ");
#endif

#if _DISTINCT_SEXES && _SEXUAL_SELECTION
    // Print out fpn matrix
    // For compatability with other parameters, insert a 1 if this data is dumped:
    // Also dump the matrix size, so view doesn't need to hard-code the value.
    fprintf(Sysf, "1 %llu %llu ", ((long long unsigned int)NICHES), MVK_Y);
    // Dump the fpn matrix
    for(i=0; i<NICHES; i++)
      for(j=0; j<MVK_Y; j++) 
        fprintf(Sysf, "%llu ", fpn[i][j]);
#else
    // For compatability with other parameters, insert a 0 if this data is not dumped
    fprintf(Sysf, "0 ");
#endif

#if _MVK
    // Print out mvk matrix
    // For compatability with other parameters, insert a 1 if this data is dumped:
    // Also dump the matrix size, so view doesn't need to hard-code the value.
    fprintf(Sysf, "1 %llu %llu ", MVK_X, MVK_Y);
    // Dump the mvk matrix
    for(i=0; i<MVK_X; i++)
      for(j=0; j<MVK_Y; j++) 
        fprintf(Sysf, "%llu ", mvk[i][j]);
#else
    // For compatability with other parameters, insert a 0 if this data is not dumped
    fprintf(Sysf, "0 ");
#endif

#if _SPVY
    fprintf(Sysf, "1 %llu %llu ", ((u64b)((int)(pow((double)(2),(double)(_k))))), ((u64b)(SPVY_Y)));
    int lx,ly;
    for(lx=0; lx<((int)(pow((double)(2),(double)(_k)))); lx++){
      for(ly=0; ly<SPVY_Y; ly++){
        fprintf(Sysf, "%d ", ((int)(spvy[lx][ly])));
      }
    }
#else
    // For compatability with other parameters, insert a 0 if this data is not dumped
    fprintf(Sysf, "0 ");
#endif
#if _NEWRM
    fprintf(Sysf, "1 %llu %llu ", ((u64b)(_k*4)),((u64b)(_k)));
    for(lx=0; lx<(_k*4); lx++){
      for(ly=0; ly<4; ly++){
        fprintf(Sysf, "%d ", ((int)(newrm[lx][ly])));
        //printf("%d ", newrm[lx][ly]);
      }
      //printf("\n");
    }
#else
    // For compatability with other parameters, insert a 0 if this data is not dumped
    fprintf(Sysf, "0 ");
#endif

#if NEWFITNESS
    fprintf(Sysf, "%.3lf ", gcs);
#endif

    fprintf(Sysf, "1 %llu %llu ", ((u64b)(_k*4)), ((u64b)(MVK_Y)));
    // Dump the spn matrix
    for(lx=0; lx<((int)(pow((double)(2),(double)(_k)))); lx++){
      for(ly=0; ly<MVK_Y; ly++){
        fprintf(Sysf, "%llu ", spn[lx][ly]);
      }
    }

    fprintf(Sysf, "1 %llu %llu ", ((u64b)(_k*4)), ((u64b)(MVK_Y)));
    // Dump the sfpn matrix
    for(lx=0; lx<((int)(pow((double)(2),(double)(_k)))); lx++){
      for(ly=0; ly<MVK_Y; ly++){
        fprintf(Sysf, "%llu ", sfpn[lx][ly]);
      }
    }
    // Terminate with a newline
    fprintf(Sysf,"\n");
    fflush(Sysf);
    // Incriment counter
    Sys_logs++;
  }
#endif
#if _DEME
  // Increment counter
  if( !(Gen%_DEME) ) {
    fflush(Demef);
    Deme_logs++;
  }
#endif


}
#endif

#if _IND
// Dumps individual data to a ".ind" file
static void LogInd()
{
  u64b t,c,i,j,k,n,pf,pm,x,y;  pPatch p;  pIndividual u;  double f,m;

#if 0
  u64b l;
#endif

  if( !(_IND_SAMPLE) || (_IND_SAMPLE < nCurrentIndividuals()) ) {
    //////////////////////////////////////////////////////////////////////
    // Log normally without any subsample
    //////////////////////////////////////////////////////////////////////
    //for(c=0; c<_CPUS; c++)
      for(i=0; i<WIDTH; i++) {
        for(j=0; j<HEIGHT; j++) {
          p = &Threads[0]->Current[i][j];
          for(k=0; k<p->ni; k++) {
            // Print allelic values
#if 1
            // Print data for computing Rst and Fst with Matlab: SG 02/27/05
            // CHANGE THIS!!!
            PreferedNicheFitness(p->i[k], &pf, &f);
            PreferedNicheMigration(p->i[k], &pm, &m);
            // Print generation, patch#, patch's niche, and prefered niches
            // original:
            // fprintf(Indf, "%llu %llu %llu %llu %llu ", Gen, i+WIDTH*c+j*WIDTH*_CPUS, p->n, pf, pm);
            fprintf(Indf, "%llu %llu %llu %llu %llu %llu ", Gen, i, j, p->n, pf, pm); 
            // print mating character and female and male traits: SG 05/04/06 
#if _DISTINCT_SEXES
            fprintf(Indf,"%lf %lf ",p->i[k]->m,p->i[k]->k);
#if _SEXUAL_SELECTION
            fprintf(Indf,"%lf ",p->i[k]->f);
#endif
#endif

#if _Ln
            // Print neutral alleles
            for(n=0; n<_Ln; n++) fprintf(Indf,"%llu %llu ",GetLocus(p->i[k]->z0,n,5),GetLocus(p->i[k]->z1,n,5));
#endif
            // Print terminating newline
            fprintf(Indf, "\n");
#else
            for(l=0; l<_k; l++) for(n=0; n<_Le; n++) fprintf(Indf,"%llu ",GetLocus(p->i[k]->x0,(l*_Le)+n,0));
            for(l=0; l<_k; l++) for(n=0; n<_Le; n++) fprintf(Indf,"%llu ",GetLocus(p->i[k]->x1,(l*_Le)+n,0));
            for(l=0; l<_k; l++) for(n=0; n<_Lp; n++) fprintf(Indf,"%llu ",GetLocus(p->i[k]->y0,(l*_Lp)+n,1));
            for(l=0; l<_k; l++) for(n=0; n<_Lp; n++) fprintf(Indf,"%llu ",GetLocus(p->i[k]->y1,(l*_Lp)+n,1));
#if _Ln
            // Neutral traits only have one character
            for(n=0; n<_Ln; n++) fprintf(Indf,"%llu ",GetLocus(p->i[k]->z0,n,5));
            for(n=0; n<_Ln; n++) fprintf(Indf,"%llu ",GetLocus(p->i[k]->z1,n,5));
#endif
            // Print trait values
            for(l=0; l<_k; l++) fprintf(Indf,"%lf ",p->i[k]->x[l]);
            for(l=0; l<_k; l++) fprintf(Indf,"%lf ",p->i[k]->y[l]);
            // Print carrying capacity and matching with current patch
            fprintf(Indf, "%lf %lf %lf ", K(Mt,p->i[k],p), Qf(p->i[k],p), Qm(p->i[k],p));
            // Print most prefered niches
            PreferedNicheFitness(p->i[k], &pf, &f);
            PreferedNicheMigration(p->i[k], &pm, &m);
            fprintf(Indf, "%llu %llu %lf %lf", pf, pm, f, m);
            // Print terminating newline
            fprintf(Indf, "\n");
            fflush(Indf);
#endif
          }
        }
      }
  } else {
    //////////////////////////////////////////////////////////////////////
    // Log with subsample
    //////////////////////////////////////////////////////////////////////
    for(i=0; i<_IND_SAMPLE; i++) {
      // Find a random individual to log
      t = rnd(&R, nCurrentIndividuals());
      //for(j=0; j<_CPUS; j++) {
        for(x=0; x<WIDTH; x++) {
          for(y=0; y<HEIGHT; y++){
            if( t >= Threads[0]->Current[x][y].ni ) {
              t -= Threads[0]->Current[x][y].ni;
            } else {
              u = Threads[0]->Current[x][y].i[t];
              goto picked;
            }
          }
        }
      //}
      picked:
      // Now that a ramdom choice was made, log it
      // Print data for computing Rst and Fst with Matlab: SG 02/27/05
      // CHANGE THIS!!!
      PreferedNicheFitness(u, &pf, &f);
      PreferedNicheMigration(u, &pm, &m); 
      // Print generation, patch#, patch's niche, and prefered niches
      fprintf(Indf, "%llu %llu %llu %llu %llu %llu ", Gen, x, y, Threads[0]->Current[x][y].n, pf, pm); 
      // print mating character and female and male traits: SG 05/04/06
#if _DISTINCT_SEXES
      fprintf(Indf,"%lf %lf ",u->m,u->k);
#if _SEXUAL_SELECTION
      fprintf(Indf,"%lf ",u->f);
#endif
#endif

#if _Ln
      // Print neutral alleles
      for(n=0; n<_Ln; n++) fprintf(Indf,"%llu %llu ",GetLocus(u->z0,n,5),GetLocus(u->z1,n,5));
#endif
      // Print terminating newline
      fprintf(Indf, "\n");
    }
  }

  // Incriment counter
  Ind_logs++;
}
#endif



// Dumps individual data to a ".indall" file
static void LogAll(FILE *Indallf)
{
  u64b i,j,k,n; pPatch p; int l; //FILE *Indallf; //char fn[64];

  //sprintf(fn,"%s.indall",n);
  //if( !(Indallf=fopen(fn,"w")) ) 
  //  Error("Could not open file \"%s\"!\n",fn);

  // Logs all individuals
  for(i=0; i<WIDTH; i++) {
    for(j=0; j<HEIGHT; j++) {
      p = &Threads[0]->Current[i][j];

      fprintf(Indallf, "%llu %llu %llu ", i, j, p->ni);
      for(l=0; l<_k; l++){ fprintf(Indallf, "%d ", ((int)(p->l[l])));}
      fprintf(Indallf, "%llu ", p->n);
      fprintf(Indallf, "%llu %d", p->SpeciesID, p->edge);
      fprintf(Indallf, "\n");

      for(k=0; k<p->ni; k++) {

        // Print allelic values
        for(l=0; l<_k; l++){
          for(n=0; n<_Le; n++){
            fprintf(Indallf,"%llu ",GetLocus(p->i[k]->x0,(l*_Le)+n,0));
          }
        }
        for(l=0; l<_k; l++){
          for(n=0; n<_Le; n++){
            fprintf(Indallf,"%llu ",GetLocus(p->i[k]->x1,(l*_Le)+n,0));
          }
        }
        for(l=0; l<_k; l++){
          for(n=0; n<_Lp; n++){
            fprintf(Indallf,"%llu ",GetLocus(p->i[k]->y0,(l*_Lp)+n,1));
          }
        }
        for(l=0; l<_k; l++){
          for(n=0; n<_Lp; n++){
            fprintf(Indallf,"%llu ",GetLocus(p->i[k]->y1,(l*_Lp)+n,1));
          }
        }
#if _DISTINCT_SEXES
        for(n=0; n<_Lm; n++) fprintf(Indallf,"%llu ",GetLocus(p->i[k]->m0,n,2));
        for(n=0; n<_Lm; n++) fprintf(Indallf,"%llu ",GetLocus(p->i[k]->m1,n,2));
        for(n=0; n<_Lk; n++) fprintf(Indallf,"%llu ",GetLocus(p->i[k]->k0,n,3));
        for(n=0; n<_Lk; n++) fprintf(Indallf,"%llu ",GetLocus(p->i[k]->k1,n,3));
#if _SEXUAL_SELECTION
        for(n=0; n<_Lf; n++) fprintf(Indallf,"%llu ",GetLocus(p->i[k]->f0,n,4));
        for(n=0; n<_Lf; n++) fprintf(Indallf,"%llu ",GetLocus(p->i[k]->f1,n,4));
#endif
#endif
#if _Ln
        for(n=0; n<_Ln; n++) fprintf(Indallf,"%llu ",GetLocus(p->i[k]->z0,n,5));
        for(n=0; n<_Ln; n++) fprintf(Indallf,"%llu ",GetLocus(p->i[k]->z1,n,5));
#endif

        // Print trait values
        for(l=0; l<_k; l++) fprintf(Indallf,"%lf ",p->i[k]->x[l]);
        for(l=0; l<_k; l++) fprintf(Indallf,"%lf ",p->i[k]->y[l]);
#if _DISTINCT_SEXES
        fprintf(Indallf,"%lf %lf ",p->i[k]->m,p->i[k]->k);
        fprintf(Indallf,"%d ",p->i[k]->s);
#if _SEXUAL_SELECTION
        fprintf(Indallf,"%lf ",p->i[k]->f);
#endif
#endif
        for(n=0; n<(_RRANGE * _RRANGE); n++) fprintf(Indallf,"%lf ",p->i[k]->TRoam[n]);
        //for(n=0; n<(_RRANGE * _RRANGE); n++) fprintf(Indallf,"%lf ",p->i[k]->TRoamMax[n]);
        fprintf(Indallf,"%llu ",p->i[k]->id);

        fprintf(Indallf, "\n");
        fflush(Indallf);
      }
    }
  }
}


// reads the data from ".indall" file
// change to the following line when this function is incorporated in the checkpoint function
static void LoadAll(FILE *f)
{
  u64b i,j,k,n,v;  pPatch p,q; int l; //FILE *f;

  ReleaseAllCurrentIndividuals(Threads[0]);
  SwapGenerations(Threads[0]);
  ReleaseAllCurrentIndividuals(Threads[0]);
  SwapGenerations(Threads[0]);

  // Logs all current individuals
  for(i=0; i<WIDTH; i++) {
    for(j=0; j<HEIGHT; j++) {
      p = &(Threads[0]->Current[i][j]);

      fscanf(f, "%d %d %llu ", &p->lat, &p->lon, &p->ni);
      for(l=0; l<_k; l++){
        fscanf(f, "%hhu ", &p->l[l]);
      }
      fscanf(f, "%llu ", &p->n);
      fscanf(f, "%llu %d", &p->SpeciesID, &p->edge);
      fscanf(f, "\n");

      if( p->ni == 0 ) continue;
      for(k=0; k<p->ni; k++) {
        p->i[k] = GetCurrentIndividual(Threads[0]);
        LinkIndividual(p->i[k]);
        // Print allelic values

        for(l=0; l<_k; l++){
          for(n=0; n<_Le; n++){
            fscanf(f,"%llu ", &v);
            SetLocus(p->i[k]->x0,(l*_Le)+n,v,0);
          }
        }
        for(l=0; l<_k; l++){
          for(n=0; n<_Le; n++){
            fscanf(f,"%llu ", &v);
            SetLocus(p->i[k]->x1,(l*_Le)+n,v,0);
          }
        }
        for(l=0; l<_k; l++){
          for(n=0; n<_Lp; n++){
            fscanf(f,"%llu ", &v);
            SetLocus(p->i[k]->y0,(l*_Lp)+n,v,1);
          }
        }
        for(l=0; l<_k; l++){
          for(n=0; n<_Lp; n++){
            fscanf(f,"%llu ", &v);
            SetLocus(p->i[k]->y1,(l*_Lp)+n,v,1);
          }
        }

#if _DISTINCT_SEXES
        for(n=0; n<_Lm; n++){
          fscanf(f,"%llu ", &v);
          SetLocus(p->i[k]->m0,n,v,2);
        }
        for(n=0; n<_Lm; n++){
          fscanf(f,"%llu ", &v);
          SetLocus(p->i[k]->m1,n,v,2);
        }
        for(n=0; n<_Lk; n++){
          fscanf(f,"%llu ", &v);
          SetLocus(p->i[k]->k0,n,v,3);
        }
        for(n=0; n<_Lk; n++){
          fscanf(f,"%llu ", &v); 
          SetLocus(p->i[k]->k1,n,v,3);
        }
#if _SEXUAL_SELECTION
        for(n=0; n<_Lf; n++){
          fscanf(f,"%llu ", &v);
          SetLocus(p->i[k]->f0,n,v,4);
        }
        for(n=0; n<_Lf; n++){
          fscanf(f,"%llu ", &v);
          SetLocus(p->i[k]->f1,n,v,4);
        }
#endif
#endif
#if _Ln
        for(n=0; n<_Ln; n++){
          fscanf(f,"%llu ", &v);
          SetLocus(p->i[k]->z0,n,v,5);
        }
        for(n=0; n<_Ln; n++){
          fscanf(f,"%llu ", &v);
          SetLocus(p->i[k]->z1,n,v,5);
        }
#endif

        // Print trait values
        for(l=0; l<_k; l++) fscanf(f,"%lf ",&p->i[k]->x[l]);
        for(l=0; l<_k; l++) fscanf(f,"%lf ",&p->i[k]->y[l]);
#if _DISTINCT_SEXES
        fscanf(f,"%lf %lf ",&p->i[k]->m,&p->i[k]->k);
        fscanf(f,"%d ",&p->i[k]->s);
#if _SEXUAL_SELECTION
        fscanf(f,"%lf ",&p->i[k]->f);
#endif
#endif
        for(n=0; n<(_RRANGE * _RRANGE); n++) fscanf(f,"%lf ",&p->i[k]->TRoam[n]);
        //for(n=0; n<(_RRANGE * _RRANGE); n++) fscanf(f,"%lf ",&p->i[k]->TRoamMax[n]);
        fscanf(f,"%llu ",&p->i[k]->id);
        fscanf(f, "\n");
      }
    }
  }

  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++) {
      p = &(Threads[0]->Current[i][j]);
      p->nothers = 0;
    }
  }

  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++){
      p = &(Threads[0]->Current[i][j]);
      for(k=0,l=p->ni; k < l; k++) {
        TimeSpentRoam(p->i[k], p);
      }
    }
  }

  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++) {
      p = &(Threads[0]->Current[i][j]);
      p->nothers = 0;
    }
  }

  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++){
      p = &(Threads[0]->Current[i][j]);
      for(k=0,l=p->ni; k < l; k++) {
        TimeSpentRoam(p->i[k], p);
      }
    }
  }

  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++){
      p = &(Threads[0]->Current[i][j]);
      q = &(Threads[0]->Next[i][j]);
      q->lat = p->lat;
      q->lon = p->lon ;
      for(l=0; l<_k; l++) q->l[l] = p->l[l];
      q->n = p->n;
      q->ni = 0;
      q->SpeciesID = 0;
      q->edge = p->edge;
    }
  }


  /*
  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++){
      q = &(Threads[0]->Next[i][j]);
      for(k=0,l=q->ni; k < l; k++) {
        TimeSpentRoam(q->i[k], q);
      }
    }
  }
  */

  /*
  // sanity print
  for(i=0; i<WIDTH; i++){
    for(j=0; j<HEIGHT; j++){
      p = &(Threads[0]->Current[i][j]);
      printf("\nin LoadAll nothers (at %d,%d) is: %d", i, j, p->nothers);
    }
  }
  printf("\n");
  */
  return;
}

void SaveCheckpoint()
{
  int    j;
  char   command[512];
  u32b   rndSt[59];
  FILE  *Indallf;

  if( (Indallf = fopen("indall", "wt")) == NULL )
    error( 1, "Could not create file 'indall'. Check that the file does not exist and is protected.\n" );

  //fprintf( chkpt, "%d %d %d\n", gens, NumGens, Seed );
  fprintf(Indallf, "%llu %llu %llu %llu %llu \n", Gen, NGen, Seed, (u64b)(_WIDTH), (u64b)(_HEIGHT));
  sv_rnd(&R, rndSt);
  for( j = 0; j < 59; j++ ){
    fprintf(Indallf, " %u", rndSt[j]);
    //printf(" %u", rndSt[j]);
  }
  fprintf(Indallf, "\n");
  //printf("\n");
  LogAll(Indallf);
  fclose(Indallf);
  sprintf( command, "tar zcf checkpoint.tar.gz %llu.par %llu.deme %llu.newrd %llu.sys %llu.habitat %llu.range %llu.histo %llu.xvss %llu.trans indall", NGen, NGen, NGen, NGen, NGen, NGen, NGen, NGen, NGen );
  if( system( command ) != 0 )
    printf( "WARNING: Could not create checkpoint file\n" );
  //sprintf( command, "rm -rf indall" );
}


//int load_checkpoint( ThreadState *states )
int LoadCheckpoint()
{
  int    j, dummy, seedDmy, genDmy, wdummy, hdummy; //, Width, Height;
  char   filename[64];
  u32b   rndSt[59];
  FILE  *indall;

  if( system( "tar zxf checkpoint.tar.gz > /dev/null" ) != 0 )
    return 0;

#if _SYS
  fclose(Sysf);
#endif
#if _HABITAT
  fclose(Habitatf);
#endif
#if _RANGE
  fclose(Rangef);
#endif
#if _HISTO
  fclose(Histof);
#endif
#if _XVSS
  fclose(Xvssf);
#endif
#if _TRANS
  fclose(Transf);
#endif
#if _NEWRESOURCED
  fclose(Newrdf);
#endif
#if _DEME
  fclose(Demef);
#endif
#if _IND
  fclose(Indf);
#endif

  sprintf( filename, "%llu.par", NGen );
  if( (Newrdf   = fopen( filename, "a" )) == NULL )
    error( 1, "Could not create file %s. Check that the file does not exist and is protected.\n", filename );
  sprintf( filename, "%llu.newrd", NGen );
  if( (Newrdf   = fopen( filename, "a" )) == NULL )
    error( 1, "Could not create file %s. Check that the file does not exist and is protected.\n", filename );
  sprintf( filename, "%llu.habitat", NGen );
  if( (Habitatf = fopen( filename, "a" )) == NULL )
    error( 1, "Could not create file %s. Check that the file does not exist and is protected.\n", filename );
  sprintf( filename, "%llu.range", NGen );
  if( (Rangef   = fopen( filename, "a" )) == NULL )
    error( 1, "Could not create file %s. Check that the file does not exist and is protected.\n", filename );
  sprintf( filename, "%llu.histo", NGen );
  if( (Histof   = fopen( filename, "a" )) == NULL )
    error( 1, "Could not create file %s. Check that the file does not exist and is protected.\n", filename );
  sprintf( filename, "%llu.xvss", NGen );
  if( (Xvssf   = fopen( filename, "a" )) == NULL )
    error( 1, "Could not create file %s. Check that the file does not exist and is protected.\n", filename );
  sprintf( filename, "%llu.trans", NGen );
  if( (Transf   = fopen( filename, "a" )) == NULL )
    error( 1, "Could not create file %s. Check that the file does not exist and is protected.\n", filename );
  sprintf( filename, "%llu.sys", NGen );
  if( (Sysf     = fopen( filename, "a" )) == NULL )
    error( 1, "Could not create file %s. Check that the file does not exist and is protected.\n", filename );
  sprintf( filename, "%llu.deme", NGen );
  if( (Demef    = fopen( filename, "a" )) == NULL )
    error( 1, "Could not create file %s. Check that the file does not exist and is protected.\n", filename );
#if _IND
  sprintf( filename, "%llu.ind", NGen );
  if( (Indf     = fopen( filename, "a" )) == NULL )
    error( 1, "Could not create file %s. Check that the file does not exist and is protected.\n", filename );
#endif
  if( (indall = fopen( "indall", "rt" )) == NULL )
    return 0;
  if( fscanf( indall, "%d %d %d %d %d \n", &genDmy, &dummy, &seedDmy, &wdummy, &hdummy) < 5 )
    return 0;

  NGen   = dummy;
  Gen    = genDmy;
  Seed   = seedDmy;
  //Width  = wdummy;
  //Height = hdummy;

  for( j = 0; j < 59; j++ ){
    if( fscanf( indall, " %u", &dummy ) < 1 )
      return 0;
    rndSt[j] = dummy;
  }
  rst_rnd( &R, rndSt );

  if( fscanf( indall, "\n" ) < 0 )
    return 0;

  LoadAll(indall);
  fclose(indall);

  // this is a sanity check, put here to check immediately what it is reading from indall, and recreates it
  // compare the old with the new indall
  //if (Gen == 100) SaveCheckpoint();
  return 1;
}

// Writes any necessary data to a log file(s)
static void Log()
{
  static u32b save[59];
  // int n;
  // for(n=0; n<59; n++) printf("%llu ", R.rtab[n]);
  // printf(" - before save\n");
  sv_rnd( &R, save );

  // Write to file(s)
#if (_SYS || _DEME)
  if( (_SYS && !(Gen%_SYS)) || (_DEME && !(Gen%_DEME)) ) LogSysDeme();
#endif
#if _HABITAT
  if( !(Gen == NGen-1) )  ClusterIt(Threads[0]);
#endif
#if _RANGE
  if( !(Gen%_RANGE) )     ClusterByID(Threads[0]);
#endif
#if _HISTO
  if( !(Gen%_HISTO) )     SpeciesPopHist(Threads[0]);
#endif
#if _TRANS
  if( Gen == NGen-1 )     TransectData(Threads[0]);
#endif
#if _NEWRESOURCED
  if( Gen == 1 )          WriteMap(Threads[0]);
#endif
#if _IND
  if( !(Gen%_IND) )       LogInd();
#endif

  // Messages for stdout/stderr
  printf("   g = %llu    p = %llu\n",Gen,nCurrentIndividuals());
  rst_rnd( &R, save );
  // for(n=0; n<59; n++) printf("%llu ", R.rtab[n]);
  // printf(" - after restore\n\n\n");
}

// Advances the simulation one generation
static void Generation()
{

  // Tell worker threads to start working
  SignalWork(1);

  // !!av: To silence compiler warnings..
  Individual a,b;
  memset(&a,0,sizeof(a));
  memset(&b,0,sizeof(b));
  CopyIndividual(&a,&b);
  
  /*
  // Now that the worker threads are done:
  // Move any "wanderers" to their new cpu
  u64b i,j,k;  pIndividual w;  
  for(i=0; i<_CPUS; i++){
    for(j=0; j<HEIGHT; j++) {
      // Handle left buffer
      for(k=0; k<Threads[i]->Current[0][j].ni; k++) {
        // Make sure there is room
        if( Threads[i-1]->Current[WIDTH][j].ni == _MAX_INDIVIDUALS ) {
          ReleaseCurrentIndividual(Threads[i],Threads[i]->Current[0][j].i[k]);
          continue;
        }
        // Get new from destination, install copy, free original
        w = GetCurrentIndividual(Threads[i-1]);
        Threads[i-1]->Current[WIDTH][j].i[Threads[i-1]->Current[WIDTH][j].ni++] = w;
        CopyIndividual(w,Threads[i]->Current[0][j].i[k]);
        ReleaseCurrentIndividual(Threads[i],Threads[i]->Current[0][j].i[k]);
      }
      Threads[i]->Current[0][j].ni = 0;
      // Handle right buffer
      for(k=0; k<Threads[i]->Current[WIDTH+1][j].ni; k++) {
        // Make sure there is room
        if( Threads[i+1]->Current[1][j].ni == _MAX_INDIVIDUALS ) {
          ReleaseCurrentIndividual(Threads[i],Threads[i]->Current[WIDTH+1][j].i[k]);
          continue;
        }
        // Get new from destination, install copy, free original
        w = GetCurrentIndividual(Threads[i+1]);
        Threads[i+1]->Current[1][j].i[Threads[i+1]->Current[1][j].ni++] = w;
        CopyIndividual(w,Threads[i]->Current[WIDTH+1][j].i[k]);
        ReleaseCurrentIndividual(Threads[i],Threads[i]->Current[WIDTH+1][j].i[k]);
      }
      Threads[i]->Current[WIDTH+1][j].ni = 0;
    }
  }
  */

  // Advance generation counter
  Gen++;
}

/***************************************************************************************
 * Main and signal handler...
 ***************************************************************************************/

// Function to be called when the simulation exits
static void Cleanup()
{

  // Print out summary and benchmark data if needed
  printf("* Done\n");
  printf("\n* Summary:\n");
//  printf("   Niche version:  %s\n",VERSION);
  printf("   Ran for:        %llu generations\n",Gen);
  printf("   Seed:           %llu\n",Seed);
#if _BENCHMARK
  printf("\n* Benchmark:\n   %lf Seconds spent processing %llu individuals.\n   %lf Seconds per million.\n",
          ProcessTime/1000000.,nProcessed,((double)ProcessTime)/nProcessed);
#endif

  // Close the log file(s)
#if _SYS
  fprintf(Sysf,"\n%llu", ((u64b)(NGen/_SYS))+1);
  //fprintf(Sysf,"\n%llu", Sys_logs);
  fclose(Sysf);
#endif
#if _HABITAT
  fprintf(Habitatf,"\n%llu", ((u64b)(NGen/_HABITAT))+1);
  //fprintf(Habitatf,"\n%llu", Habitat_logs);
  fclose(Habitatf);
#endif
#if _RANGE
  fprintf(Rangef,"\n%llu", ((u64b)(NGen/_RANGE))+1);
  //fprintf(Rangef,"\n%llu", Range_logs);
  fclose(Rangef);
#endif
#if _HISTO
  fprintf(Histof,"\n%llu", ((u64b)(NGen/_HISTO))+1);
  //fprintf(Histof,"\n%llu", Histo_logs);
  fclose(Histof);
#endif
#if _XVSS
  fprintf(Xvssf,"\n%llu", ((u64b)(NGen/_XVSS))+1);
  fclose(Xvssf);
#endif
#if _TRANS
  fprintf(Transf,"\n%llu", ((u64b)(NGen/_TRANS))+1);
  //fprintf(Transf,"\n%llu", Trans_logs);
  fclose(Transf);
#endif
#if _NEWRESOURCED
  fprintf(Newrdf,"\n%llu", ((u64b)(NGen/_NEWRESOURCED))+1);
  //fprintf(Newrdf,"\n%llu", Newrd_logs);
  fclose(Newrdf);
#endif
#if _DEME
  fprintf(Demef,"\n%llu", ((u64b)(NGen/_DEME))+1);
  //fprintf(Demef,"\n%llu", Deme_logs);
  fclose(Demef);
#endif
#if _IND
  fprintf(Indf,"\n%llu", ((u64b)(NGen/_IND))+1);
  //fprintf(Indf,"\n%llu", Ind_logs);
  fclose(Indf);
#endif

  // All allocated memory and threads will be released by the OS upon process termination,
  // so we don't need to bother with freeing or killing threads here

}


// Signal handler to catch SIGTERM and SIGINT
static void Signals(int arg)
{

  volatile static int called=0;

  if(!called) {
    called = 1;
    fprintf(stderr, "* Signal received; exiting gracefully.\n");
    Cleanup();
    exit(0);
  }
}

// Application entry point
int main(int argc, char **argv)
{
  FILE *d;

  // Check command line args
  if(argc < 2) {
#if _ARBITER
    NGen = _GENERATIONS;
#else
    Error("useage:\n\t%s <ngen>\n\nwhere <ngen> is the number of generations to simulate.\n",*argv);
#endif
  } else {
    sscanf(argv[1],"%llu",&NGen);
  }

#if _ARBITER
  arbiter_init();
#endif

  // Install signal handler
  signal(SIGTERM, Signals);
  signal(SIGINT,  Signals);

  // Initialize and run the simulation
  printf("* Initializing...\n");
  // printf("   Niche version:  %s\n",VERSION);
  printf("   Running for:    %llu generations\n",NGen);
  Init();
  printf("* Done.\n\n* Running simulation:\n");
  // Log() should be after LoadCheckpoint() since it shouldn't write to the files as if gen=0 if there is already an indall file
  // but it shouldn't also write the same generation twice, i.e. gen it resumes, and before advances with Generation() below
  if( !LoadCheckpoint())
    Log();

  while( (Gen < NGen) && nCurrentIndividuals() ) {
    Generation();
#if _ARBITER
    if( !(Gen%_SYS) ){
      char string[128];
      sprintf(string,"g-%llu\n",Gen);
      arbiter_custom(string);
    }
#endif
    /*
    if( Gen == 321){
      printf("\n\n\n\n\n\n\n\n\n\n\n");
    }
    */

    Log();
    //if (Gen == 9)
    if( !(Gen%_RESUME) )
      SaveCheckpoint();
  }
  if( !nCurrentIndividuals() )
    printf("* Extinction.\n");

  // Clean up and return success
  Cleanup();
  d = fopen( ".done", "wt" );
  fclose(d);

#if _ARBITER
  arbiter_finished();
#endif
  return 0;
}

#endif
