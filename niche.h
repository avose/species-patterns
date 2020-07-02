#ifndef __NICHE_H
#define __NICHE_H

#include "parameters.h"
#include "types.h"

#define NICHE_LITTLE_ENDIAN 1
#define NICHE_BIG_ENDIAN    2

#define MALE                0
#define FEMALE              1

// _WIDTH and _HEIGHT as seen from an individual thread's perspective
#define WIDTH               (_WIDTH/_CPUS)
#define HEIGHT              _HEIGHT

// Maximum population sizes
// #define MAX_POP          (_WIDTH*_HEIGHT*_MAX_INDIVIDUALS)
#define MAX_THREAD_POP      (_WIDTH*_HEIGHT*_MAX_INDIVIDUALS)

// Number of different niches possible
#define NICHES              _k

// Number of u64b's needed per half characteristic-set
#define LENGTH_E            (((_k*_Le*_Ae)/64)+((((_k*_Le*_Ae)%64)>0)*1))
#define LENGTH_P            (((_k*_Lp*_Ap)/64)+((((_k*_Lp*_Ap)%64)>0)*1))
#define LENGTH_M            (((_Lm*_Am)/64)+((((_Lm*_Am)%64)>0)*1))
#define LENGTH_K            (((_Lk*_Ak)/64)+((((_Lk*_Ak)%64)>0)*1))
#define LENGTH_F            (((_Lf*_Af)/64)+((((_Lf*_Af)%64)>0)*1))
#define LENGTH_N            (((_Ln*_An)/64)+((((_Ln*_An)%64)>0)*1))
#define T_LENGTH            ((LENGTH_E<<1)+(LENGTH_P<<1)+(LENGTH_M<<1)+(LENGTH_K<<1)+(LENGTH_F<<1)+(LENGTH_N<<1))

// Number of bytes that actually need to be processed
#define PLENGTH_E           (((_k*_Le*_Ae)/8)+((((_k*_Le*_Ae)%8)>0)*1))
#define PLENGTH_P           (((_k*_Lp*_Ap)/8)+((((_k*_Lp*_Ap)%8)>0)*1))
#define PLENGTH_M           (((_Lm*_Am)/8)+((((_Lm*_Am)%8)>0)*1))
#define PLENGTH_K           (((_Lk*_Ak)/8)+((((_Lk*_Ak)%8)>0)*1))
#define PLENGTH_F           (((_Lf*_Af)/8)+((((_Lf*_Af)%8)>0)*1))
#define PLENGTH_N           (((_Ln*_An)/8)+((((_Ln*_An)%8)>0)*1))

// Number of bits in a u64b
#define BITS                (8*sizeof(u64b))

// Loci per byte; used by fast-recombination
#define FIELDS_E            (8/_Ae)
#define FIELDS_P            (8/_Ap)
#define FIELDS_M            (8/_Am)
#define FIELDS_K            (8/_Ak)
#define FIELDS_F            (8/_Af)
#define FIELDS_N            (8/_An)

// Mask for pulling out a locus, and "loci per u64b(word)"
#define MASK_E              ((((u64b)1)<<_Ae)-1)
#define MASK_P              ((((u64b)1)<<_Ap)-1)
#define MASK_M              ((((u64b)1)<<_Am)-1)
#define MASK_K              ((((u64b)1)<<_Ak)-1)
#define MASK_F              ((((u64b)1)<<_Af)-1)
#define MASK_N              ((((u64b)1)<<_An)-1)
#define LPW_E               (BITS/_Ae)
#define LPW_P               (BITS/_Ap)
#define LPW_M               (BITS/_Am)
#define LPW_K               (BITS/_Ak)
#define LPW_F               (BITS/_Af)
#define LPW_N               (BITS/_An)

// This is used to help build the MVK matrix; number of buckets needed so
// that each bucket describes a unique trait value.  MAX_MVK is here in
// an attempt to make it easier to raise the max matrix size if someone
// ever decides they want to do that.
#define MVK_X               ((((MASK_M)*_Lm)<<1)+1)
#define MVK_Y               ((((MASK_K)*_Lk)<<1)+1)
#define X0_Y                ((((MASK_E)*_Le)<<1)+1)

// this is for the species vs. preference matrix (each y trait gets four slots)
#define SPVY_Y              _k*4

// Nifty little macros to make malloc a little cleaner
#define ALLOC(t,s,e)        { if( !((t)=malloc(s)) )    Error(e);                         }
#define ZALLOC(t,s,e)       { if( !((t)=malloc(s)) )    Error(e); else memset((t),0,(s)); }
#define REALLOC(t,s,e)      { if( !((t)=realloc(t,s)) ) Error(e);                         }

typedef  void* (*WorkThread)(void*);

// Structure to represent an individual
typedef struct {
  u64b      d[T_LENGTH];                  // Space for x, y, m, k, and z below
  u64b     *x0;                           // d; ecological characters
  u64b     *x1;                           // d; ecological characters
  u64b     *y0;                           // d; niche preference
  u64b     *y1;                           // d; niche preference
  double    x[_k];                        // Scaled characteristis
  double    y[_k];                        // Scaled niche preference
#if _DISTINCT_SEXES
  u64b     *m0;                           // d; mating (choosiness in females)
  u64b     *m1;                           // d; mating (choosiness in females)
  u64b     *k0;                           // d; marker (in males, in females only if no sexual selection)
  u64b     *k1;                           // d; marker (in males, in females only if no sexual selection)
  double    m;                            // Scaled mating (choosiness in females)
  double    k;                            // Scaled marker (in males, in females only if no sexual selection)
  int       s;                            // Sex of the individual
#if _SEXUAL_SELECTION
  u64b     *f0;                           // d; mating preference (female trait)
  u64b     *f1;                           // d; mating preference (female trait)
  double    f;                            // Scaled mating preference (female trait)
#endif
#endif
#if _Ln
  u64b     *z0;                           // d; neutral characters
  u64b     *z1;                           // d; neutral characters
#endif
  u64b      id;                           // Individuals id (index into ->Individuals)
  double    TRoam[_RRANGE * _RRANGE];     // individuals time spent profile in the roaming range
} Individual, *pIndividual;

// Structure to hold all needed information about an extinction event
typedef struct {
  byte              l[_k];                  // New patch traits
  u64b              n;                      // New niche value
  u64b              g;                      // The generation in which this event takes place
} ExtinctionInfo,  *pExtinctionInfo;

typedef struct {
  pIndividual       ind;                    // the other individual
  double            timespent;              // time the other individual spends in this current patch
} OthersTime,      *pOthersTime;

// Structure to represent a patch
typedef struct patch {
  pIndividual       i[_MAX_INDIVIDUALS];
  int               lat;
  int               lon;
  u64b              ni;                               // The number of individuals currently in this patch
  u64b              n;                                // This patch's "niche value"
  u64b              nne;                              // Number of neighboring patches
  struct patch     *ne[9];                            // Array of neighboring patches (+1 for loop structure)
  byte              l[_k];                            // The patch traits
  //ExtinctionInfo *ei;                               // Extinction information
  //u64b            en;                               // Number of extinction events
  //u64b            ec;                               // Counter to track extinction events
  u64b              ddne;                             // no of dispersal patches
  struct patch     *dne[(_DRANGE * _DRANGE) + 1];     // array of dispersal patches (plus 1 as above)
  u64b              rrne;                             // no of roaming patches
  struct patch     *rne[_RRANGE * _RRANGE + 1];       // array of roaming patches (plus 1 as above)
  OthersTime        Other[_RRANGE * _RRANGE * _MAX_INDIVIDUALS];
  u32b              nothers;                          // no of others spending part of their time in the focal patch
  int               included;                         // whether it is included in the current patch or not
  int               rangeid;
  u64b              SpeciesID;                        // Patch's "species value" like "niche value"
  int               AllSpecies[16];                   // the species ids present in the patch
  int               speciespops[16];                  // pop size of each species in the patch
  int               SRangeIncluded[16];               // for range calculation
  int               edge;                             // (boolean) whether the patch is at the edge or a center
  } Patch, *pPatch;

// Define a type for our "space"
typedef Patch **Space, ***pSpace;

// This bundles all the data a thread needs into a nice little structure
typedef struct {
  Space           Current,Next;       // Current and next generation spaces
  Individual     *ICurrentData;       // Enough storeage for MAXPOP individuals
  Individual     *INextData;          // Enough storeage for MAXPOP individuals
  pIndividual    *ICurrent;           // Array of pointers to individual storeage
  pIndividual    *INext;              // Array of pointers to individual storeage
  pIndividual    *ICurrentCleared;    // "Cleared" or initial allocation
  pIndividual    *INextCleared;       // "Cleared" or initial allocation
  u64b            nICurrent;          // Number of individual structures in use
  u64b            nINext;             // Number of individual structures in use
  int             id;                 // Thread id
#if _Ln
  distribution   *CrossDist[6];       // Byte-wise crossover distribution
  distribution   *MutateDist[6][256]; // Mutation distribution for each character
  distribution   *MutateSkip[6];      // Skip distribution for each character
#else
  distribution   *CrossDist[5];       // Byte-wise crossover distribution
  distribution   *MutateDist[5][256]; // Mutation distribution for each character
  distribution   *MutateSkip[5];      // Skip distribution for each character
#endif
} ThreadData, *pThreadData;

#endif
