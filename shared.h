#include <math.h>

// Useful "utility" functions
// Similar to fprintf(stderr...); exit(-1);
static void Error(const char *fmt, ...)
{
  va_list ap;

  if (!fmt) return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(1);
}

// This is Michael's version of c's exp() function.
// I don't know much about it except it was used in the sim project,
// and is supposed to be a good bit faster than c's version.
static double Exp(double x)
{
  int i,j;  double p;  static double T[512];  static int t = 0;

  if( !t ) {
    // initialize Exp array
    for (i = 512; i--; T[i] = exp((double)i)); 
    t = 1;
  }

  if (x > 0){
    if ((i = x) > 511)
      return 2.2844135865397566e+222;
    for (p = 1. + (x-i)/256., j = 8; j--; p *= p);
    return p*T[i];
  }
  if (x < 0){
    if ((i = -x) > 511)
      return 4.377491037053051e-223;
    for (p = 1. - (x+i)/256., j = 8; j--; p *= p);
    return 1./(p*T[i]);
  }
  return 1.;
}

// Re-mapping macro
#if _FAST_EXP
#define exp(x) Exp(x)
#endif


// Functions dealing directly with maintenance of Individuals
// Returns a pointer to an available individual structure.  NULL for none.
static pIndividual GetCurrentIndividual(pThreadData td)
{
  // Record where this individual was allocated from
  td->ICurrent[td->nICurrent]->id = td->nICurrent;
  return td->ICurrent[td->nICurrent++];
}

// Links an individual's x, y, and z characteristics to it's data (d) member
static void LinkIndividual(pIndividual i)
{
  i->x0 = i->d;
  i->x1 = i->d+LENGTH_E;
  i->y0 = i->d+(LENGTH_E<<1);
  i->y1 = i->d+(LENGTH_E<<1)+LENGTH_P;
#if _DISTINCT_SEXES
  i->m0 = i->d+(LENGTH_E<<1)+(LENGTH_P<<1);
  i->m1 = i->d+(LENGTH_E<<1)+(LENGTH_P<<1)+LENGTH_M;
  i->k0 = i->d+(LENGTH_E<<1)+(LENGTH_P<<1)+(LENGTH_M<<1);
  i->k1 = i->d+(LENGTH_E<<1)+(LENGTH_P<<1)+(LENGTH_M<<1)+LENGTH_K;
#if _SEXUAL_SELECTION
  i->f0 = i->d+(LENGTH_E<<1)+(LENGTH_P<<1)+(LENGTH_M<<1)+(LENGTH_K<<1);
  i->f1 = i->d+(LENGTH_E<<1)+(LENGTH_P<<1)+(LENGTH_M<<1)+(LENGTH_K<<1)+LENGTH_F;
#endif
#endif
#if _Ln
  i->z0 = i->d+(LENGTH_E<<1)+(LENGTH_P<<1)+(LENGTH_M<<1)+(LENGTH_K<<1)+(LENGTH_F<<1);
  i->z1 = i->d+(LENGTH_E<<1)+(LENGTH_P<<1)+(LENGTH_M<<1)+(LENGTH_K<<1)+(LENGTH_F<<1)+LENGTH_N;
#endif
}


// Copies all data in a safe manner from Individual s to d
static void CopyIndividual(pIndividual d, pIndividual s)
{
  u64b did;

  // Save id field
  did  = d->id;

  // Copy data
  memcpy(d,s,sizeof(Individual));

  // Re-link and restore id field
  LinkIndividual(d);
  d->id  = did;
}


// Functions which compute probabilities, properties, or preferences of individuals
// Effective carrying capacity - fitness
static double K(pIndividual i, pPatch p)
{

  int     j, r, k, res;
  double  cost, gain, tradeoff, s, t;
  s = 0.;
  t = 0.;

  // new version
  // res is the resource position
  // Go through the range of the individual
  for( j=0, r=0; r<p->rrne; r++ ){
    /*
    // sanity print
    printf("\n %d/%d\n", r, p->rrne);
    for( j=0; j<_k;j++ ){
      printf("\nx is: %f, env is: %d, time is: %f\n", i->x[j], p->rne[r]->l[j], i->TRoam[j]);
    }
    */

    // go through the resource types and find that is not zero
    res      = 0;
    gain     = 0.;
    cost     = 0.;
    tradeoff = 0.;

    t += i->TRoam[r];
    // find the position for the resource and calculate 'gain'
    for( k=0; k<_k;k++ ){
      if (p->rne[r]->l[k] != 0){
        res = k;
        gain = exp(-_ALPHA * (i->x[res]-1.) * (i->x[res]-1.)) * i->TRoam[r];
      }
    }
    // printf("resource is at: %d, and gain is: %f, with time: %f\n", res, gain, i->TRoam[res]);

    // calculate trade-off
    for( j=0; j<_k;j++ ){
      // have to go through the entire loop again find empty spots 
      if( j != res ){
          // printf("\n'empty' resources are at positions: %d\n", j);
          // printf("\nj is: %d, other traits are: %lf", j, ((double)(i->x[j])));
         cost += pow(((double)(i->x[j])), _GAMMA);
       }
     }

     tradeoff = exp(-_BETA * cost);
     s += gain * tradeoff; // fitness
     // printf("cost is: %f, trade-off is: %f, cumulative fitness is: %f\n", cost, tradeoff, s);
  }

  // printf("total fitness in range is: %f\n", s);
  /*
  //printf("\ntotal fitness is: %f (%d)\n\n", tf, p->rrne);
  if(s < 0.8){
    printf("the fitness (%f, %f, %f, %f) is: %f\n", i->x[0], i->x[1], i->x[2], i->x[3], s);
    for( r=0; r<p->rrne; r++ ){
      printf("*p:%d (%d, %d, %d, %d) at (%d/%d), t:%f ",p->n, p->l[0], p->l[1], p->l[2], p->l[3], r, p->rrne, i->TRoam[r]);
    }
    printf("\n");
  }*/

  return s;
}


// preference for a resource in a patch
static double P(pIndividual i, pPatch p)
{

  int j;  double s;
  s = 0.;
  //int r = 0;

  for(j=0; j<_k; j++){
    if ( p->l[j] == 1 ){
      //r = j;
      // printf("\nresource is present at: %d", j); 
      s += 0.001 + ((1. - 0.002) * (i->y[j]));
      // printf("\npreference %f for resource %d\n", s, j);
    } else {
      s += 0.;
    }
  }

  /*
  // sanity print
  printf("preference for patch at (%d, %d) with resource %d (%d, %d, %d, %d) by ind (%lf,%lf,%lf,%lf) is: %f\n", p->lat, p->lon, r, p->l[0], p->l[1], p->l[2], p->l[3], i->y[0], i->y[1], i->y[2], i->y[3], s);
  */

  if( s == 0. ){
    Error("\nThere might be a problem with preference calculation: there are probably some patches without resources----*\n");
    // printf("preference for patch at (%d, %d) with resource %d (%d, %d, %d, %d) by ind (%lf,%lf,%lf,%lf) is: %f\n", p->lat, p->lon, r, p->l[0], p->l[1], p->l[2], p->l[3], i->y[0], i->y[1], i->y[2], i->y[3], s);
  }

  return s;
}
