#ifndef __RANDOM_H
#define __RANDOM_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define TWO_32   (4294967296.0)

// random generator
#define rndm()  ((++rndx>54)?rtab[rndx=nrndm()]:rtab[rndx]) 
#define U01()   (rndm()/TWO_32)                             // random in interval [0,1)
#define U(x)    (U01()*(x))                                 // random in interval [0,x)
#define rnd(n)  ((u64b)U((n)))                              // random from set {0..n-1}

#ifndef __8b
#define __8b
typedef unsigned char byte;
#endif

#ifndef __32b
#define __32b
// These should be reasonably portable 32-bit integer types
typedef unsigned int u32b;
typedef signed   int s32b;
#endif

// For generating random number i with probability p[i]
typedef struct dist { 
  double *p;
  s32b   *a;
  s32b    n;
  double m1;
  double m2;
} dist;

static u32b   rtab[55];
static byte   rndx;

static void error_rnd(char *s)
{
  printf("%s\n",s);
  exit(1);
}

static byte nrndm()
{
  byte i;

  for (i =  0; i < 24; i++) rtab[i] -= rtab[i+31];
  for (i = 24; i < 55; i++) rtab[i] -= rtab[i-24];

  return 0;
}

// Initialize the 64 bit random number generator with seed j
static void initrand(u32b j) 
{
  s32b k;  unsigned short h,i;

  for (rtab[54] = j |= (k = i = 1); i < 55; i++)
    h = (21*i)%55, rtab[--h] = k, k = j - k, j = rtab[h];

  // Hack for 64 bit
  for(k=0; k<2000; k++)
    nrndm();

  rndx = 0; 
}

// allocate a distribution over {0..n-1}
static dist *allocdist(u32b n) 
{
  dist *d;

  if (!(d    = (dist *)  malloc(sizeof(dist))))          error_rnd("malloc (allocdist: d)");
  d->n = n;
  if (!(d->a = (s32b   *)malloc(d->n * sizeof(s32b  )))) error_rnd("malloc (allocdist: d->a)");
  if (!(d->p = (double *)malloc(d->n * sizeof(double)))) error_rnd("malloc (allocdist: d->p)");
  return d;
}

#define getsmall { while (p[j] >= q) if ((++j) == stop) goto end; t = j++; }
#define getlarge   while (p[k] <  q) if ((++k) == stop) goto cleanup;

// Initialize the distribution d
static dist *initdist(dist *d, double s) 
{
  /*
    Note: d->p must have d->n elements which sum to s on entry to initdist.
    The elements of d->p and d->a are overwritten by the initialization process.
  */

  s32b j,k,t,stop,*a;  double q,*p;

  stop = d->n, q = s/stop, j = k = 0;

  d->m1 = stop/TWO_32;
  d->m2 = s/(stop * TWO_32);

  a = d->a;
  p = d->p;

  getsmall; getlarge;

 loop:
    
  a[t]  = k;
  p[k] += p[t] - q;

  if (p[k] >= q) { 
    if (j == stop) goto end;
    getsmall;
    goto loop;
  }
  t = k++;
  if (k == stop) goto cleanup;
  if (j < k) getsmall;
  getlarge;
  goto loop;

 cleanup:

  a[t] = t;
  while (j < stop) { a[j] = j; j++; }

 end: 
  return d;
}

#undef getsmall
#undef getlarge

// Return element from {0..d->n-1} according to d
static u32b drand(dist *d) 
{
  u32b j;

  if (rndm()*(d->m2) < d->p[j=rndm()*d->m1]) return j;
  return d->a[j];
}

#endif
