#ifndef __PARAMETERS_H
#define __PARAMETERS_H

// this is when running in arbiter cluster (Aaron's)
#define _ARBITER                  0

// this is when running in arbiter cluster (Aaron's)
#define _GENERATIONS              100000

// Number of available processors (threads to create); _WIDTH _must_ be divisible by _CPUS
// Note that this version runs only with 1 CPU
#define _CPUS                     1

// Seed for the random number generator, 0 -> seed based on time
#define _RAND_SEED                0

// Log() only when (Gen%_RESUME) == 0. _RESUME == 0 -> disable
#define _RESUME                   1000

// Log() only logs to graph file when (Gen%_SYS) == 0. _SYS == 0 -> disable
#define _SYS                      1000

// Log() only logs to graph file when (Gen%_DEME) == 0. _DEME == 0 -> disable
#define _DEME                     1000

// Log() only logs to graph file when (Gen%_RANGE) == 0. _RANGE == 0 -> disable
#define _RANGE                    1000

// Log() only logs to graph file when (Gen%_HISTO) == 0. _HISTO == 0 -> disable
#define _HISTO                    1000

// Log() only logs to graph file when (Gen%_XVSS) == 0. _XVSS == 0 -> disable
#define _XVSS                     1000

// Log() only logs to graph file when (Gen%_HABITAT) == 0. _HABITAT == 0 -> disable
#define _HABITAT                  1000

// Log() only logs to graph file when (Gen%_TRANS) == 0. _TRANS == 0 -> disable
#define _TRANS                    1000

// Log() only logs to state file when (Gen%_IND) == 0.  _IND == 0  -> disable
#define _IND                      0
#define _IND_SAMPLE               10

//-------------------------------------------------------------------------------------
// (boolean) resource distribution by fractal
// EXCLUSIVE BOOLEAN WITH _DISTGRADIENT,_ALT_SCENARIO, AND _LANDSCAPEBLOCK, ONLY ONE CAN BE 1, REST SHOULD BE ZERO
#define _NEWRESOURCED             1

// (double) initial standard deviation for the random number generation (!= 0.0)
#define _SIGMAR                   0.1

// (double) parameter that determines the fractal dimension D=3-H
#define _H                        0.1

// (double?) maximal number of recursions
// CAREFULL! _WIDTH=2^_Maxlevel
#define _MAXLEVEL                 6.0

// (boolean) introduces random noise
#define _ADDITION                 1

// (boolean) for resource gradient, makes them to be distributed on four corners
// if zero, random initial probabilities on four corners, I amssuming it would cause more 'ties'
// note if _SIGMAR == 0, and _RGRADIENT == 0, the random no generator spits only 0's. so be careful...
#define _RGRADIENT                0

// this is to keep the fractal nature of resource distribution, either 4 or 8, 8 is better!!!
#define _K                        8

//-------------------------------------------------------------------------------------

// Width and height of environment
// DO NOT FORGET TO ADJUST _MAXLEVEL ABOVE (32->5; 64->6)
#define _WIDTH                    64
#define _HEIGHT                   64

// Number of different characteristics (for _*e and _*p only)
#define _k                        4

// Maximum number of individuals per patch: this has to do with memory allocation.
// 128 hits the upper max regardless of _KO (even when it is 40, so make it 256!!!)
#define _MAX_INDIVIDUALS          256

// Maximum carrying capacity
#define _K0                       40

// Initial number of individuals per patch
#define _INIT_INDIVIDUALS         20

// Birth rate
#define _b                        4

// this is the 'roaming' range for resource search, should be an ODD number
// for larger _DRANGE's RRANGE should be large as well to prevent initial extinctions (5 is OK)
#define _RRANGE                   5

// this is the 'dispersal range' of the offspring.
// should be an ODD number
#define _DRANGE                   3

// these are for fitness: _ALPHA {3 3.5 4}; _BETA {.24 .44 .64}
#define _ALPHA                    3.0
#define _BETA                     0.60
//-------------------------------------------------------------------------------------

// (Boolean) whether the initial population starts a specialists with (1,0,0,0)
#define _SPECIALIST               0

// (Boolean) set it _PREF_DISPERSAL == 0 if random dispersal;
// _PREF_DISPERSAL == 1 to one of the "neighboring patches" if dispersal is pref based
#define _PREF_DISPERSAL           1

// Deviation for mating probabilities (genotypic)
#define _SIGMA_A                  0.05

// Type of non-random mating: 0 == assortative mating, 1 == sexual selection.
// Note that these do nothing if _DISTINCT_SEXES == 0.
// When running with _SEXUAL_SELECTION = 0, be sure to also set _lf to 0 as well.
#define _SEXUAL_SELECTION         1

// To use first ecological character as male mating character
#define _MAGIC_TRAIT              0

// Number of loci per characteristic; _must_ be even if _RANDOM_GENOTYPE == 0
#define _Le                       8
#define _Lp                       8
#define _Lm                       8
#define _Lk                       8
#define _Lf                       8
#define _Ln                       8

// Number of bits per allele; _must_ be a member of {1,2,4,8}
#define _Ae                       1
#define _Ap                       1
#define _Am                       1
#define _Ak                       1
#define _Af                       1
#define _An                       8

// Mutation rate
#define _mue                      0.00001
#define _mup                      0.00001
#define _mum                      0.00001
#define _muk                      0.00001
#define _muf                      0.00001
#define _mun                      0.001

// You _must_ set these to 0 turn off mutation
#define _use_mue                  1
#define _use_mup                  1
#define _use_mum                  1
#define _use_muk                  1
#define _use_muf                  1
#define _use_mun                  1

// When enabled, this will cause niche to use the faster "skip" method to do mutation
#define  SKIP_MUTATION            1

// When enabled, this will cause niche to use Micheal's faster version of exp()
#define  _FAST_EXP                1

// Will cause some extra printing to be done if == 1 (2 for more)
#define _VERBOSE                  1

// Causes some timing information to be computed and printed
#define _BENCHMARK                1

// Prefix to use for output file names (before .ind, etc).  See FormatName() for more details
#define _FILE_NAME                "%g"

// Log() will only write a Mating vs. Marker matrix to the sys file if this is enabled:
#define _MVK                      1

// Log() will only write a species vs. preference matrix to the sys file if this is enabled:
// exclusive with NEWCRAMER below, turn that off if this is on!!!
#define _SPVY                     1

// Log() will only write a the distribution of x traits across resources if this is enabled:
// exclusive with NEWCRAMER below, turn that off if this is on!!!
// NEEDS TO BE TURNED ON/OFF IN VIEW.H AS WELL!!!
#define _NEWRM                    1

// (boolean) this is to check for new cramer's v and fitness vs preference matrix
// it is per species not per niche (to allow for 1010, 1110, etc)
// NEEDS TO BE TURNED ON/OFF IN VIEW.H AS WELL!!!
// TURN THIS OFF OF _SPVY IS ON ABOVE
#define NEWCRAMER                 0

// turn this on of you want species/hybrid fitness (* ALSO IN VIEW.H)
#define NEWFITNESS                1

// (Boolean) Flags weather or not to allow selfing
#define _SELFING                  0

// (Boolean) allow for direct competition within patch? (NOT USED)
#define _COMPETITION              0

// (Boolean) 0: Turn off sexual model, 1: enable sexual model
// When running with _DISTINCT_SEXES == 0, be sure to also set _Lm and _Lk to 0 as well.
#define _DISTINCT_SEXES           1

// (Boolean) use Gaussian stabilizing selection within patch (1) or exponential (0)?
#define _GAUSSIAN                 1

// Extinction rate
#define _x                        0.0

// Crossover rate
#define _Xi                       {.5,.5,.5,.5,.5,.5}

// (Boolean) dispersal mode: (1) Child dispersal, or (0) Parent dispersal (PARENT DISPERSAL IS NOT USED CURRENTLY)
#define _CHILD_DISPERSAL          1

// == 0.5 for square root in the fitness function
#define _GAMMA                    0.5

// species definition if _TRAITMINIMUM = 0.8, then (0.8,0,0,0) will be considered as species 1, etc...
#define _TRAITMINIMUM             0.8
#define _PREFTRAITMINIMUM         0.0

#endif
