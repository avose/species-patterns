#include <stdio.h>
#include <gsl/gsl_fit.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/select.h>
#include <pthread.h>
#include <unistd.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "view.h"
#include "types.h"
#if PRETTY_PS
#include "ps.h"
#endif

/***************************************************************************************
 * Globals
 ***************************************************************************************/

Generation      *Gen;                // pointers to generations
u64b             nGen;               // number of generations
u64b             Width,Height;       // Width and Height of the patch space
int              nTraits;            // Number of traits (_k)
double           Max;                // _K0
double           _b;
int              Clean;              // True if the data file contains a trailer
int              Amcg=-1;            // The generation which fisrt saw amc >= .5
int              Amcf=-1;            // The frame which fisrt saw amc >= .5

char             *FNPrefix;

int              Sexual, Selection, Speciesid;  // Flags for the sexual options
int              MVK;                // Flag for the mvk display
int              MVKx,MVKy;          // Width and height of the MVK matrix
int              KPN;                // Flag for kpn display
int              KPNx,KPNy;          // Dimensions of kpn matrix
int              SPN;                // Flag for spn display
int              SPNx,SPNy;          // Dimensions of spn matrix
int              SFPN;               // Flag for spn display
int              SFPNx,SFPNy;        // Dimensions of spn matrix
int              FPN;                // Flag for fpn display
int              FPNx,FPNy;          // Dimensions of fpn matrix
int              FPr;                // Flag for fpn display
int              FPrx,FPry;          // Dimensions of fpn matrix
int              SPVY;               // Flag for spvy display
int              SPVYx,SPVYy;        // Dimensions of spvy matrix
int              NEWRM;              // Flag for newrm display
int              NEWRMx,NEWRMy;      // Dimensions of newrm matrix

float            Trans[2],Scale=22;  // graph translation and scale
float            HackScale;          // graph translation and scale
int              Mx=10000,My=10000;  // Mouse coordinates
int              Speed = 10, Frame;  // FPSm and currently drawn generation
int              X=1400,Y=900;       // Width and height of window
int              dump;               // dump mode flag
int              ColorMode;          // Draw colors for pref or colors for marker
int              Filter[1024];       // Filters which niches are drawn

double           Coexistmean;
double           Coexistmax;
double           Coexisttotal;
double           Coexistcount;
double           Multimean;
double           Multimax;
double           Multitotal;
double           Multicount;
int              Maxmultinos = 0;
int              Timetospec = 0;
// this works only if _k<=4;
int              Tspec[17];
int              Tsexuals = 0;
int              Tss[17];
int              Maxss[17];

// Simple vector type
typedef struct {
  float          x;
  float          y;
  float          z;
} vec3f_t;

typedef struct {
  s32b           ox;        // Mouse down position
  s32b           oy;
  s32b           nx;        // Current mouse position
  s32b           ny;

  u32b           d;         // Mouse down flag

  vec3f_t        rot;       // Rotation vector
  vec3f_t        trn;       // Translation vector

  s32b           x;
  s32b           y;
  s32b           w;
  s32b           h;
} plot_3d_t;

plot_3d_t        Plot;


#if PRETTY_PS
FILE            *PSFile0;
FILE            *PSFile1;
#endif

/* Color blend array */
Byte RB[78][3] = {{0xff, 0x00, 0x00},{0xff, 0x10, 0x00},{0xff, 0x1f, 0x00},{0xff, 0x2f, 0x00},
		  {0xff, 0x3e, 0x00},{0xff, 0x4e, 0x00},{0xff, 0x5d, 0x00},{0xff, 0x6d, 0x00},
		  {0xff, 0x7c, 0x00},{0xff, 0x8c, 0x00},{0xff, 0x9c, 0x00},{0xff, 0xab, 0x00},
		  {0xff, 0xbb, 0x00},{0xff, 0xca, 0x00},{0xff, 0xda, 0x00},{0xff, 0xe9, 0x00},
		  {0xff, 0xf9, 0x00},{0xf6, 0xff, 0x00},{0xe6, 0xff, 0x00},{0xd6, 0xff, 0x00},
		  {0xc7, 0xff, 0x00},{0xb7, 0xff, 0x00},{0xa8, 0xff, 0x00},{0x98, 0xff, 0x00},
		  {0x89, 0xff, 0x00},{0x79, 0xff, 0x00},{0x6a, 0xff, 0x00},{0x5a, 0xff, 0x00},
		  {0x4a, 0xff, 0x00},{0x3b, 0xff, 0x00},{0x2b, 0xff, 0x00},{0x1c, 0xff, 0x00},
		  {0x0c, 0xff, 0x00},{0x00, 0xff, 0x03},{0x00, 0xff, 0x13},{0x00, 0xff, 0x22},
		  {0x00, 0xff, 0x32},{0x00, 0xff, 0x42},{0x00, 0xff, 0x51},{0x00, 0xff, 0x61},
		  {0x00, 0xff, 0x70},{0x00, 0xff, 0x80},{0x00, 0xff, 0x8f},{0x00, 0xff, 0x9f},
		  {0x00, 0xff, 0xae},{0x00, 0xff, 0xbe},{0x00, 0xff, 0xce},{0x00, 0xff, 0xdd},
		  {0x00, 0xff, 0xed},{0x00, 0xff, 0xfc},{0x00, 0xf2, 0xff},{0x00, 0xe3, 0xff},
		  {0x00, 0xd3, 0xff},{0x00, 0xc4, 0xff},{0x00, 0xb4, 0xff},{0x00, 0xa4, 0xff},
		  {0x00, 0x95, 0xff},{0x00, 0x85, 0xff},{0x00, 0x76, 0xff},{0x00, 0x66, 0xff},
		  {0x00, 0x57, 0xff},{0x00, 0x47, 0xff},{0x00, 0x38, 0xff},{0x00, 0x28, 0xff},
		  {0x00, 0x18, 0xff},{0x00, 0x09, 0xff},{0x07, 0x00, 0xff},{0x16, 0x00, 0xff},
		  {0x26, 0x00, 0xff},{0x35, 0x00, 0xff},{0x45, 0x00, 0xff},{0x54, 0x00, 0xff},
		  {0x64, 0x00, 0xff},{0x74, 0x00, 0xff},{0x83, 0x00, 0xff},{0x93, 0x00, 0xff},
		  {0xa2, 0x00, 0xff},{0xb2, 0x00, 0xff}};

#if PRESENTATION_MODE
/* simple color array */
Byte simple[8][3] = {{0,255,0},{255,0,0},{0,0,255},{255,255,255},{255,0,255},{255,255,0},
		     {0,255,255},{255,110,0}};
#endif

void Black()  { glColor3ub(  0,   0,   0); }
void Cyan()   { glColor3ub(  0, 255, 255); }
void Yellow() { glColor3ub(255, 255,   0); }
void Red()    { glColor3ub(255,   0,   0); }
void Purple() { glColor3ub(255,   0, 255); }
void Blue()   { glColor3ub(  0,   0, 255); }
void White()  { glColor3ub(255, 255, 255); }
void Green()  { glColor3ub(  0, 255,   0); }


/***************************************************************************************
 * Font / GL / X / etc
 ***************************************************************************************/

void ViewPort2D(GLWindow *glw)
{
  glViewport(0, 0, glw->width, glw->height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, glw->width, glw->height, 0.0, 1, -100);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void ViewPort3D(int x, int y, int w, int h)
{
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glViewport(x, y, w, h); 
  gluPerspective(55.0f,(float)w/(float)h,0.8f,10.0f);
  glTranslatef(0.0f, 0.0f, -2.0f);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static GLuint BuildFont(GLWindow *glw, char *fname)
{
  XFontStruct *font;  GLuint fbase;   

  /* Storage for 96 characters */
  fbase = glGenLists(96);

  /* Load a font with a specific name in "Host Portable Character Encoding" */
  if ( !(font = XLoadQueryFont(glw->dpy, fname)) ) {
    fprintf(stderr,"Could not load font \"%s\"\n",fname);
    exit(-1);
  }

  /* Build 96 display lists out of our font starting at char 32 */
  glXUseXFont(font->fid, 32, 96, fbase);
  XFreeFont(glw->dpy, font);

  return fbase;
}

/*
static void DeleteFont(GLuint font)
{
  glDeleteLists(font, 96);
}
*/

static void printGLf(GLuint font, const char *fmt, ...)
{
  va_list ap;  char text[256];

  if (fmt == NULL) return;

  va_start(ap, fmt);  
  vsprintf(text, fmt, ap);
  va_end(ap);
  glPushAttrib(GL_LIST_BIT);
  glListBase(font - 32);
  glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);
  glPopAttrib();
}

/* called when window is resized */
static void ResizeGLScene(unsigned int width, unsigned int height)
{
  glViewport(0, 0, width, height); 
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0., width, height, 0., 1, -100);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

/* destroy window */
static void KillGLWindow(GLWindow *glw)
{
  if (glw->ctx) {
    glDeleteLists(0, 65535);
    if (!glXMakeCurrent(glw->dpy, None, NULL))
      printf("Could not release drawing context.\n");
    glXDestroyContext(glw->dpy, glw->ctx);
    glw->ctx = NULL;
  }
  XCloseDisplay(glw->dpy);
}

/* create window */
static void CreateGLWindow(GLWindow *glw, char* title, int width, int height)
{
  XVisualInfo *vi;  Colormap cmap;  Window twin;  Atom wmDelete;  int glxM, glxm, t;

  /* Connect to X */
  if( !(glw->dpy    = XOpenDisplay(0)) ) {
    fprintf(stderr,"Cannot connect to X server!\n");
    exit(1);
  }
  glw->screen = DefaultScreen(glw->dpy);

  /* Get the appropriate visual */
  glXQueryVersion(glw->dpy, &glxM, &glxm);
#ifdef VERBOSE
  printf("%d: glX-Version %d.%d\n", glw->id, glxM, glxm);
#endif
  if ( (vi = glXChooseVisual(glw->dpy, glw->screen, attrListDbl)) )
#ifdef VERBOSE
    printf("%d: Selected doublebuffered mode.\n", glw->id);
#else
  ;
#endif
  else {
    vi = glXChooseVisual(glw->dpy, glw->screen, attrListSgl);
#ifdef VERBOSE
    printf("%d: Selected singlebuffered mode.\n", glw->id);
#endif
  }

  /* Create a GLX context and color map */
  glw->ctx               = glXCreateContext(glw->dpy, vi, 0, GL_TRUE);
  cmap                   = XCreateColormap(glw->dpy, RootWindow(glw->dpy, vi->screen), vi->visual, AllocNone);
  glw->attr.colormap     = cmap;
  glw->attr.border_pixel = 0;

  /* Create the window */
  glw->attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask| ButtonReleaseMask | StructureNotifyMask | PointerMotionMask;
  glw->win = XCreateWindow(glw->dpy, RootWindow(glw->dpy, vi->screen),
                           0, 0, width, height, 0, vi->depth, InputOutput, vi->visual,
                           CWBorderPixel | CWColormap | CWEventMask, &glw->attr);
  wmDelete = XInternAtom(glw->dpy, "WM_DELETE_WINDOW", True);
  XSetWMProtocols(glw->dpy, glw->win, &wmDelete, 1);
  XSetStandardProperties(glw->dpy, glw->win, title, title, None, NULL, 0, NULL);
  XMapRaised(glw->dpy, glw->win);

  /* Connect the glx-context to the window */
  glXMakeCurrent(glw->dpy, glw->win, glw->ctx);
  XGetGeometry(glw->dpy, glw->win, &twin, &glw->x, &glw->y,
              &glw->width, &glw->height, (unsigned int*)&t, &glw->depth);
#ifdef VERBOSE
  printf("%d: Depth %d\n", glw->id, glw->depth);
  if (glXIsDirect(glw->dpy, glw->ctx)) printf("%d: Direct Rendering enabled.\n", glw->id);
  else                                 printf("%d: Direct Rendering disabled.\n", glw->id);
#endif
}

void yeild()
{
  struct timeval tv;

  // Sleep for a ms
  tv.tv_sec  = 0;
  tv.tv_usec = 30000;
  select(0, NULL,  NULL, NULL, &tv);
}

static int AnyEvent(Display *d, XEvent *e, XPointer p)
{
  return True;
}

/***************************************************************************************
 * Data file loading code
 ***************************************************************************************/

#define ALLOC(x,y)  if (!(x = malloc(y))) { perror("niche_view"); exit(2); }
#define ZALLOC(x,y) if (!(x = malloc(y))) { perror("niche_view"); exit(2); } else memset(x,0,y);

int ReadSys(char *fn)
{
  u64b i,j,k=0;  FILE *f;  char b[1024];  int lm,g=0,nav;  double m,av,min,t=0.;

  // Open file and read the header
  Clean = 1;
  if (!(f = fopen(fn,"r"))) {
    sprintf(b,"niche_view: .sys-open: \"%s\"",fn);
    perror(b);
    return 1;
  }
  fseek(f,i=-1,SEEK_END);
  while( (lm=fgetc(f)) != '\n' ) {
    if( lm == EOF ) {
      fprintf(stderr,"niche_view: data file looks VERY wrong..\n");
      return 3;
    }
    fseek(f,--i,SEEK_END);
  }
  if( fscanf(f,"%llu", &nGen) != 1 ) { 
    fprintf(stderr, "niche_view: sys: trailer error\n");
    fprintf(stderr, "niche_view: sys: attempting to guess... (500) \n");
    nGen = 500;
    g = 1;
  }
  rewind(f);
  if (fscanf(f,"%d %lf %lf %llu %llu\n",&nTraits,&Max,&_b,&Width,&Height) != 5) 
    { fprintf(stderr, "niche_view: sys: header error\n"); return 1; }
  if(nTraits > 64) {
    fprintf(stderr, "niche_view: sys: _k too large.  (0 < _k < 64)\n");
    return 1;
  }
  // Auto-Scale
  Scale = ((((X-280)/Width)<((Y-410)/(Height)))?((X-280)/Width):((Y-410)/(Height)));
  HackScale = Scale;
#ifdef VERBOSE
  fprintf(stderr,"Sys Header: %d traits, %u Generations, %llux%llu, %lf Max\n",nTraits,nGen,Width,Height,Max);
#endif
  // Now that the header is read in, compute the minimum number of species
  // needed to call a niche "filled"
  min = NICHE_FILLED_MINIMUM*((Width*Height)/(nTraits))*Max;
  // Allocate generation structures
  ZALLOC(Gen,  nGen*sizeof(Generation));
  for(i=0; i<nGen; i++) {
    // Allocate patch data structures
    ALLOC(Gen[i].p,             Width*sizeof(patch*));
    for(j=0; j<Width; j++)
      ZALLOC(Gen[i].p[j],       Height*sizeof(patch));
    // Allocate preference matrix
    ALLOC(Gen[i].Prefs,         NICHES*sizeof(u64b*));
    for(j=0; j<NICHES; j++)
      ZALLOC(Gen[i].Prefs[j],   NICHES*sizeof(u64b));
#if _SPVY
    ALLOC(Gen[i].spvy,          ((int)(pow((double)(2),(double)(nTraits))))*sizeof(u64b*));
    for(j=0; j<(4*(nTraits)); j++)
      ZALLOC(Gen[i].spvy[j],    ((int)(4*nTraits))*sizeof(u64b));
#endif
#if _NEWRM
    ALLOC(Gen[i].newrm,         16*sizeof(u64b*));
    for(j=0; j<(4*(nTraits)); j++)
      ZALLOC(Gen[i].newrm[j],   4*sizeof(u64b));
#endif
    // Allocate niche frequency array
    ZALLOC(Gen[i].Nichef,       NICHES*sizeof(u64b));
    // Allocate species count array
    ZALLOC(Gen[i].ns,           NICHES*sizeof(int));
    // Allocate fille niche flags
    ZALLOC(Gen[i].filled,       NICHES*sizeof(char));
    ZALLOC(Gen[i].nss,          16*sizeof(int));
  }

  // Read system data
  for(i=0; i<nGen; i++) {
    // Read generation header
    if( fscanf(f, "gen: %llu %llu ", &Gen[i].g, &Gen[i].Popsize) != 2 ) {
      fprintf(stderr,"niche_view: sys: generation header error (%llu)\n",i);
      if( g == 1 ) {
        nGen = i;
        Clean = 0;
        fprintf(stderr,"niche_view: sys: trailer was not present; using (%llu) as nGen instead.\n",i); 
        break;
      } else {
        return 1;
      }
    }
    // If empty gen, skip it
    if(!Gen[i].Popsize)
      continue;
    // Read generation data
    if( fscanf(f, "%lf %lf %llu %lf %llu %lf %lf %lf %lf %lf ",
                &Gen[i].acp, &Gen[i].apr, &Gen[i].rMax, &Gen[i].cV, &Gen[i].rs,
                &Gen[i].cf, &Gen[i].cm, &Gen[i].pf, &Gen[i].pm, &Gen[i].amc)    != 10 )
      { fprintf(stderr,"niche_view: sys: generation error (%llu)\n",i); return 1; }
    // Check for amc >= .5
    if( (Amcg == -1) && (Gen[i].amc >= .5) ) {
      Amcg = Gen[i].g;
      Amcf = i;
    }
    // Read niche counts
    for(j=0; j<NICHES; j++) {
      if( fscanf(f, "%llu ", Gen[i].Nichef+j) != 1 )
        { fprintf(stderr,"niche_view: sys: niche count error (%llu)\n",i); return 1; }
      // Find max for scaling
      if(Gen[i].Nichef[j] > Gen[i].mNichef) Gen[i].mNichef = Gen[i].Nichef[j];
    }
    // Read most prefered fitness and migration
    if( fscanf(f, "%llu %llu ", &Gen[i].Mpnf, &Gen[i].Mpnm) != 2 )
      { fprintf(stderr,"niche_view: sys: niche pref error (%llu)\n",i); return 1; }
    // Read preference matrix
    for(j=0; j<NICHES; j++){
      for(k=0; k<NICHES; k++) {
        if( fscanf(f,"%llu ",&Gen[i].Prefs[j][k]) != 1 )
        { fprintf(stderr,"niche_view: sys: preference matrix error (%llu)\n",i); return 1; }
        // Find max to scale with
        if(Gen[i].Prefs[j][k] > Gen[i].mPref)
          Gen[i].mPref = Gen[i].Prefs[j][k];
      }
    }

    // Check to see if this data file contains a kpn matrix
    if( fscanf(f, "%d ", &KPN) != 1 )
      { fprintf(stderr,"niche_view: sys: kpn matrix flag error (%llu)\n",i); return 1; }
    if( KPN ) {
      // Read in the size of the KPN matrix
      if( fscanf(f, "%d %d ", &KPNx, &KPNy) != 2 )
      { fprintf(stderr,"niche_view: sys: kpn matrix size error (%llu)\n",i); return 1; }
      // Allocate space for this generation's KPN
      ALLOC(Gen[i].kpn, KPNx*sizeof(u64b));
      for(j=0; j<KPNx; j++)
        ALLOC(Gen[i].kpn[j], KPNy*sizeof(u64b));
      // Read in the kpn matrix
      for(j=0; j<KPNx; j++){
        for(k=0; k<KPNy; k++) {
          // Read value
          if( fscanf(f, "%llu ", &Gen[i].kpn[j][k]) != 1 )
          { fprintf(stderr,"niche_view: sys: kpn matrix error (%llu)\n",i); return 1; }
          // Check for max
          if(Gen[i].kpn[j][k] > Gen[i].mkpn)
            Gen[i].mkpn = Gen[i].kpn[j][k];
        }
      }

      // Now that the KPN matrix is built for this gen, I can use it to count the species for this gen
      for(j=Gen[i].fn=Gen[i].tns=0,nav=0,av=Gen[i].spfn=0.0; j<KPNx; j++) {
        // Check "filled" status
        if( Gen[i].Nichef[j] >= min ){
          // Count species in this niche
          for(k=Gen[i].ns[j]=0; k<KPNy; k++){
            // Check for density threshold -> count species
            m = Gen[i].mkpn;
            t = Gen[i].kpn[j][k]/m;
            // Contribute to average
            av  += ((double)Gen[i].kpn[j][k]) * (((double)k)/(KPNy-1.0));
            nav += Gen[i].kpn[j][k];
            if( !(k%2) && (t > SPECIES_K_MINIMUM) ) {
              // Check for local maximum
              lm = 1;
              if( (k > 1)      && ( (Gen[i].kpn[j][k-2]/m) >= t ) ) lm = 0;
              if( (k < KPNy-2) && ( (Gen[i].kpn[j][k+2]/m) >  t ) ) lm = 0;
              Gen[i].ns[j] += lm;
              Gen[i].tns   += lm;
            }
          }
          // Update per-niche counts
          Gen[i].filled[j] = 1;
          Gen[i].fn++;
          Gen[i].spfn += Gen[i].ns[j];
        } else {
          // Niche not filled
          Gen[i].filled[j] = 0;
        }
      }
      // Divide by number of filled niches to get spfn average
      if( Gen[i].fn ) Gen[i].spfn /= Gen[i].fn;
      // Now do a second pass to find variance
      av /= nav;
      for(j=0,t=0.0,nav=0; j<KPNx; j++) {
        for(k=0; k<KPNy; k++) {
          t   += Gen[i].kpn[j][k] * ((((double)k)/(KPNy-1.0))-av)*((((double)k)/(KPNy-1.0))-av);
          nav += Gen[i].kpn[j][k];
        }
      }
      Gen[i].vark = 2.0*sqrt(t/nav);
    }

    // Check to see if this data file contains a fpn matrix
    if( fscanf(f, "%d ", &FPN) != 1 )
      { fprintf(stderr,"niche_view: sys: fpn matrix flag error (%llu)\n",i); return 1; }
    if( FPN ) {
      // Read in the size of the FPN matrix
      if( fscanf(f, "%d %d ", &FPNx, &FPNy) != 2 )
        { fprintf(stderr,"niche_view: sys: fpn matrix size error (%llu)\n",i); return 1; }
      // Allocate space for this generation's FPN
      ALLOC(Gen[i].fpn, FPNx*sizeof(u64b));
      for(j=0; j<FPNx; j++)
        ALLOC(Gen[i].fpn[j], FPNy*sizeof(u64b));
      // Read in the fpn matrix
      for(j=0; j<FPNx; j++){
        for(k=0; k<FPNy; k++){
          // Read value
          if( fscanf(f, "%llu ", &Gen[i].fpn[j][k]) != 1 )
            { fprintf(stderr,"niche_view: sys: fpn matrix error (%llu)\n",i); return 1; }
          // Check for max
          if(Gen[i].fpn[j][k] > Gen[i].mfpn)
          Gen[i].mfpn = Gen[i].fpn[j][k];
        }
      }
    }

    // Check to see if this data file contains an MVK matrix
    if( fscanf(f, "%d ", &MVK) != 1 )
      { fprintf(stderr,"niche_view: sys: mvk matrix flag error (%llu)\n",i); return 1; }
    if( MVK ) {
      // Read in the size of the MVK matrix
      if( fscanf(f, "%d %d ", &MVKx, &MVKy) != 2 )
        { fprintf(stderr,"niche_view: sys: mvk matrix size error (%llu)\n",i); return 1; }
      // Allocate space for this generation's MVK
      ALLOC(Gen[i].mvk, MVKx*sizeof(u64b));
      for(j=0; j<MVKx; j++)
        ALLOC(Gen[i].mvk[j], MVKy*sizeof(u64b));
      // Read in the mvk matrix
      for(j=0; j<MVKx; j++){
        for(k=0; k<MVKy; k++) {
          // Read value
          if( fscanf(f, "%llu ", &Gen[i].mvk[j][k]) != 1 )
            { fprintf(stderr,"niche_view: sys: mvk matrix error (%llu)\n",i); return 1; }
          // Check for max
          if(Gen[i].mvk[j][k] > Gen[i].mmvk)
            Gen[i].mmvk = Gen[i].mvk[j][k];
        }
      }
    }

#if _SPVY
    if( fscanf(f, "%d ", &SPVY) != 1 )
    { fprintf(stderr,"niche_view: sys: new species/pref matrix flag error (%llu)\n",i); return 1; }
    if( SPVY ) {
      // Read in the size of the FPN matrix
      if( fscanf(f, "%d %d ", &SPVYx, &SPVYy) != 2 )
      { fprintf(stderr,"niche_view: sys: new species/pref matrix size error (%llu)\n",i); return 1; }
      for(j=0; j<SPVYx; j++){
        for(k=0; k<SPVYy; k++){
          // Read value
          if( fscanf(f, "%llu ", &Gen[i].spvy[j][k]) != 1 )
          { fprintf(stderr,"niche_view: sys: new species/pref matrix error (%llu)\n",i); return 1; }
        }
      }
    }
#endif
#if _NEWRM
    if( fscanf(f, "%d ", &NEWRM) != 1 )
    { fprintf(stderr,"niche_view: sys: new x trait/res matrix flag error (%llu)\n",i); return 1; }
    if( NEWRM ) {
      // Read in the size of the FPN matrix
      if( fscanf(f, "%d %d ", &NEWRMx, &NEWRMy) != 2 )
      { fprintf(stderr,"niche_view: sys: new x trait/res matrix size error (%llu)\n",i); return 1; }
      for(j=0; j<NEWRMx; j++){
        for(k=0; k<NEWRMy; k++){
          // Read value
          if( fscanf(f, "%llu ", &Gen[i].newrm[j][k]) != 1 )
          { fprintf(stderr,"niche_view: sys: new x trait/res matrix error (%llu)\n",i); return 1; }
          // Check for max
          if(Gen[i].newrm[j][k] > Gen[i].mnewrm)
            Gen[i].mnewrm = Gen[i].newrm[j][k];
        }
      }
    }
#endif
#if NEWFITNESS
    /* Read species and hybrid fitnesses*/
    if( fscanf(f, "%lf ", &Gen[i].sfit) != 1 )
    { fprintf(stderr,"niche_view: sys: species fitness error (turn NEWFITNESS on/off) (%llu)\n",i); return 1; }
#endif

    // Check to see if this data file contains a spn matrix
    if( fscanf(f, "%d ", &SPN) != 1 )
    { fprintf(stderr,"niche_view: sys: spn matrix flag error (%llu)\n",i); return 1; }
    if( SPN ) {
      // Read in the size of the SPN matrix
      if( fscanf(f, "%d %d ", &SPNx, &SPNy) != 2 )
      { fprintf(stderr,"niche_view: sys: spn matrix size error (%llu)\n",i); return 1; }
      // Allocate space for this generation's SPN
      ALLOC(Gen[i].spn, SPNx*sizeof(u64b));
      for(j=0; j<SPNx; j++)
        ALLOC(Gen[i].spn[j], SPNy*sizeof(u64b));
      // Read in the spn matrix
      for(j=0; j<SPNx; j++){
        for(k=0; k<SPNy; k++) {
          // Read value
          if( fscanf(f, "%llu ", &Gen[i].spn[j][k]) != 1 )
          { fprintf(stderr,"niche_view: sys: spn matrix error (%llu)\n",i); return 1; }
          // Check for max
          if(Gen[i].spn[j][k] > Gen[i].mspn)
            Gen[i].mspn = Gen[i].spn[j][k];
        }
      }

      //----------------------------------------------------------------------------------------------------
      // Now that the SPN matrix is built for this gen, I can use it to count the species for this gen
      m = Gen[i].mspn;
      if( m != 0 ){
        for(j=Gen[i].tnss=0,nav=0,av=Gen[i].sexsps=0.0; j<SPNx; j++) {
          // Count the sexual species in this ecological species
          for(k=Gen[i].nss[j]=0; k<SPNy; k++){
            // Check for density threshold -> count species
            t = Gen[i].spn[j][k]/m;
            // Contribute to average
            av   += ((double)Gen[i].spn[j][k]) * (((double)k)/(SPNy-1.0));
            nav += Gen[i].spn[j][k];
            if( !(k%2) && (t > 0.) ) {
              // Check for local maximum
              lm = 1;
              if( (k > 1)      && ( (Gen[i].spn[j][k-2]/m) >= t ) ) lm = 0;
              if( (k < SPNy-2) && ( (Gen[i].spn[j][k+2]/m) >  t ) ) lm = 0;
              Gen[i].nss[j] += lm;
              if( Gen[i].nss[j] > 1 && Tsexuals == 0 ) Tsexuals = i;
              if( Gen[i].nss[j] > 1 && Tss[j] == 0 )   Tss[j]   = i;
              if( Gen[i].nss[j] > 0 && Gen[i].nss[j] > Maxss[j] ) Maxss[j] = Gen[i].nss[j];
              Gen[i].tnss   += lm;
            }
          }
          Gen[i].sexsps += Gen[i].nss[j];
        }
      }
      //----------------------------------------------------------------------------------------------------
    }

    // Check to see if this data file contains a spn matrix
    if( fscanf(f, "%d ", &SFPN) != 1 )
    { fprintf(stderr,"niche_view: sys: sfpn matrix flag error (%llu)\n",i); return 1; }
    if( SFPN ) {
      // Read in the size of the SFPN matrix
      if( fscanf(f, "%d %d ", &SFPNx, &SFPNy) != 2 )
      { fprintf(stderr,"niche_view: sys: sfpn matrix size error (%llu)\n",i); return 1; }
      // Allocate space for this generation's SFPN
      ALLOC(Gen[i].sfpn, SFPNx*sizeof(u64b));
      for(j=0; j<SFPNx; j++)
        ALLOC(Gen[i].sfpn[j], SFPNy*sizeof(u64b));
      // Read in the sfpn matrix
      for(j=0; j<SFPNx; j++){
        for(k=0; k<SFPNy; k++) {
          // Read value
          if( fscanf(f, "%llu ", &Gen[i].sfpn[j][k]) != 1 )
          { fprintf(stderr,"niche_view: sys: sfpn matrix error (%llu)\n",i); return 1; }
          // Check for max
          if(Gen[i].sfpn[j][k] > Gen[i].msfpn)
            Gen[i].msfpn = Gen[i].sfpn[j][k];
        }
      }
    }

    // Read in the terminating newline
    fscanf(f,"\n");
  }
  // Return success
  return 0;
}

int ReadDeme(char *fn)
{
  u64b i,j,k,l,t,g;  FILE *f;  patch *p;  int traits;  u64b width,height;  double max;

  /* Open file and read the header */
  if (!(f = fopen(fn,"r")))
    { perror("niche_view: .deme-open"); return 2; }
  fseek(f,i=-1,SEEK_END);
  while( fgetc(f) != '\n') {
    fseek(f,--i,SEEK_END);
  }
  if( fscanf(f,"%llu", &g) != 1 ) {
    fprintf(stderr, "niche_view: deme: trailer error\n"); 
    fprintf(stderr, "niche_view: deme: using sys value (or guess) instead...\n"); 
    g = 1;
  } else {
    g = 0;
  }
  rewind(f);
  if (fscanf(f,"%d %lf %lf %llu %llu\n",&traits,&max,&_b,&width,&height) != 5) 
    { fprintf(stderr, "niche_view: deme: header error\n"); return 2; }
  if( (traits != nTraits) || (max != Max) || (width != Width) || (height != Height) )
    { fprintf(stderr, "niche_view: deme: deme file does not match sys file (header)\n"); return 2; }
  if(nTraits > 64) {
    fprintf(stderr, "niche_view: deme: _k too large.  (0 < _k < 64)\n");
    return 1;
  }
#ifdef VERBOSE
  fprintf(stderr,"Deme Header: %d traits, %u Generations, %llux%llu, %lf Max\n",nTraits,nGen,Width,Height,Max);
#endif

  /* Read deme data */
  for(i=0; i<nGen; i++) {
    // Read the generation "header" just to make sure no generation 'skew' is introduced.
    if( fscanf(f, "gen: ") == EOF ) {
      fprintf(stderr,"niche_view: deme: generation header error (%llu)\n",i); 
      if( g == 1 ) {
        fprintf(stderr,"niche_view: deme: sys ends at (%llu) but deme ends at (%llu)\n.",nGen,i); 
        fprintf(stderr,"niche_view: deme: setting nGen to (%llu)\n.",i); 
        nGen = i;
      } else {
        return 1;
      }
    }
    for(j=0; j<Width; j++) {
      for(k=0; k<Height; k++) {
        p = &Gen[i].p[j][k];
	/* Allocate needed data structures */
	ALLOC(p->l,      nTraits*sizeof(u64b));
	ALLOC(p->af,     nTraits*sizeof(u64b));
	ALLOC(p->am,     nTraits*sizeof(u64b));
	ALLOC(p->vf,     nTraits*sizeof(u64b));
	ALLOC(p->vm,     nTraits*sizeof(u64b));
	/* Read deme header */
	if( fscanf(f, "%llu %llu ", &l, &p->ni) != 2 )
	  { fprintf(stderr,"niche_view: deme: per-deme generation header error (%llu)\n",i); return 1; }
	/* Read this patch's stats */
	if( fscanf(f, "%llu %lf %lf %lf %lf %llu ", &p->pnf, &p->acp, &p->apr, &p->amf, &p->amm, &p->n) != 6 )
	  { fprintf(stderr,"niche_view: deme: deme data error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	/* Read niche traits */
	for(l=0; l<nTraits; l++) { 
	  if( fscanf(f, "%llu ", &t) != 1)
	    { fprintf(stderr,"niche_view: deme: trait error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	  p->l[l] = ((t)?(1):(0));
	}
	/* Read average traits */
	for(l=0; l<nTraits; l++) 
	  if( fscanf(f, "%lf ", &p->af[l]) != 1 )
	    { fprintf(stderr,"niche_view: deme: average xtrait error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	for(l=0; l<nTraits; l++) 
	  if( fscanf(f, "%lf ", &p->am[l]) != 1 )
	    { fprintf(stderr,"niche_view: deme: average ytrait error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	/* Read variance of traits */
	for(l=0; l<nTraits; l++) 
	  if( fscanf(f, "%lf ", &p->vf[l]) != 1 )
	    { fprintf(stderr,"niche_view: deme: variance xtrait error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	for(l=0; l<nTraits; l++) 
	  if( fscanf(f, "%lf ", &p->vm[l]) != 1 )
	    { fprintf(stderr,"niche_view: deme: variance ytrait error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	// Read mating and marker if _SEXUAL flag is set
	if( fscanf(f, "%llu ", &t) != 1 )
	  { fprintf(stderr,"niche_view: deme: _SEXUAL flag error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	Sexual = t;
	if( Sexual ) {
	  if( fscanf(f, "%lf ", &p->m) != 1 )
	    { fprintf(stderr,"niche_view: deme: mating error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	  if( fscanf(f, "%lf ", &p->k) != 1 )
	    { fprintf(stderr,"niche_view: deme: mating error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	  // Read female preference if _SEXUAL_SELECTION flag is set
	  if( fscanf(f, "%llu ", &t) != 1 )
	    { fprintf(stderr,"niche_view: deme: _SEXUAL_SELECTION flag error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	  Selection = t;
	  if( Selection ) {
	    if( fscanf(f, "%lf ", &p->f) != 1 )
	      { fprintf(stderr,"niche_view: deme: fpref error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
	  }
        }
        // Read species
        if( fscanf(f, "%llu ", &t) != 1 )
          { fprintf(stderr,"niche_view: deme: species ID flag error (%llu,[%llu,%llu])\n",i,j,k); return 1; }
        if( fscanf(f, "%llu ", &p->sid) != 1 )
        { fprintf(stderr,"niche_view: deme: deme species id and type data error (%llu,[%llu,%llu])\n",i,j,k); return 1; }

        // Read terminating newline
        fscanf(f, "\n");
      }
    }
  }

  // Return success
  return 0;
}

int ReadRange(char *fn)
{
  u64b i,l,g;  FILE *f;  rangest *r;  int traits;  u64b width,height;  double max;

  /* Open file and read the header */
  if (!(f = fopen(fn,"r")))
  { perror("niche_view: .range-open"); return 2; }
  fseek(f,i=-1,SEEK_END);
  while( fgetc(f) != '\n') {
    fseek(f,--i,SEEK_END);
  }
  if( fscanf(f,"%llu", &g) != 1 ) {
    fprintf(stderr, "niche_view: range: trailer error\n"); 
    fprintf(stderr, "niche_view: range: using sys value (or guess) instead...\n"); 
    g = 1;
  } else {
    g = 0;
  }
  rewind(f);
  if (fscanf(f,"%d %lf %lf %llu %llu\n",&traits,&max,&_b,&width,&height) != 5) 
  { fprintf(stderr, "niche_view: range: header error\n"); return 2; }
  if( (traits != nTraits) || (max != Max) || (width != Width) || (height != Height) )
  { fprintf(stderr, "niche_view: range: range file does not match sys file (header)\n"); return 2; }
  if(nTraits > 64) {
    fprintf(stderr, "niche_view: range: _k too large.  (0 < _k < 64)\n");
    return 1;
  }
#ifdef VERBOSE
  fprintf(stderr,"Range Header: %d traits, %u Generations, %llux%llu, %lf Max\n",nTraits,nGen,Width,Height,Max);
#endif

  // Read range data
  for(i=0; i<nGen; i++) {
    // Read the generation "header" just to make sure no generation 'skew' is introduced.
    if( fscanf(f, "gen: ") == EOF ) {
      fprintf(stderr,"niche_view: range: generation header error (%llu)\n",i); 
      if( g == 1 ) {
        fprintf(stderr,"niche_view: range: sys ends at (%llu) but rane ends at (%llu)\n.",nGen,i); 
        fprintf(stderr,"niche_view: range: setting nGen to (%llu)\n.",i); 
        nGen = i;
      } else {
        return 1;
      }
    }

    r = &(Gen[i].range);
    // Allocate needed data structures 
    ALLOC(r->idspecies, ((int)(pow((double)(2),(double)(nTraits))))*sizeof(int));
    ALLOC(r->noisolate, ((int)(pow((double)(2),(double)(nTraits))))*sizeof(int));
    ALLOC(r->dranges,   ((int)(pow((double)(2),(double)(nTraits))))*sizeof(int));
    ALLOC(r->mrange,    ((int)(pow((double)(2),(double)(nTraits))))*sizeof(int));
    ALLOC(r->meanr,     ((int)(pow((double)(2),(double)(nTraits))))*sizeof(u64b));
    ALLOC(r->sdevr,     ((int)(pow((double)(2),(double)(nTraits))))*sizeof(u64b));
    ALLOC(r->sir,       ((int)(pow((double)(2),(double)(nTraits))))*sizeof(u64b));
    ALLOC(r->shr,       ((int)(pow((double)(2),(double)(nTraits))))*sizeof(u64b));
    ALLOC(r->sppop,     ((int)(pow((double)(2),(double)(nTraits))))*sizeof(int));

    u64b g;


    // Read range header
    for(l=0; l<(((int)(pow((double)(2),(double)(nTraits))))+1); l++){
      //printf("\n--------l is %d/%d",l,((int)(pow((double)(2),(double)(nTraits)))));
      fscanf(f, "%llu ", &g);
      //printf("* gen is %lf", g);
      fscanf(f, "%d ",  &r->idspecies[l]);
      //printf("\n id %d ",r->idspecies[l]); 
      fscanf(f, "%d ",  &r->noisolate[l]);
      //printf("noiso %d ", r->noisolate[l]);
      fscanf(f, "%d ",  &r->dranges[l]);
      //printf("dr %d ", r->dranges[l]);
      fscanf(f, "%d ",  &r->mrange[l]);
      //printf("mr %d ", r->mrange[l]);
      fscanf(f, "%lf ", &r->meanr[l]);
      //printf("mean %lf ", r->meanr[l]);
      fscanf(f, "%lf ", &r->sdevr[l]);
      //printf("sd %lf ", r->sdevr[l]);
      fscanf(f, "%lf ", &r->sir[l]);
      //printf("sir %lf ", r->sir[l]);
      fscanf(f, "%lf ", &r->shr[l]);
      //printf("shr %lf ", r->shr[l]);
      fscanf(f, "%d ",  &r->sppop[l]);
      fscanf(f, "\n"); // Read terminating newline
      /*if( fscanf(f, "%d %d %d %d %lf %lf %lf %lf\n",  &r->idspecies[l], &r->noisolate[l],  
          &r->dranges[l], &r->mrange[l], &r->meanr[l], &r->sdevr[l], &r->sir[l], &r->shr[l]) != 8 )
      { fprintf(stderr,"niche_view: range: range error (%llu)\n",i); return 1; }*/
    }
  }

  // Return success
  return 0;
}

#if READ_HISTO
int ReadHisto(char *fn)
{
  u64b i,l,g;  FILE *f;  sphistst *hs;  int traits;  u64b width,height; double max;

  /* Open file and read the header */
  if (!(f = fopen(fn,"r")))                                     
  { perror("niche_view: .histo-open"); return 2; }
  fseek(f,i=-1,SEEK_END);
  while( fgetc(f) != '\n') {
    fseek(f,--i,SEEK_END);
  }
  if( fscanf(f,"%llu", &g) != 1 ) {
    fprintf(stderr, "niche_view: histo: trailer error\n"); 
    fprintf(stderr, "niche_view: histo: using sys value (or guess) instead...\n"); 
    g = 1;
  } else {
    g = 0;
  }
  rewind(f);
  if (fscanf(f,"%d %lf %lf %llu %llu\n",&traits,&max,&_b,&width,&height) != 5) 
  { fprintf(stderr, "niche_view: histo: header error\n"); return 2; }
  if( (traits != nTraits) || (max != Max) || (width != Width) || (height != Height) )
  { fprintf(stderr, "niche_view: histo: range file does not match sys file (header)\n"); return 2; }
  if(nTraits > 64) {
    fprintf(stderr, "niche_view: histo: _k too large.  (0 < _k < 64)\n");
    return 1;
  }
#ifdef VERBOSE
  fprintf(stderr,"Histo Header: %d traits, %u Generations, %llux%llu, %lf Max\n",nTraits,nGen,Width,Height,Max);
#endif

  /* Read histo data */
  for(i=0; i<nGen; i++) {

    // Read the generation "header" just to make sure no generation 'skew' is introduced.
    if( fscanf(f, "gen: ") == EOF ) {
      fprintf(stderr,"niche_view: histo: generation header error (%llu)\n",i); 
      if( g == 1 ) {
        fprintf(stderr,"niche_view: histo: sys ends at (%llu) but rane ends at (%llu)\n.",nGen,i); 
        fprintf(stderr,"niche_view: histo: setting nGen to (%llu)\n.",i); 
        nGen = i;
      } else {
        return 1;
      }
    }

    hs = &(Gen[i].sphist);
    // Allocate needed data structures 
    ALLOC(hs->sphno,   ((int)(pow((double)(2),(double)(nTraits))))*sizeof(int));
    ALLOC(hs->sphid,   ((int)(pow((double)(2),(double)(nTraits))))*sizeof(int));
    ALLOC(hs->sphsize, ((int)(pow((double)(2),(double)(nTraits))))*sizeof(int));

    u64b g;
    hs->msphsize = 0;

    // Read histo header
    for(l=0; l<((int)(pow((double)(2),(double)(nTraits)))); l++){
      fscanf(f, "%llu ",  &g);
      fscanf(f, "%d ",    &hs->sphno[l]);
      fscanf(f, "%d ",    &hs->sphid[l]);
      fscanf(f, "%d ",    &hs->sphsize[l]);
      if( hs->sphsize[l] > hs->msphsize) hs->msphsize = hs->sphsize[l];
      // Read terminating newline
      fscanf(f, "\n");
    }

  }

  // Return success
  return 0;
}
#endif

#if READ_HABITAT
int ReadHabitat(char *fn)
{
  u64b i;  FILE *f;  char b[1024]; habitatst *h; int lm,g=0;

  /* Open file and read the header */
  Clean = 1;
  if (!(f = fopen(fn,"r"))) {
    sprintf(b,"niche_view: .habitat-open: \"%s\"",fn);
    perror(b);
    return 1;
  }
  fseek(f,i=-1,SEEK_END);
  while( (lm=fgetc(f)) != '\n' ) {
    if( lm == EOF ) {
      fprintf(stderr,"niche_view: habitat data file looks VERY wrong..\n");
      return 3;
    }
    fseek(f,--i,SEEK_END);
  }
  if( fscanf(f,"%llu", &nGen) != 1 ) {
    fprintf(stderr, "niche_view: habitat: trailer error\n");
    fprintf(stderr, "niche_view: habitat: attempting to guess... (500) \n");
    nGen = 500;
    g = 1;
  }
  rewind(f);
  //if (fscanf(f,"%d %lf %lf %llu %llu\n",&nTraits,&Max,&_b,&Width,&Height) != 5) 
  //{ fprintf(stderr, "niche_view: habitat: header error\n"); return 1; }
  //if(nTraits > 64) {
  //  fprintf(stderr, "niche_view: habitat: _k too large.  (0 < _k < 64)\n");
  //  return 1;
  //}

//#ifdef VERBOSE
//  fprintf(stderr,"Habitat Header: %d traits, %u Generations, %llux%llu, %lf Max\n",nTraits,nGen,Width,Height,Max);
//#endif

  // Read habitat data
  for(i=0; i<nGen; i++) {
    // Read the generation "header" just to make sure no generation 'skew' is introduced.
    if( fscanf(f, "gen: ") == EOF ) {
      fprintf(stderr,"niche_view: habitat: generation header error (%llu)\n",i); 
      if( g == 1 ) {
        fprintf(stderr,"niche_view: habitat: sys ends at (%llu) but habitat ends at (%llu)\n.",nGen,i); 
        fprintf(stderr,"niche_view: habitat: setting nGen to (%llu)\n.",i); 
        nGen = i;
      } else {
        return 1;
      }
    }

    h = &(Gen[i].hab);
    // Allocate needed data structures 
    ALLOC(h->idpatch,      ((int)(nTraits))*sizeof(int));
    ALLOC(h->noisolpatch,  ((int)(nTraits))*sizeof(int));
    ALLOC(h->dpatchsize,   ((int)(nTraits))*sizeof(int));
    ALLOC(h->mpatchs,      ((int)(nTraits))*sizeof(int));
    ALLOC(h->meanpatchs,   ((int)(nTraits))*sizeof(u64b));
    ALLOC(h->sdevp,        ((int)(nTraits))*sizeof(u64b));
    ALLOC(h->sip,          ((int)(nTraits))*sizeof(u64b));
    ALLOC(h->shp,          ((int)(nTraits))*sizeof(u64b));

    u64b g;
    int l;

    fscanf(f, "%llu ", &g);
    // Read habitat header
    for(l=0; l<((int)(nTraits)); l++){
      //printf("\n--------l is %d/%d",l,((int)(pow((double)(2),(double)(nTraits)))));
      fscanf(f, "%d ",  &h->idpatch[l]);
      //printf("\n id %d ",r->idspecies[l]); 
      fscanf(f, "%d ",  &h->noisolpatch[l]);
      //printf("noiso %d ", r->noisolate[l]);
      fscanf(f, "%d ",  &h->dpatchsize[l]);
      //printf("dr %d ", r->dranges[l]);
      fscanf(f, "%d ",  &h->mpatchs[l]);
      //printf("mr %d ", r->mrange[l]);
      fscanf(f, "%lf ", &h->meanpatchs[l]);
      //printf("mean %lf ", r->meanr[l]);
      fscanf(f, "%lf ", &h->sdevp[l]);
      //printf("sd %lf ", r->sdevr[l]);
      fscanf(f, "%lf ", &h->sip[l]);
      //printf("sir %lf ", r->sir[l]);
      fscanf(f, "%lf ", &h->shp[l]);
      //printf("shr %lf ", r->shr[l]); 
    }
    fscanf(f, "\n"); // Read terminating newline

    fscanf(f, "%d ",  &h->check);
    fscanf(f, "%d ",  &h->tcount);
    fscanf(f, "%d ",  &h->maxp);
    fscanf(f, "%lf ", &h->meanp);
    fscanf(f, "%lf ", &h->tsd);
    fscanf(f, "%lf ", &h->tsip);
    fscanf(f, "%lf ", &h->tshp);
    fscanf(f, "%lf ", &h->frag);
    fscanf(f, "\n"); // Read terminating newline

  }

  // Return success
  return 0;
}
#endif

int readDataFile(char *fn)
{
  char sys[1024], deme[1024], range[1024];  int err;
#if READ_HISTO
  char histo[1024];
#endif
#if READ_HABITAT
  char habitat[1024];
#endif
  // fn is really just a file prefix, so build 2 real file names
  sprintf(sys,    "%s.sys",     fn);
  sprintf(deme,   "%s.deme",    fn);
  sprintf(range,  "%s.range",   fn);
#if READ_HISTO
  sprintf(histo,  "%s.histo",   fn);
#endif
#if READ_HABITAT
  sprintf(habitat,"%s.habitat", fn);
#endif
  // Read in system and then deme data
  if( (err=ReadSys(sys))   )        return err;
  if( (err=ReadDeme(deme)) )        return err;
  if( (err=ReadRange(range)) )      return err;
#if READ_HISTO
  if( (err=ReadHisto(histo)) )      return err;
#endif
#if READ_HABITAT
  if( (err=ReadHabitat(habitat)) )  return err;
#endif
  // Return success
  return 0;
}

/***************************************************************************************
 * Util functions used by drawing code
 ***************************************************************************************/

static void InitGLWindow(GLWindow *glw)
{
  glShadeModel(GL_SMOOTH);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClearDepth(1.0f);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if PRESENTATION_MODE
  glw->font = BuildFont(glw, "-adobe-helvetica-bold-r-normal-*-*-180-*-*-p-*-iso10646-1");
#else
  glw->font = BuildFont(glw, "fixed");   
#endif
  ResizeGLScene(glw->width, glw->height);
  glFlush();
}

#if PRESENTATION_MODE
Byte red(double w)
{
  if (w < 0.) w = 0.;
  if (w > 1.) w = 1.;
  return simple[(int)(w*7+.5)][0];
}

Byte green(double w)
{
  if (w < 0.) w = 0.;
  if (w > 1.) w = 1.;
  return simple[(int)(w*7.5)][1];
}

Byte blue(double w)
{
  if (w < 0.) w = 0.;
  if (w > 1.) w = 1.;
  return simple[(int)(w*7.5)][2];
}
#else
Byte red(double w)
{
  if (w < 0.) w = 0.;
  if (w > 1.) w = 1.;
  return RB[(int)(.5+77*(1.-w))][0];
}

Byte green(double w)
{
  if (w < 0.) w = 0.;
  if (w > 1.) w = 1.;
  return RB[(int)(.5+77*(1.-w))][1];
}

Byte blue(double w)
{
  if (w < 0.) w = 0.;
  if (w > 1.) w = 1.;
  return RB[(int)(.5+77*(1.-w))][2];
}
#endif

Byte toByte(double x)
{
  if (x <   0) return 0;
  if (x > 255) return 255;
  return (unsigned char) x;
}

void glOutlineCircle(int x, int y, double r)
{
  int k;

  glBegin(GL_LINE_LOOP);
  for(k=0;k<32;k++)
    glVertex2d(x+r*cos(k*2*PI/32),y+r*sin(k*2*PI/32));
  glEnd();
}

void glCircle(int x, int y, double r)
{
  int k;

  glBegin(GL_POLYGON);
  for(k=0;k<32;k++)
    glVertex2d(x+r*cos(k*2*PI/32),y+r*sin(k*2*PI/32));
  glEnd();
}

/***************************************************************************************
 * Components of the graph
 ***************************************************************************************/

void w()
{
  glColor3ub(255,255,255);
}

void y()
{
  glColor3ub(255,255,0);
}

void DrawTextStats(GLWindow *glw, float tx, float ty)
{
  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  // Draw
  w(); glRasterPos2f(  0.0f,  20.0f); printGLf(glw->font,"Speed:");
  y(); glRasterPos2f(100.0f,  20.0f); printGLf(glw->font,"%d",Speed);

  w(); glRasterPos2f(  0.0f,  40.0f); printGLf(glw->font,"Generation:");
  y(); glRasterPos2f(100.0f,  40.0f); printGLf(glw->font,"%llu (%d/%llu frames)",Gen[Frame-1].g, Frame-1, nGen-1);

  w(); glRasterPos2f(  0.0f,  60.0f); printGLf(glw->font,"Av Fitness:");
  y(); glRasterPos2f(100.0f,  60.0f); printGLf(glw->font,"%.1lf%c", (double)(Gen[Frame-1].acp*100),'%');
#if NEWFITNESS
  w(); glRasterPos2f(  0.0f,  80.0f); printGLf(glw->font,"Species Fitness:");
  y(); glRasterPos2f(100.0f,  80.0f); printGLf(glw->font,"%.1lf%c", (double)(Gen[Frame-1].sfit*100),'%');
#endif
  w(); glRasterPos2f(  0.0f, 100.0f); printGLf(glw->font,"Pref:");
  y(); glRasterPos2f(100.0f, 100.0f); printGLf(glw->font,"%.1lf%c", (double)(Gen[Frame-1].apr*100),'%');

  w(); glRasterPos2f(  0.0f, 120.0f); printGLf(glw->font,"Choosiness:");
  y(); glRasterPos2f(100.0f, 120.0f); printGLf(glw->font,"%.4lf",Gen[Frame-1].amc);

  w(); glRasterPos2f(  0.0f, 140.0f); printGLf(glw->font,"Resources used:");
  y(); glRasterPos2f(100.0f, 140.0f); printGLf(glw->font,"%d",Gen[Frame-1].fn);

  w(); glRasterPos2f(  0.0f, 160.0f); printGLf(glw->font,"T(sexual sp):");
  y(); glRasterPos2f(100.0f, 160.0f); printGLf(glw->font,"%d",Tsexuals);

  /*
  w(); glRasterPos2f(  0.0f,  60.0f); printGLf(glw->font,"MaxPopSize:");
  y(); glRasterPos2f(100.0f,  60.0f); printGLf(glw->font,"%.1f%c (%llu/%llu)",((double)Gen[Frame-1].rMax*100)/(Max/_b),'%',Gen[Frame-1].rMax,(u64b)(Max/_b));

  w(); glRasterPos2f(  0.0f,  80.0f); printGLf(glw->font,"SysSize:");
  y(); glRasterPos2f(100.0f,  80.0f); printGLf(glw->font,"%.1f%c (%llu/%llu)",((double)Gen[Frame-1].Popsize*100)/(Width*Height*(Max/_b)),'%',Gen[Frame-1].Popsize,(u64b)(Width*Height*(Max/_b)));

  if( Gen[Frame-1].cV != -1 ) {
    w(); glRasterPos2f(  0.0f, 140.0f); printGLf(glw->font,"Cramer's V:");
    y(); glRasterPos2f(100.0f, 140.0f); printGLf(glw->font,"%.1f%c", (double)(Gen[Frame-1].cV*100),'%');
  } else {
    w(); glRasterPos2f(  0.0f, 140.0f); printGLf(glw->font,"Cramer's V:");
    y(); glRasterPos2f(100.0f, 140.0f); printGLf(glw->font,"n/a");
  }

  w(); glRasterPos2f(250.0f,  20.0f); printGLf(glw->font,"AvMatch (fitn):");
  y(); glRasterPos2f(350.0f,  20.0f); printGLf(glw->font,"%.1lf%c", (double)(100*Gen[Frame-1].acp),'%'); // originally .cf

  w(); glRasterPos2f(250.0f,  40.0f); printGLf(glw->font,"AvMatch (pref):");
  y(); glRasterPos2f(350.0f,  40.0f); printGLf(glw->font,"%.1lf%c", (double)(100*Gen[Frame-1].apr),'%'); // originally .cm

  w(); glRasterPos2f(  0.0f, 160.0f); printGLf(glw->font,"Purebred:");
  y(); glRasterPos2f(100.0f, 160.0f); printGLf(glw->font,"%.1f%c", (double)(Gen[Frame-1].rs)/(Gen[Frame-1].Popsize)*100,'%');

  w(); glRasterPos2f(250.0f,  80.0f); printGLf(glw->font,"Sexual Species:");
  y(); glRasterPos2f(350.0f,  80.0f); printGLf(glw->font,"%d",Gen[Frame-1].tns);

  w(); glRasterPos2f(250.0f, 100.0f); printGLf(glw->font,"KD Var:");
  y(); glRasterPos2f(350.0f, 100.0f); printGLf(glw->font,"%.4lf",Gen[Frame-1].vark);

  w(); glRasterPos2f(250.0f, 140.0f); printGLf(glw->font,"Spcs/FN:");
  y(); glRasterPos2f(350.0f, 140.0f); printGLf(glw->font,"%.4lf",Gen[Frame-1].spfn);
  */
}

#if READ_HABITAT
void DrawHabitatStats(GLWindow *glw, float tx, float ty)
{
  //int l;
  double amm;
  //habitatst hab;
  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  amm = ((double)(Gen[Frame-1].hab.mpatchs[0] + Gen[Frame-1].hab.mpatchs[1] + Gen[Frame-1].hab.mpatchs[2] + Gen[Frame-1].hab.mpatchs[3]))/((double)(nTraits));

  Gen[Frame-1].hab.am = amm;

  // Draw Column 1
  //w(); glRasterPos2f(200.0f, 20.0f); printGLf(glw->font,"Resources:");
  //y(); glRasterPos2f(290.0f, 20.0f); printGLf(glw->font,"%d ",nTraits);

  w(); glRasterPos2f(200.0f,  20.0f); printGLf(glw->font,"Landscape s:");
  y(); glRasterPos2f(300.0f,  20.0f); printGLf(glw->font,"%d ",Gen[Frame-1].hab.check);

  //w(); glRasterPos2f(270.0f,  60.0f); printGLf(glw->font,"D of p sizes:");
  //y(); glRasterPos2f(360.0f,  60.0f); printGLf(glw->font,"%d ",Gen[Frame-1].hab.tcount);

  //w(); glRasterPos2f(270.0f,  80.0f); printGLf(glw->font,"Max:");
  //y(); glRasterPos2f(360.0f,  80.0f); printGLf(glw->font,"%d ",Gen[Frame-1].hab.maxp);

  //w(); glRasterPos2f(270.0f, 100.0f); printGLf(glw->font,"Mean:");
  //y(); glRasterPos2f(360.0f, 100.0f); printGLf(glw->font,"%.2f ",Gen[Frame-1].hab.meanp);

  //w(); glRasterPos2f(270.0f, 120.0f); printGLf(glw->font,"SD:");
  //y(); glRasterPos2f(360.0f, 120.0f); printGLf(glw->font,"%.2f ",Gen[Frame-1].hab.tsd);

  w(); glRasterPos2f(200.0f, 40.0f); printGLf(glw->font,"Simpson's EI:");
  y(); glRasterPos2f(300.0f, 40.0f); printGLf(glw->font,"%.2f ",Gen[Frame-1].hab.tsip);

  w(); glRasterPos2f(200.0f, 60.0f); printGLf(glw->font,"Shannon's EI:");
  y(); glRasterPos2f(300.0f, 60.0f); printGLf(glw->font,"%.2f ",Gen[Frame-1].hab.tshp);

  w(); glRasterPos2f(200.0f, 80.0f); printGLf(glw->font,"Percent edges:");
  y(); glRasterPos2f(300.0f, 80.0f); printGLf(glw->font,"%.2f ",Gen[Frame-1].hab.frag);

  //w(); glRasterPos2f(270.0f, 200.0f); printGLf(glw->font,"Av. Max'es:");
  //y(); glRasterPos2f(360.0f, 200.0f); printGLf(glw->font,"%.2f ", Gen[Frame-1].hab.am);

  w(); glRasterPos2f(200.0f, 100.0f); printGLf(glw->font,"Max/Mean(ID):");
  y(); glRasterPos2f(300.0f, 100.0f); printGLf(glw->font,"%d / %.2f (%d)", Gen[Frame-1].hab.mpatchs[0], Gen[Frame-1].hab.meanpatchs[0], Gen[Frame-1].hab.idpatch[0]+1);

  w(); glRasterPos2f(200.0f, 120.0f); printGLf(glw->font,"Max/Mean(ID):");
  y(); glRasterPos2f(300.0f, 120.0f); printGLf(glw->font,"%d / %.2f (%d)",Gen[Frame-1].hab.mpatchs[1], Gen[Frame-1].hab.meanpatchs[1], Gen[Frame-1].hab.idpatch[1]+1);

  w(); glRasterPos2f(200.0f, 140.0f); printGLf(glw->font,"Max/Mean(ID):");
  y(); glRasterPos2f(300.0f, 140.0f); printGLf(glw->font,"%d / %.2f (%d)", Gen[Frame-1].hab.mpatchs[2], Gen[Frame-1].hab.meanpatchs[2], Gen[Frame-1].hab.idpatch[2]+1);

  w(); glRasterPos2f(200.0f, 160.0f); printGLf(glw->font,"Max/Mean(ID):");
  y(); glRasterPos2f(300.0f, 160.0f); printGLf(glw->font,"%d / %.2f (%d)", Gen[Frame-1].hab.mpatchs[3], Gen[Frame-1].hab.meanpatchs[3], Gen[Frame-1].hab.idpatch[3]+1);

}
#endif

void DrawSpeciesLegend(GLWindow *glw, float tx, float ty)
{
  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  // Draw Column 1
  w(); glRasterPos2f(  0.0f,  20.0f); printGLf(glw->font,"Specialists:");
  glColor3ub(255,   0,   0);  glRasterPos2f(  0.0f,  40.0f); printGLf(glw->font,"Species 1");
  glColor3ub(255,   0, 255);  glRasterPos2f(  0.0f,  60.0f); printGLf(glw->font,"Species 2");
  glColor3ub(100,   0, 100);  glRasterPos2f(  0.0f,  80.0f); printGLf(glw->font,"Species 3");
  glColor3ub(255, 100,   0);  glRasterPos2f(  0.0f, 100.0f); printGLf(glw->font,"Species 4");

  w(); glRasterPos2f(  0.0f, 130.0f); printGLf(glw->font,"Generalists (2-r):");
  glColor3ub(  0,   0, 255);  glRasterPos2f(  0.0f, 150.0f); printGLf(glw->font,"Species 5");
  glColor3ub(  0, 100, 255);  glRasterPos2f(  0.0f, 170.0f); printGLf(glw->font,"Species 6");
  glColor3ub( 75,  75, 255);  glRasterPos2f(  0.0f, 190.0f); printGLf(glw->font,"Species 7");
  glColor3ub(  0, 180, 255);  glRasterPos2f(  0.0f, 210.0f); printGLf(glw->font,"Species 8");
  glColor3ub(  0, 255, 255);  glRasterPos2f(  0.0f, 230.0f); printGLf(glw->font,"Species 9");
  glColor3ub(100,   0, 255);  glRasterPos2f(  0.0f, 250.0f); printGLf(glw->font,"Species 10");

  // Draw Column 2
  w(); glRasterPos2f(120.0f,  20.0f); printGLf(glw->font,"Generalists (3-r):");
  glColor3ub(  0, 100,   0);  glRasterPos2f(120.0f,  40.0f); printGLf(glw->font,"Species 11");
  glColor3ub(  0, 200,  55);  glRasterPos2f(120.0f,  60.0f); printGLf(glw->font,"Species 12");
  glColor3ub(  0, 255,   0);  glRasterPos2f(120.0f,  80.0f); printGLf(glw->font,"Species 13");
  glColor3ub( 50, 130,   0);  glRasterPos2f(120.0f, 100.0f); printGLf(glw->font,"Species 14");

  w(); glRasterPos2f(120.0f, 130.0f); printGLf(glw->font,"Generalists (4-r):");
  glColor3ub(255, 255, 255);  glRasterPos2f(120.0f, 150.0f); printGLf(glw->font,"Species 15");

}


void DrawDemeGraph(GLWindow *glw, float tx, float ty)
{
  int i,j;  double t,c;

  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  // Draw the background patch character
  glBegin(GL_QUADS);
  for(i=0; i<Width; i++) {
    for(j=0; j<Height; j++) {

      t = ((double)Gen[Frame-1].p[i][j].n)/(NICHES-1);
#if BLACK_WHITE
      if( ((int)((double)(t)*((double)(NICHES-1))) == 0) ) glColor3ub(232,220,232);
      if( ((int)((double)(t)*((double)(NICHES-1))) == 1) ) glColor3ub(202,190,202);
      if( ((int)((double)(t)*((double)(NICHES-1))) == 2) ) glColor3ub(172,160,172);
      if( ((int)((double)(t)*((double)(NICHES-1))) == 3) ) glColor3ub(142,130,142);
#else
      glColor3ub(toByte(red  (t)*.3), toByte(green(t)*.3), toByte(blue (t)*.3));
#endif
      // t = ((double)Gen[Frame-1].p[i][j].n);//this is old
      // printf("\n %f", t);
#if PRESENTATION_MODE
      glColor3ub(toByte(red  (t)), toByte(green(t)), toByte(blue (t)));
#endif
      glVertex2i(i*Scale,           j*Scale);
      glVertex2i(i*Scale,           (j+1)*Scale);
      glVertex2i(i*Scale+Scale,     (j+1)*Scale);
      glVertex2i(i*Scale+Scale,     j*Scale);

#if PRETTY_PS
      if(PSFile1) {
        PS_Color(PSFile1, toByte(red(t)*.3)/255., toByte(green(t)*.3)/255., toByte(blue(t)*.3)/255.);
        PS_Box(PSFile1, i/2., j/2., .5, .5, 1);
      }
#endif

    }
  }
  glEnd();

  // Draw the patch size circle and outline
  for(i=0; i<Width && Gen[Frame-1].Popsize; i++) {
    for(j=0; j<Height; j++) {
      // Skip empty patches
      if( !Gen[Frame-1].p[i][j].ni ) continue;

#if SCALING
      t = Gen[Frame-1].p[i][j].ni/((double)Gen[Frame-1].rMax);
#else
      t = Gen[Frame-1].p[i][j].ni/((Gen[Frame-1].rMax<Max/_b)?(Max/_b):((double)Gen[Frame-1].rMax));
#endif

#if BLACK_WHITE
      c = Gen[Frame-1].p[i][j].sid;
      if( t >= 0 ){
      if( (((int)(c)) == 0) )  glColor3ub( 50,  50,  50);
      if( (((int)(c)) == 1) )  glColor3ub(255,   0,   0);
      if( (((int)(c)) == 2) )  glColor3ub(255,   0, 255);
      if( (((int)(c)) == 3) )  glColor3ub(  0,   0, 255);
      if( (((int)(c)) == 4) )  glColor3ub(100,   0, 100);
      if( (((int)(c)) == 5) )  glColor3ub(  0, 100, 255);
      if( (((int)(c)) == 6) )  glColor3ub( 75,  75, 255);
      if( (((int)(c)) == 7) )  glColor3ub(  0, 100,   0);
      if( (((int)(c)) == 8) )  glColor3ub(255, 100,   0);
      if( (((int)(c)) == 9) )  glColor3ub(  0, 180, 255);
      if( (((int)(c)) == 10) ) glColor3ub(  0, 255, 255);
      if( (((int)(c)) == 11) ) glColor3ub(  0, 200,  55);
      if( (((int)(c)) == 12) ) glColor3ub(100,   0, 255);
      if( (((int)(c)) == 13) ) glColor3ub(  0, 255,   0);
      if( (((int)(c)) == 14) ) glColor3ub( 50, 130,   0);
      if( (((int)(c)) == 15) ) glColor3ub(255, 255, 255);
      if( (((int)(c)) > 15) )  glColor3ub( 50,  50,  50);
      glCircle(i*Scale+Scale*.5,j*Scale+Scale*.5,sqrt((Scale*.5)*(Scale*.5)*t));
      glOutlineCircle(i*Scale+Scale*.5,j*Scale+Scale*.5,sqrt((Scale*.5)*(Scale*.5)*t));
      }

#else
      if( ColorMode == COLOR_PREF ) {
        // Circle colors are based on niche preference
        c = Gen[Frame-1].p[i][j].pnf/((double)(NICHES-1));
#if PRESENTATION_MODE
        glColor3ub(0,0,0);
        glCircle(i*Scale+Scale*.5,j*Scale+Scale*.5,(Scale*sqrt(t))*.5);
        glColor3ub(toByte(red  (c)), toByte(green(c)), toByte(blue (c)));      
        if((Scale*t)*.5-2 > 1)
          glCircle(i*Scale+Scale*.5,j*Scale+Scale*.5,(Scale*sqrt(t))*.5-2);
#else
        glColor3ub(toByte(red  (c)*.3), toByte(green(c)*.3), toByte(blue (c)*.3));
        if( t >= 0 )
          glCircle(i*Scale+Scale*.5,j*Scale+Scale*.5,sqrt((Scale*.5)*(Scale*.5)*t));
        glColor3ub(toByte(red  (c)), toByte(green(c)), toByte(blue (c)));
        if( t >= 0 )
          glOutlineCircle(i*Scale+Scale*.5,j*Scale+Scale*.5,sqrt((Scale*.5)*(Scale*.5)*t));      
#endif

#if PRETTY_PS
        if(PSFile1) {
          PS_Color(PSFile1, toByte(red(c)*.3)/255., toByte(green(c)*.3)/255., toByte(blue(c)*.3)/255.);
          if( t >= 0 )
            PS_Circle(PSFile1, i/2.+.25, j/2.+.25, sqrt(t)*.25, 1);
          PS_Color(PSFile1, toByte(red(c))/255., toByte(green(c))/255., toByte(blue(c))/255.);
          if( t >= 0 )
            PS_Circle(PSFile1, i/2.+.25, j/2.+.25, sqrt(t)*.25, 0);
        }
#endif
      } else {
        // Circle color is based on marker trait, outline is still preference
        if( !Filter[Gen[Frame-1].p[i][j].n] ) {
          // Only draw if not filtered
          c = Gen[Frame-1].p[i][j].k;
          glColor3f(c, c, c);
          if( t >= 0 )
            glCircle(i*Scale+Scale*.5,j*Scale+Scale*.5,sqrt((Scale*.5)*(Scale*.5)*t));
          c = Gen[Frame-1].p[i][j].pnf/((double)(NICHES-1));
          glColor3ub(toByte(red  (c)), toByte(green(c)), toByte(blue (c)));
          if( t >= 0 )
            glOutlineCircle(i*Scale+Scale*.5,j*Scale+Scale*.5,sqrt((Scale*.5)*(Scale*.5)*t));
        }
      }
#endif
    }
  }

  // Outline
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(0,0);
  glVertex2i(Width*Scale,0);
  glVertex2i(Width*Scale,Height*Scale);
  glVertex2i(0,Height*Scale);
  glEnd();
}


////////////////////////////////////////////////////////////////////////////////
// Within-Deme graph
//
// This gives a surface graph of a property of a single deme, where the x-axis
// and the z-axis are environment height and width, respectively.
////////////////////////////////////////////////////////////////////////////////

void Draw3DDeme(GLWindow *glw)
{
  u32b x,y;
  //char title[64];
  patch *p;
  int i;

  ////////////////////////////////////////////////////////////
  // 3D trait cluster view
  ////////////////////////////////////////////////////////////

  // Outline widget border
  Yellow();
  glBegin(GL_LINE_LOOP);
  glVertex2f(0.0f,0.0f);
  glVertex2f(0.0f,1.0f);
  glVertex2f(1.0f,1.0f);
  glVertex2f(1.0f,0.0f);
  glEnd();

  // Draw title
  //glRasterPos2f(0.5f-((strlen("traits")*6.0f/2.0f)/Plot.w),0.9f);
  //printGLf(glw->font,"traits");
  //sprintf(title,"Min: %d",Plot.spctrshld);
  //glRasterPos2f(0.5f-((strlen(title)*6.0f/2.0f)/Plot.w),0.95f);
  //printGLf(glw->font,title);

  // Switch over to a custom viewport "mode" which will let
  // us draw with our own perspective within our widget boundries.
  ViewPort3D(Plot.x, glw->height-Plot.y-Plot.h, Plot.w, Plot.h);

  // Rotate and translate to position 0.5 at the origin.
  // This allows all further drawing to be done [0,1], which,
  // IMHO, is very nice to work with.
  // glTranslatef(x,yf,z);                                        // Move graph "object" up/down on screen
  glRotatef(-55.0f+Plot.rot.x+(Plot.ny-Plot.oy),1.0f,0.0f,0.0f);  // Tilt back (rotate on graph's x axis)
  glRotatef(Plot.rot.z+(Plot.nx-Plot.ox),0.0f,0.0f,1.0f);         // Rotate on graph's y-axis
  glTranslatef(-0.5f,-0.5f,-0.0f);                                // Position .5 at origin
  glScalef(1.0f,1.0f,0.75f);

  // Outline bottom and top of heightmap
  glColor3f(0.3f, 0.3f, 0.3f);
  glEnable(GL_LINE_SMOOTH);
  glBegin(GL_LINE_LOOP); 
  glVertex2f(0.0f,0.0f);
  glVertex2f(0.0f,1.0f);
  glVertex2f(1.0f,1.0f);
  glVertex2f(1.0f,0.0f);
  glEnd();
  glBegin(GL_LINE_LOOP);
  glVertex3f(0.0f,0.0f,1.0f);
  glVertex3f(0.0f,1.0f,1.0f);
  glVertex3f(1.0f,1.0f,1.0f);
  glVertex3f(1.0f,0.0f,1.0f);
  glEnd();
  glDisable(GL_LINE_SMOOTH);

  //
  // 3-d plot
  //
  // Draw the plot
  glColor3f(1.0f, 0.0f, 1.0f);
  glPointSize(3.0f);
  glEnable(GL_POINT_SMOOTH);
  glBegin(GL_POINTS);
  for(i=0; i<nGen; i++) {
    for(x=0; x<Width; x++) {
      for(y=0; y<Height; y++) {
        p = &Gen[Frame-1].p[x][y];
        // Color based on fourth trait
        glColor3f(0.0f, p->af[3], 1.0f-p->af[3]);
        // Position is based on the three other traits
        glVertex3f(p->af[0], p->af[1], p->af[2]);
      }
    }
  }

  glEnd();
  glPointSize(1.0f);
  glDisable(GL_POINT_SMOOTH);

  // Label the four corners
  White();
  glRasterPos3f(0.0f,0.0f,0.1f); printGLf(glw->font,"(0,0,0)");
  glRasterPos3f(0.0f,1.0f,0.1f); printGLf(glw->font,"(0,1,0)");
  glRasterPos3f(1.0f,1.0f,0.1f); printGLf(glw->font,"(1,1,0)");
  glRasterPos3f(1.0f,0.0f,0.1f); printGLf(glw->font,"(1,0,0)");

  // Restore 2d viewport for other widgets
  ViewPort2D(glw);
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

#define HS (((Scale-HackScale)<0)?(0):(Scale-HackScale))

void DrawNicheHistogram(GLWindow *glw, float tx, float ty)
{
  int i; double t,c;

  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  // The outline for the histogram should be drawn first
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(-1,0);
  glVertex2i(NICHES*(20+HS)+1,0);
  glVertex2i(NICHES*(20+HS)+1,100);
  glVertex2i(-1,100);
  glEnd();

  // Draw our "niche histogram"
  for(i=0; i<NICHES && Gen[Frame-1].Popsize; i++) {
    t = ((double)i)/(NICHES-1);
#if BLACK_WHITE
    if( ((int)((double)(t)*((double)(NICHES-1))) == 0) ) glColor3ub(232,220,232);
    if( ((int)((double)(t)*((double)(NICHES-1))) == 1) ) glColor3ub(202,190,202);
    if( ((int)((double)(t)*((double)(NICHES-1))) == 2) ) glColor3ub(172,160,172);
    if( ((int)((double)(t)*((double)(NICHES-1))) == 3) ) glColor3ub(142,130,142);
#else
    glColor3ub(toByte(red  (t)*.3), toByte(green(t)*.3), toByte(blue (t)*.3));
#endif
#if PRESENTATION_MODE
    glColor3ub(toByte(red  (t)*.7), toByte(green(t)*.7), toByte(blue (t)*.7));

#endif
    glBegin(GL_QUADS);
    glVertex2i(i*(20+HS),100-(((double)Gen[Frame-1].Nichef[i])/Gen[Frame-1].mNichef)*99);
    glVertex2i(i*(20+HS),99);
    glVertex2i((i+1)*(20+HS),99);
    glVertex2i((i+1)*(20+HS),100-(((double)Gen[Frame-1].Nichef[i])/Gen[Frame-1].mNichef)*99);
    glEnd();    
    // Draw a tick mark to represent the proportion of "real" species
#if BLACK_WHITE
    glColor3ub(60,60,60);
#else
    glColor3ub(toByte(red  (t)), toByte(green(t)), toByte(blue (t)));
#endif

#if PRESENTATION_MODE
    glColor3ub(toByte(red  (1-t)), toByte(green(1-t)), toByte(blue (1-t)));

#endif
    c = (((double)Gen[Frame-1].Prefs[i][i]) / Gen[Frame-1].mPref) * 100;
    glBegin(GL_LINES);
    glVertex2i(i*(20+HS),100-c);
    glVertex2i((i+1)*(20+HS),100-c);
    glEnd();
    if( i == Gen[Frame-1].Mpnf ) { 
      // Draw a marker on the bar which represents the most prefered niche (fitness)
#if PRESENTATION_MODE
      glRasterPos2f(i*(20+HS)+20, 90.0f);
#else
      glRasterPos2f(i*(20+HS)+13, 90.0f);
#endif
      printGLf(glw->font,"fi");
    }
    if( i == Gen[Frame-1].Mpnm ) {
      // Draw a marker on the bar which represents the most prefered niche (migration)
#if PRESENTATION_MODE
      glRasterPos2f(i*(20+HS)+3, 90.0f);
#else
      glRasterPos2f(i*(20+HS)+3, 90.0f);
#endif
      printGLf(glw->font,"mi");
    }
    if( ColorMode != COLOR_PREF ) {
      if( !Filter[i] ) {
        glColor3ub(255,255,255);
        glCircle(i*(20+HS)+((20.0+HS)/2.),50,2);
      }
    }
    glLogicOp(GL_COPY);
  }
}

void DrawSpeciesHistogramI(GLWindow *glw, float tx, float ty)
{
  int i,sp=0; double t;
  sp = ((int)(pow((double)(2),(double)(nTraits))));

  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  // The outline for the histogram should be drawn first
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(-1,0);
  glVertex2i(sp*(20+HS)+1,0);
  glVertex2i(sp*(20+HS)+1,100);
  glVertex2i(-1,100);
  glEnd();

  // Draw our "species histogram"
  for(i=0; i<sp; i++) {
    t = ((double)(i))/((double)(sp));
    if( ((int)((double)(t)*((double)(sp))) == 0) )      glColor3ub( 50,  50,  50);
    if( ((int)((double)(t)*((double)(sp))) == 1) )      glColor3ub(255,   0,   0);
    if( ((int)((double)(t)*((double)(sp))) == 2) )      glColor3ub(255,   0, 255);
    if( ((int)((double)(t)*((double)(sp))) == 3) )      glColor3ub(  0,   0, 255);
    if( ((int)((double)(t)*((double)(sp))) == 4) )      glColor3ub(100,   0, 100);
    if( ((int)((double)(t)*((double)(sp))) == 5) )      glColor3ub(  0, 100, 255);
    if( ((int)((double)(t)*((double)(sp))) == 6) )      glColor3ub( 75,  75, 255);
    if( ((int)((double)(t)*((double)(sp))) == 7) )      glColor3ub(  0, 100,   0);
    if( ((int)((double)(t)*((double)(sp))) == 8) )      glColor3ub(255, 100,   0);
    if( ((int)((double)(t)*((double)(sp))) == 9) )      glColor3ub(  0, 180, 255);
    if( ((int)((double)(t)*((double)(sp))) == 10) )     glColor3ub(  0, 255, 255);
    if( ((int)((double)(t)*((double)(sp))) == 11) )     glColor3ub(  0, 200,  55);
    if( ((int)((double)(t)*((double)(sp))) == 12) )     glColor3ub(100,   0, 255);
    if( ((int)((double)(t)*((double)(sp))) == 13) )     glColor3ub(  0, 255,   0);
    if( ((int)((double)(t)*((double)(sp))) == 14) )     glColor3ub( 50, 130,   0);
    if( ((int)((double)(t)*((double)(sp))) == 15) )     glColor3ub(255, 255, 255);

    if( ((double)Gen[Frame-1].sphist.sphsize[i]) == 0. || ((double)Gen[Frame-1].sphist.msphsize) == 0. ){
      glBegin(GL_QUADS);
      glVertex2i(i*(20+HS),100);
      glVertex2i(i*(20+HS),99);
      glVertex2i((i+1)*(20+HS),99);
      glVertex2i((i+1)*(20+HS),100);
      glEnd();
    } else {
      glBegin(GL_QUADS);
      glVertex2i(i*(20+HS),100-(((double)Gen[Frame-1].sphist.sphsize[i])/((double)Gen[Frame-1].sphist.msphsize))*99);
      glVertex2i(i*(20+HS),99);
      glVertex2i((i+1)*(20+HS),99);
      glVertex2i((i+1)*(20+HS),100-(((double)Gen[Frame-1].sphist.sphsize[i])/((double)Gen[Frame-1].sphist.msphsize))*99);
      glEnd();
    }
    glLogicOp(GL_COPY);
  }
}

/*
void DrawSpeciesHistogramII(GLWindow *glw, float tx, float ty)
{
  int sp, spo, s;
  sp=spo=s=0;
  sp = ((int)(pow((double)(2),(double)(nTraits))));
  spo= (2*((int)(pow((double)(2),(double)(nTraits)))));
  s  = spo - sp;


  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  // The outline for the histogram should be drawn first
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(-1,0);
  glVertex2i(sp*(20+HS)+1,0);
  glVertex2i(sp*(20+HS)+1,100);
  glVertex2i(-1,100);
  glEnd();

  // Draw our "species histogram"
  int j;
  for(j=0; j<sp; j++) {
    glColor3ub(100, 100, 100);
    glBegin(GL_QUADS);
    glVertex2i((j)*(20+HS),100-(((double)Gen[Frame-1].sphist.sphsize[j+s])/((double)Gen[Frame-1].sphist.msphsize))*99);
    glVertex2i((j)*(20+HS),99);
    glVertex2i((j+1)*(20+HS),99);
    glVertex2i((j+1)*(20+HS),100-(((double)Gen[Frame-1].sphist.sphsize[j+s])/((double)Gen[Frame-1].sphist.msphsize))*99);
    glEnd();

    glLogicOp(GL_COPY);
  }
}
*/

void DrawPreferenceGraph(GLWindow *glw, float tx, float ty)
{
  int i,j;  double t;

#if PRETTY_PS
  char b[128];
#endif

  /* Translate */
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

#if PRETTY_PS
  if(PSFile0 && Gen[Frame-1].Popsize) {
    // Move the graph over so there is room for labels
    PS_Translate(PSFile0,.25,0);
    // Draw a white background 
    PS_Gray(PSFile0, 1.0);
    PS_Box(PSFile0,0,0,NICHES*.5,NICHES*.5,1);
  }
#endif

  /* Draw preference graph */
  glBegin(GL_QUADS);
  for(i=0; i<NICHES && Gen[Frame-1].Popsize; i++)
    for(j=0; j<NICHES; j++) {
      t = (((double)Gen[Frame-1].Prefs[i][j]) / Gen[Frame-1].mPref) * 255;
      glColor3ub(toByte(t), toByte(t), toByte(t));
      glVertex2i(i*(20+HS),j*(20+HS));
      glVertex2i(i*(20+HS),(j+1)*(20+HS));
      glVertex2i((i+1)*(20+HS),(j+1)*(20+HS));
      glVertex2i((i+1)*(20+HS),j*(20+HS));
#if PRETTY_PS
      if(PSFile0) {
        // Draw the boxes
        t = toByte(t)/255.;
#if LOG_SCALING
        t = log(toByte(t)+1)/log(256);
#endif
        t = 1.-t;
        PS_Gray(PSFile0,t);
        PS_Box(PSFile0,i/2.,j/2.,.5,.5,1);
      }
#endif
    }
  glEnd();

#if PRETTY_PS
  if(PSFile0 && Gen[Frame-1].Popsize) {
    // Draw an outline
    PS_Gray(PSFile0,0.);
    PS_Box(PSFile0,0,0,NICHES*.5,NICHES*.5,0); 
    // Restore the origin and draw some text labels
    PS_Translate(PSFile0,-.25,0);
    // Draw left labels
    for(i=0; i<NICHES; i++) {
      sprintf(b,"%d",i);
      PS_Text(PSFile0, .115, i*.5+.25, 12, b);
    }
    // Draw bottom labels
    for(i=0; i<NICHES; i++) {
      sprintf(b,"%d",i);
      PS_Text(PSFile0, i*.5+.45, NICHES*.5+.16, 12, b);
    }
  }
#endif

  /* Outline */
  glBegin(GL_LINES);
  for(i=0; i<NICHES;) {
    t = ((double)i)/(NICHES-1);
    glColor3ub(toByte(red(t)), toByte(green(t)), toByte(blue(t)));
    glVertex2i(i*(20+HS),0);
    i++;
    t = ((double)i)/(NICHES-1);
    glColor3ub(toByte(red(t)), toByte(green(t)), toByte(blue(t)));
    glVertex2i(i*(20+HS),0);
    i--;
    t = ((double)i)/(NICHES-1);
    glColor3ub(toByte(red(t)), toByte(green(t)), toByte(blue(t)));
    glVertex2i(i*(20+HS),(20+HS)*NICHES);
    i++;
    t = ((double)i)/(NICHES-1);
    glColor3ub(toByte(red(t)), toByte(green(t)), toByte(blue(t)));
    glVertex2i(i*(20+HS),(20+HS)*NICHES);
    i--;
    t = ((double)i)/(NICHES-1);
    glColor3ub(toByte(red(t)), toByte(green(t)), toByte(blue(t)));
    glVertex2i(0,i*(20+HS));
    i++;
    t = ((double)i)/(NICHES-1);
    glColor3ub(toByte(red(t)), toByte(green(t)), toByte(blue(t)));
    glVertex2i(0.0,i*(20+HS));
    i--;
    t = ((double)i)/(NICHES-1);
    glColor3ub(toByte(red(t)), toByte(green(t)), toByte(blue(t)));
    glVertex2i((20+HS)*NICHES,i*(20+HS));
    i++;
    t = ((double)i)/(NICHES-1);
    glColor3ub(toByte(red(t)), toByte(green(t)), toByte(blue(t)));
    glVertex2i((20+HS)*NICHES,i*(20+HS));
  }
  glEnd();
}

#if _SPVY
void DrawPreferenceGraphIII(GLWindow *glw, float tx, float ty)
{
  int i,j;  double t;
  int sp = 0;
  sp = ((int)(pow((double)(2),(double)(nTraits))));

#if PRETTY_PS
  char b[128];
#endif

  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

#if PRETTY_PS
  if(PSFile0 && Gen[Frame-1].Popsize) {
    // Move the graph over so there is room for labels
    PS_Translate(PSFile0,.25,0);
    // Draw a white background 
    PS_Gray(PSFile0, 1.0);
    PS_Box(PSFile0,0,0,sp*.5,sp*.5,1);
  }
#endif

  // Draw preference graph
  glBegin(GL_QUADS);
  for(i=1; i<sp && Gen[Frame-1].Popsize; i++)
    for(j=0; j<(nTraits*4); j++) {
    t = (((double)Gen[Frame-1].spvy[i][j]) / ((double)(Gen[Frame-1].range.sppop[i])))* 255;

    if( i == 1 ){
      if( j<4 ){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 2 ){
      if(j>=4 && j<8 ){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 3 ){
      if( j<8 ){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 4 ){
      if( j>=8 && j<12){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 5 ){
      if( j<4 || (j>=8 && j<12) ){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 6 ){
      if( j>=4 && j<12){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 7 ){
      if( j<12){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 8 ){
      if( j>=12 && j<16){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 9 ){
      if( j<4 || (j>=12 && j<16) ){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 10 ){
      if( (j>=4 && j<8) || (j>=12 && j<16) ){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 11 ){
      if( j<8 || (j>=12 && j<16) ){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 12 ){
      if( j>=8 && j<16 ){ 
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 13 ){
      if( j<4 || (j>=8 && j<16) ){
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 14 ){
      if( j>=4 && j<16 ){ 
        glColor3ub(toByte(t), toByte(t), 0);
      } else {
        glColor3ub(toByte(t), toByte(t), toByte(t));
      }
    }

    if( i == 15 ){
      glColor3ub(toByte(t), toByte(t), 0);
    }

    // original line
    // glColor3ub(toByte(t), toByte(t), toByte(t));
    glVertex2i(i*(20+HS),j*(20+HS));
    glVertex2i(i*(20+HS),(j+1)*(20+HS));
    glVertex2i((i+1)*(20+HS),(j+1)*(20+HS));
    glVertex2i((i+1)*(20+HS),j*(20+HS));
#if PRETTY_PS
    if(PSFile0) {
      // Draw the boxes
      t = toByte(t)/255.;
#if LOG_SCALING
      t = log(toByte(t)+1)/log(256);
#endif
      t = 1.-t;
      PS_Gray(PSFile0,t);
      PS_Box(PSFile0,i/2.,j/2.,.5,.5,1);
    }
#endif
    }
    glEnd();

#if PRETTY_PS
    if(PSFile0 && Gen[Frame-1].Popsize) {
      // Draw an outline
      PS_Gray(PSFile0,0.);
      PS_Box(PSFile0,0,0,sp*.5,sp*.5,0); 
      // Restore the origin and draw some text labels
      PS_Translate(PSFile0,-.25,0);
      // Draw left labels
      for(i=0; i<sp; i++) {
        sprintf(b,"%d",i);
        PS_Text(PSFile0, .115, i*.5+.25, 12, b);
      }
      // Draw bottom labels
      for(i=0; i<sp; i++) {
        sprintf(b,"%d",i);
        PS_Text(PSFile0, i*.5+.45, sp*.5+.16, 12, b);
      }
    }
#endif

    // Inline
    glBegin(GL_LINES);
    glColor3ub(255, 255, 255);
    glVertex2i(0,(20+HS)*4);
    glVertex2i((20+HS)*16,(20+HS)*4);
    glVertex2i(0,(20+HS)*8);
    glVertex2i((20+HS)*16,(20+HS)*8);
    glVertex2i(0,(20+HS)*12);
    glVertex2i((20+HS)*16,(20+HS)*12);
    //glVertex2i((20+HS)*sp,0);
    glEnd();

    // Outline
    glBegin(GL_LINE_LOOP);
    glColor3ub(255, 255, 255);
    glVertex2i(0,0);
    glVertex2i(0,(20+HS)*sp);
    glVertex2i((20+HS)*sp,(20+HS)*sp);
    glVertex2i((20+HS)*sp,0);
    glEnd();
}
#endif

#if _NEWRM
void DrawTrait(GLWindow *glw, float tx, float ty)
{
  int i,j;  double t;
  int sp = 16;

#if PRETTY_PS
  char b[128];
#endif

  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

#if PRETTY_PS
  if(PSFile0 && Gen[Frame-1].Popsize) {
    // Move the graph over so there is room for labels
    PS_Translate(PSFile0,.25,0);
    // Draw a white background 
    PS_Gray(PSFile0, 1.0);
    PS_Box(PSFile0,0,0,sp*.5,sp*.5,1);
  }
#endif

  // Draw preference graph
  glBegin(GL_QUADS);
  for(i=0; i<sp && Gen[Frame-1].Popsize; i++)
    for(j=0; j<4; j++) {
    t = (((double)Gen[Frame-1].newrm[i][j]) / Gen[Frame-1].mnewrm)* 255;

    if( i == 0 || i == 5 || i == 10 || i == 15 ){
      glColor3ub(toByte(t), toByte(t), 0);
    } else {
      glColor3ub(toByte(t), toByte(t), toByte(t));
    }

    glVertex2i(i*(20+HS),j*(20+HS));
    glVertex2i(i*(20+HS),(j+1)*(20+HS));
    glVertex2i((i+1)*(20+HS),(j+1)*(20+HS));
    glVertex2i((i+1)*(20+HS),j*(20+HS));
#if PRETTY_PS
    if(PSFile0) {
      // Draw the boxes
      t = toByte(t)/255.;
#if LOG_SCALING
      t = log(toByte(t)+1)/log(256);
#endif
      t = 1.-t;
      PS_Gray(PSFile0,t);
      PS_Box(PSFile0,i/2.,j/2.,.5,.5,1);
    }
#endif
    }
    glEnd();

#if PRETTY_PS
    if(PSFile0 && Gen[Frame-1].Popsize) {
      // Draw an outline
      PS_Gray(PSFile0,0.);
      PS_Box(PSFile0,0,0,sp*.5,sp*.5,0); 
      // Restore the origin and draw some text labels
      PS_Translate(PSFile0,-.25,0);
      // Draw left labels
      for(i=0; i<sp; i++) {
        sprintf(b,"%d",i);
        PS_Text(PSFile0, .115, i*.5+.25, 12, b);
      }
      // Draw bottom labels
      for(i=0; i<sp; i++) {
        sprintf(b,"%d",i);
        PS_Text(PSFile0, i*.5+.45, sp*.5+.16, 12, b);
      }
    }
#endif

    // Inline
    glBegin(GL_LINES);
    glColor3ub(255, 255, 255);
    glVertex2i(0,(20+HS)*4);
    glVertex2i((20+HS)*16,(20+HS)*4);

    glVertex2i((20+HS)*4,(20+HS)*4);
    glVertex2i((20+HS)*4,0);
    glVertex2i((20+HS)*8,(20+HS)*4);
    glVertex2i((20+HS)*8,0);
    glVertex2i((20+HS)*12,(20+HS)*4);
    glVertex2i((20+HS)*12,0);

    //glVertex2i(0,(20+HS)*12);
    //glVertex2i((20+HS)*16,(20+HS)*12);
    //glVertex2i((20+HS)*sp,0);
    glEnd();

    // Outline
    glBegin(GL_LINE_LOOP);
    glColor3ub(255, 255, 255);
    glVertex2i(0,0);
    glVertex2i(0,(20+HS)*4);
    glVertex2i((20+HS)*sp,(20+HS)*4);
    glVertex2i((20+HS)*sp,0);
    glEnd();
}
#endif

void ProcessDuration(int spc, int start, int end, int flag)
{
  static double  mdur,adur,amrange,count,amrangemaxdur;
  static FILE   *f, *g;
  static int     mdurs, mdure;
  int            i;

  switch(flag) {
    case -1:
      // per-file
      if( !(f=fopen("durationdata","w")) ) {
        fprintf(stderr, "Error: cannot open duration data file!\n");
      }
      if( !(g=fopen("maxdurationdata","w")) ) {
        fprintf(stderr, "Error: cannot open max duration data file!\n");
      }
      break;

    case 0:
      // init (per species)
      count           = 0.0; // number of separate durations
      adur            = 0.0; // average duration
      mdur            = 0.0; // maximum duration
      mdurs           = 0;   // maximum duration start
      mdure           = 0;   // maximum duration end
      amrange         = 0.0; // summing all the ranges across duration specified
      amrangemaxdur   = 0.0; // average maximum range during the maximum duration
      break;

    case 1:
      // Still causing "duration" to be applied to average (per species)
      // printf("duration: spc%d [%d,%d) %d\n", Gen[start].range.idspecies[spc], start, end, end-start);
      adur += end-start;                          // summing the durations to calculate average length of durations
      count++;                                    // number of durations
      if( end-start > mdur ) {                    // find max duration length meanwhile
        mdur = end-start;
        mdurs = start;                            // set the start of max duration
        mdure = end;                              // set the end of max duration
      }

      // Find average max range for the current duration
      for(i=start; i<end; i++) {
        amrange += Gen[i].range.mrange[spc];      //summing all ranges for average range calculation later
      }
      break;

    case 2:
      // done with all "durations" collected so far (per species)
      if( count ){
        amrange /= adur;  // doesn't seem very useful
        adur    /= count; // doesn't seem very useful
        // also calculate the mean max range during the max duration
        for(i=mdurs; i<mdure; i++){
          amrangemaxdur += Gen[i].range.mrange[spc];      //summing all ranges for average range calculation later
        }
        amrangemaxdur /= mdur;
      } else {
        adur =  0.;
      }

      // printf("duration: spc%d has avg duration len (with %lf samples) of %lf\n", spc, count, adur);
      // for "durationdata" file
      fprintf(f,"%lf ", adur);
      for(i=0; i<spc; i++) { 
        fprintf(f,"0.000000 ");
      }
      fprintf(f,"%lf ",amrange);
      for(i=spc+1; i<(((int)(pow((double)(2),(double)(nTraits))))); i++) { 
        fprintf(f,"0.000000 ");
      }
      fprintf(f,"\n");

      // for "maxdurationdata" file
      fprintf(g,"%lf ", mdur);
      for(i=0; i<spc; i++) {
        fprintf(g,"0.000000 ");
      }
      fprintf(g,"%lf ",amrangemaxdur);

      printf("species%dmaxdur: %lff-species%drangemdur: %lff\n", spc, mdur, spc, amrangemaxdur);

      for(i=spc+1; i<(((int)(pow((double)(2),(double)(nTraits))))); i++) {
        fprintf(g,"0.000000 ");
      }
      fprintf(g,"\n");
      break;

    case 3:
      // per file
      fclose(f);
      fclose(g);
      break;
  }
}

void CountDuration()
{
  int start, end, l, i;
  start = end = -1;

#define thresholdrange 0
#define thresholdt     0

  ProcessDuration(0,0,0,-1);
  for(l=0; l<((int)(pow((double)(2),(double)(nTraits)))); l++){
    ProcessDuration(l,0,0,0);
    for( i=0; i<nGen; i++){
      if( start == -1 ){
        if( Gen[i].range.mrange[l] > thresholdrange ){
          start = i;
        }
      } else {
        if( Gen[i].range.mrange[l] <= thresholdrange ){
          end = i;
          if( end-start >= thresholdt ) {
            ProcessDuration(l, start, end, 1);
          }
          start = -1;
          end = -1;
        } else {
          if( i == nGen - 1 ){
            end = i;
            if( end-start >= thresholdt ) {
              ProcessDuration(l, start, end, 1);
            }
            start = -1;
            end = -1;
          }
        }
      }
    }
    ProcessDuration(l,0,0,2);
    //printf("\n--------------\n");
  }
  ProcessDuration(0,0,0,3);
  system("gnuplot durationplot");
  system("gnuplot maxdurationplot");
}

void CountCoexistenceDuration()
{
  int     start, end, i;
  double  mcdur,acdur,ccount,tcdur;

  start =  -1;
  end   =  -1;
  ccount = 0.0;
  acdur  = 0.0;
  mcdur  = 0.0;
  tcdur  = 0.0;

  for( i=0; i<nGen; i++ ){
    if( start == -1 ){
      if( Gen[i].coexist > 0 ){
        start = i;
      }
    } else {
      if( Gen[i].coexist == 0 ){
        end = i;
          // adding it up for average
        acdur += end-start;                          // summing the durations to calculate ave. length of durations
        ccount++;                                    // number of durations
        if( end-start > mcdur ) {                    // find max duration length meanwhile
          mcdur = end-start;
        }
        start = -1;
        end = -1;
      } else {
        if( i == nGen - 1 ){
          end = i;
            // same as above, but if they last till the end
          acdur += end-start;                        // summing the durations to calculate ave. length of durations
          ccount++;                                  // number of durations
          if( end-start > mcdur ) {                  // find max duration length meanwhile
            mcdur = end-start;
          }
          start = -1;
          end = -1;
        }
      }
    }
  }

  tcdur = acdur;
  // done with all "durations" collected so far
  if( ccount ) {
    acdur /= ccount;
  } else {
    acdur =  0.;
  }
  //printf("----\ntotal: %f, average: %f, number of times: %d, max: %f\n\n", tcdur, acdur, ((int)(ccount)), mcdur);
  Coexistmean   = acdur;
  Coexistmax    = mcdur;
  Coexisttotal  = tcdur;
  Coexistcount  = ccount;
}

void CountMultiSDuration()
{
  int     s, e, i,count;
  double  mcd,acd,tcd;

  s =  -1;
  e =  -1;
  count = 0;
  acd  = 0.0; // average multispecies coexistence duration
  mcd  = 0.0; // maximum multispecies coexistence duration
  tcd  = 0.0; // total multispecies coexistence duration

  for( i=0; i<nGen; i++ ){
    if( s == -1 ){
      if( Gen[i].multisp == 1 ){
        s = i;
      }
    } else {
      if( Gen[i].multisp == 0 ){
        e = i;
        // adding it up for average
        acd += e - s;         // summing the durations to calculate average length of durations
        count++;              // number of durations
        if( e - s > mcd ) {   // find max duration length meanwhile
          mcd = e - s;
        }
        s = -1;
        e = -1;
      } else {
        if( i == nGen - 1 ){
          e = i;
            // same as above, but if they last till the end
          acd += e - s;       // summing the durations to calculate average length of durations
          count++;            // number of durations
          if( e - s > mcd ) { // find max duration length meanwhile
            mcd = e - s;
          }
          s = -1;
          e = -1;
        }
      }
    }
  }

  tcd = acd;
  // done with all "durations" collected so far
  if( count ) {
    acd /= ((double)(count));
  } else {
    acd =  0.;
  }
  //printf("---\ntotal: %f, average: %f, number of times: %d, max: %f\n\n", tcdur, acdur, ((int)(ccount)), mcdur);
  Multimean   = acd;
  Multimax    = mcd;
  Multitotal  = tcd;
  Multicount  = ((double)(count));
}


void CountSpecies()
{
  int     i, l, spc, gc, tc;
  FILE   *f;

  if( !(f=fopen("coexistencedata","w")) ){
    fprintf(stderr, "Error: cannot open coexistence data file!\n");
  }

  for( i=0; i<nGen; i++){

    // time to speciation
    if( Timetospec == 0 ){
      for(l=0; l<((int)(pow((double)(2),(double)(nTraits)))); l++){
        if( Gen[i].range.mrange[l]  > thresholdrange ){
          Timetospec = i;
          break;
        }
      }
    }

    fprintf(f, "%d ", i);
    spc = gc = tc = 0;
    if( Gen[i].range.mrange[1]  > thresholdrange ){
      spc++;
      tc++;
      if( Tspec[1] == 0) Tspec[1] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[3] > 0 || Gen[i].range.mrange[5] > 0 || Gen[i].range.mrange[7] > 0 ||
          Gen[i].range.mrange[9] > 0 || Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[13] > 0 ||
          Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[2]  > thresholdrange ){
      spc++;
      tc++;
      if( Tspec[2] == 0) Tspec[2] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[3] > 0 || Gen[i].range.mrange[6] > 0 || Gen[i].range.mrange[7] > 0 ||
          Gen[i].range.mrange[10] > 0 || Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[14] > 0 ||
          Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[4]  > thresholdrange ){
      spc++;
      tc++;
      if( Tspec[4] == 0) Tspec[4] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[5] > 0 || Gen[i].range.mrange[6] > 0 || Gen[i].range.mrange[7] > 0 ||
          Gen[i].range.mrange[12] > 0 || Gen[i].range.mrange[13] > 0 || Gen[i].range.mrange[14] > 0 ||
          Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[8]  > thresholdrange ){
      spc++;
      tc++;
      if( Tspec[8] == 0) Tspec[8] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[9] > 0 || Gen[i].range.mrange[10] > 0 || Gen[i].range.mrange[11] > 0 ||
          Gen[i].range.mrange[12] > 0 || Gen[i].range.mrange[13] > 0 || Gen[i].range.mrange[14] > 0 ||
          Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[3]  > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[3] == 0) Tspec[3] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[1] > 0 || Gen[i].range.mrange[2] > 0 || Gen[i].range.mrange[5] > 0 ||
          Gen[i].range.mrange[6] > 0 || Gen[i].range.mrange[7] > 0 || Gen[i].range.mrange[9] > 0 ||
          Gen[i].range.mrange[10] > 0 || Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[13] > 0 ||
          Gen[i].range.mrange[14] > 0 || Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[5]  > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[5] == 0) Tspec[5] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[1] > 0 || Gen[i].range.mrange[3] > 0 || Gen[i].range.mrange[4] > 0 ||
          Gen[i].range.mrange[6] > 0 || Gen[i].range.mrange[7] > 0 || Gen[i].range.mrange[9] > 0 ||
          Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[12] > 0 || Gen[i].range.mrange[13] > 0 ||
          Gen[i].range.mrange[14] > 0 || Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[6]  > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[6] == 0) Tspec[6] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[2] > 0 || Gen[i].range.mrange[3] > 0 || Gen[i].range.mrange[4] > 0 ||
          Gen[i].range.mrange[5] > 0 || Gen[i].range.mrange[7] > 0 || Gen[i].range.mrange[10] > 0 ||
          Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[12] > 0 || Gen[i].range.mrange[13] > 0 ||
          Gen[i].range.mrange[14] > 0 || Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[7]  > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[7] == 0) Tspec[7] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[1] > 0 || Gen[i].range.mrange[2] > 0 || Gen[i].range.mrange[3] > 0 ||
          Gen[i].range.mrange[4] > 0 || Gen[i].range.mrange[5] > 0 || Gen[i].range.mrange[6] > 0 ||
          Gen[i].range.mrange[9] > 0 || Gen[i].range.mrange[10] > 0 || Gen[i].range.mrange[11] > 0 ||
          Gen[i].range.mrange[12] > 0 || Gen[i].range.mrange[13] > 0 || Gen[i].range.mrange[14] > 0 ||
          Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[9]  > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[9] == 0) Tspec[9] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[1] > 0 || Gen[i].range.mrange[3] > 0 || Gen[i].range.mrange[5] > 0 ||
          Gen[i].range.mrange[7] > 0 || Gen[i].range.mrange[8] > 0 || Gen[i].range.mrange[10] > 0 ||
          Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[12] > 0 || Gen[i].range.mrange[13] > 0 ||
          Gen[i].range.mrange[14] > 0 || Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[10] > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[10] == 0) Tspec[10] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[2] > 0 || Gen[i].range.mrange[3] > 0 || Gen[i].range.mrange[6] > 0 ||
          Gen[i].range.mrange[7] > 0 || Gen[i].range.mrange[8] > 0 || Gen[i].range.mrange[9] > 0 ||
          Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[12] > 0 || Gen[i].range.mrange[13] > 0 ||
          Gen[i].range.mrange[14] > 0 || Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[11] > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[11] == 0) Tspec[11] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[1] > 0 || Gen[i].range.mrange[2] > 0 || Gen[i].range.mrange[3] > 0 ||
          Gen[i].range.mrange[5] > 0 || Gen[i].range.mrange[6] > 0 || Gen[i].range.mrange[7] > 0 ||
          Gen[i].range.mrange[8] > 0 || Gen[i].range.mrange[9] > 0 || Gen[i].range.mrange[10] > 0 ||
          Gen[i].range.mrange[12] > 0 || Gen[i].range.mrange[13] > 0 || Gen[i].range.mrange[14] > 0 ||
          Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[12] > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[12] == 0) Tspec[12] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[4] > 0 || Gen[i].range.mrange[5] > 0 || Gen[i].range.mrange[6] > 0 ||
          Gen[i].range.mrange[7] > 0 || Gen[i].range.mrange[8] > 0 || Gen[i].range.mrange[9] > 0 ||
          Gen[i].range.mrange[10] > 0 || Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[13] > 0 ||
          Gen[i].range.mrange[14] > 0 || Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[13] > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[13] == 0) Tspec[13] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[1] > 0 || Gen[i].range.mrange[3] > 0 || Gen[i].range.mrange[4] > 0 ||
          Gen[i].range.mrange[5] > 0 || Gen[i].range.mrange[6] > 0 || Gen[i].range.mrange[7] > 0 ||
          Gen[i].range.mrange[8] > 0 || Gen[i].range.mrange[9] > 0 || Gen[i].range.mrange[10] > 0 ||
          Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[12] > 0 || Gen[i].range.mrange[14] > 0 ||
          Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[14] > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[14] == 0) Tspec[14] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[2] > 0 || Gen[i].range.mrange[3] > 0 || Gen[i].range.mrange[4] > 0 ||
          Gen[i].range.mrange[5] > 0 || Gen[i].range.mrange[6] > 0 || Gen[i].range.mrange[7] > 0 ||
          Gen[i].range.mrange[8] > 0 || Gen[i].range.mrange[9] > 0 || Gen[i].range.mrange[10] > 0 ||
          Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[12] > 0 || Gen[i].range.mrange[13] > 0 ||
          Gen[i].range.mrange[15] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }
    if( Gen[i].range.mrange[15] > thresholdrange ){
      gc++;
      tc++;
      if( Tspec[15] == 0) Tspec[15] = i;
      if( Gen[i].coexist == 0 && (Gen[i].range.mrange[1] > 0 || Gen[i].range.mrange[2] > 0 || Gen[i].range.mrange[3] > 0 ||
          Gen[i].range.mrange[4] > 0 || Gen[i].range.mrange[5] > 0 || Gen[i].range.mrange[6] > 0 ||
          Gen[i].range.mrange[7] > 0 || Gen[i].range.mrange[8] > 0 || Gen[i].range.mrange[9] > 0 ||
          Gen[i].range.mrange[10] > 0 || Gen[i].range.mrange[11] > 0 || Gen[i].range.mrange[12] > 0 ||
          Gen[i].range.mrange[13] > 0 || Gen[i].range.mrange[14] > 0) ){
        Gen[i].coexist = 1;
      } else {
        Gen[i].coexist = 0;
      }
    }

    if( tc > Maxmultinos )
      Maxmultinos = tc;

    Gen[i].spcount =  spc;
    Gen[i].gencount = gc;
    Gen[i].tcount =   tc;
    fprintf(f, "%d %d %d ", Gen[i].spcount, Gen[i].gencount, Gen[i].tcount);

    if( Gen[i].tcount > 1 ){
      Gen[i].multisp = 1;
      //printf("gen: %d\n", i);
    } else {
      Gen[i].multisp = 0;
    }
    fprintf(f, "%d\n", Gen[i].coexist);
  }
  fclose(f);
  CountCoexistenceDuration();
  system("gnuplot coexistenceplot");
  CountMultiSDuration();
}


void WriteGNUPlot()
{
  FILE *gnuplot;  int l,i;

  // create data file for gnu file
  if( !(gnuplot=fopen("gnuplotdata","w")) ) {
    printf("Could not open gnuplotdata file!\n");
    exit(7);
  }

  for(i=0; i<nGen; i++) {
    fprintf(gnuplot,"%d ",i);
    for(l=0; l<((int)(pow((double)(2),(double)(nTraits)))); l++){
      fprintf(gnuplot,"%lf ",(((double)(Gen[i].range.mrange[l]))/(((double)(Width))*((double)(Height)))));
    }
    fprintf(gnuplot,"\n");
  }

  fclose(gnuplot);

  // run gnu plot
  system("gnuplot plot");
}

void WriteResource()
{
  FILE *f;  patch *p; int l,j,k;
  int widthsum[nTraits][Width], heightsum[nTraits][Height];
  memset(widthsum, 0, sizeof(widthsum));
  memset(heightsum, 0, sizeof(heightsum));

  // create data file for gnu file
  if( !(f=fopen("resourcedata","w")) ){
    printf("Could not open resourcedata file!\n");
    exit(7);
  }

  for(j=0; j<Width; j++){
    for(k=0; k<Height; k++){
      p = &Gen[nGen-1].p[j][k];
      widthsum[p->n][j]++;
      heightsum[p->n][k]++;
    }
  }

  //int x[64];
  // Width == Height so doesn't matter which
  for(j=0; j<Width; j++){
    //x[j] = j;
    fprintf(f,"%d ", j);
    for(l=0; l<((int)(nTraits)); l++){
      fprintf(f,"%d %d ", widthsum[l][j], heightsum[l][j]);
    }
    fprintf(f,"\n");
  }

  /*
  // something like this?
  double c0, c1, cov00, cov01, cov11, sumsq;
  gsl_fit_linear (x, 1, widthsum[0], 1, Width, &c0, &c1, &cov00, &cov01, &cov11, &sumsq);
  printf ("# best fit: Y = %g + %g X\n", c0, c1);
  */

  fclose(f);
  // run gnu plot
  system("gnuplot resourceplot");

}


void WritePopSizePlot()
{
  FILE *f;  int l,i;

  // create data file for gnu file
  if( !(f=fopen("popsizedata","w")) ) {
    printf("Could not open popsizedata file!\n");
    exit(7);
  }
  for(i=0; i<nGen; i++) {
    fprintf(f,"%d ",i);
    for(l=0; l<((int)(pow((double)(2),(double)(nTraits)))); l++){
      fprintf(f,"%lf ",(((double)(Gen[i].range.sppop[l]))/(Max*(((double)(Width))*((double)(Height))))));
    }
    fprintf(f,"\n");
  }
  fclose(f);

  // run gnu plot
  system("gnuplot popsizeplot"); 
}

void WriteSummaryPlot()
{

  FILE *f;  int i;

  // create data file for gnu file
  if( !(f=fopen("summarydata","w")) ) {
    printf("Could not open summarydata file!\n");
    exit(7);
  }
  for(i=0; i<nGen; i++) {
    fprintf(f,"%d ",i);
    fprintf(f,"%lf %lf %lf %lf %lf ",((double)(Gen[i].acp)), ((double)(Gen[i].apr)), ((double)(Gen[i].amc)), ((double)Gen[i].tns)/((((KPNy-1)/4)+1)*NICHES), ((double)(Gen[i].sfit)));
    fprintf(f,"\n");
  }
  fclose(f);

  // run gnu plot
  system("gnuplot summaryplot"); 

}


void WriteTrans()
{
  //FILE *gnuplot;
  char buff[1024];
  sprintf(buff,"cat transplot2 | sed -e 's/name/%s.trans/g' > transplot",FNPrefix);
  system(buff);
  system("gnuplot transplot");
}

void WriteXvss()
{
  char buff[1024];
  sprintf(buff,"cat xvssplot2 | sed -e 's/name/%s.xvss/g' > xvssplot",FNPrefix);
  system(buff);
  system("gnuplot xvssplot");
}

#if DETAILED_HISTO
void WriteTraitPlot()
{
  FILE *f;  patch *p; int l,i,j,k;

  // create data file for gnu file
  if( !(f=fopen("traitdata","w")) ) {
    printf("Could not open traitdata file!\n");
    exit(7);
  }

  for(i=0; i<nGen; i++) {
    for( j=0; j<Width; j++ ){
      for( k=0; k<Height; k++ ){
        p = &Gen[i].p[j][k];
        fprintf(f,"%d ",i);
        for(l=0; l<((int)(nTraits)); l++){
          fprintf(f,"%lf ",p->af[l]);
        }
        fprintf(f,"\n");
      }
    }
  }

  fclose(f);

  // run gnu plot
  system("gnuplot traitplot");
}
#endif


#if PRETTY_PS
void WriteDemes()
{
  int i,j;  double t,c;
  FILE *f;  double sx=4.0/(Width*Scale),sy=4.0/(Height*Scale);

  // Open eps file
  if( !(f = PS_Start("demesfig.eps",Width*Scale*sx,Height*Scale*sy)) ) {
    fprintf(stderr,"Could not open output post-script file! (demesfig.eps)\n");
    exit(1);
  }
//printf( "demesfig.eps file is opened!\n" );
  // Move the graph over so there is room for labels
  PS_Translate(f,0.,0.);

  // Draw a white background
  PS_Color(f, 1.0, 1.0, 1.0);
  PS_Quad(f,
          0,              0,
          0,              sy*Scale*Height,
          sx*Scale*Width, sy*Scale*Height,
          sx*Scale*Width, 0,
          1);


  // Draw the background patch character
  for(i=0; i<Width; i++) {
    for(j=0; j<Height; j++) {

      t = ((double)Gen[Frame-1].p[i][j].n)/(NICHES-1);
#if BLACK_WHITE
      if( ((int)((double)(t)*((double)(NICHES-1))) == 0) ) PS_Color(f,232/((float)255),220/((float)255),232/((float)255));
      if( ((int)((double)(t)*((double)(NICHES-1))) == 1) ) PS_Color(f,202/((float)255),190/((float)255),202/((float)255));
      if( ((int)((double)(t)*((double)(NICHES-1))) == 2) ) PS_Color(f,172/((float)255),160/((float)255),172/((float)255));
      if( ((int)((double)(t)*((double)(NICHES-1))) == 3) ) PS_Color(f,142/((float)255),130/((float)255),142/((float)255));
#else
      PS_Color(f,toByte(red  (t)*.3)/((float)255), toByte(green(t)*.3)/((float)255), toByte(blue (t)*.3)/((float)255));
#endif
      //t = ((double)Gen[Frame-1].p[i][j].n);//this is old
      //printf("\n %f", t);
      PS_Quad(f,
              i*Scale*sx,           j*Scale*sy,
              i*Scale*sx,           (j+1)*Scale*sy,
              (i*Scale+Scale)*sx,   (j+1)*Scale*sy,
              (i*Scale+Scale)*sx,   j*Scale*sy,
              1);
    }
  }

  // Draw the patch size circle and outline 
  for(i=0; i<Width && Gen[Frame-1].Popsize; i++) {
    for(j=0; j<Height; j++) {
      /* Skip empty patcehs */
      if( !Gen[Frame-1].p[i][j].ni )
        continue;
      t = Gen[Frame-1].p[i][j].ni/((Gen[Frame-1].rMax<Max/_b)?(Max/_b):((double)Gen[Frame-1].rMax));

#if BLACK_WHITE
      c = Gen[Frame-1].p[i][j].sid;
      if( t >= 0 ){
        if( (((int)(c)) == 0) )  PS_Color(f,  50/((float)255),  50/((float)255),  50/((float)255));
        if( (((int)(c)) == 1) )  PS_Color(f, 255/((float)255),   0/((float)255),   0/((float)255));
        if( (((int)(c)) == 2) )  PS_Color(f, 255/((float)255),   0/((float)255), 255/((float)255));
        if( (((int)(c)) == 3) )  PS_Color(f,   0/((float)255),   0/((float)255), 255/((float)255));
        if( (((int)(c)) == 4) )  PS_Color(f, 100/((float)255),   0/((float)255), 100/((float)255));
        if( (((int)(c)) == 5) )  PS_Color(f,   0/((float)255), 100/((float)255), 255/((float)255));
        if( (((int)(c)) == 6) )  PS_Color(f,  75/((float)255),  75/((float)255), 255/((float)255));
        if( (((int)(c)) == 7) )  PS_Color(f,   0/((float)255), 100/((float)255),   0/((float)255));
        if( (((int)(c)) == 8) )  PS_Color(f, 255/((float)255), 100/((float)255),   0/((float)255));
        if( (((int)(c)) == 9) )  PS_Color(f,   0/((float)255), 180/((float)255), 255/((float)255));
        if( (((int)(c)) == 10) ) PS_Color(f,   0/((float)255), 255/((float)255), 255/((float)255));
        if( (((int)(c)) == 11) ) PS_Color(f,   0/((float)255), 200/((float)255),  55/((float)255));
        if( (((int)(c)) == 12) ) PS_Color(f, 100/((float)255),   0/((float)255), 255/((float)255));
        if( (((int)(c)) == 13) ) PS_Color(f,   0/((float)255), 255/((float)255),   0/((float)255));
        if( (((int)(c)) == 14) ) PS_Color(f,  50/((float)255), 130/((float)255),   0/((float)255));
        if( (((int)(c)) == 15) ) PS_Color(f, 255/((float)255), 255/((float)255), 255/((float)255));
        if( (((int)(c)) >  15) ) PS_Color(f,  50/((float)255),  50/((float)255),  50/((float)255));
        PS_Circle(f, (i*Scale+Scale*.5)*sx, (j*Scale+Scale*.5)*sy, sqrt((Scale*.5)*(Scale*.5)*t)*sy, 1);
        PS_Circle(f, (i*Scale+Scale*.5)*sx, (j*Scale+Scale*.5)*sy, sqrt((Scale*.5)*(Scale*.5)*t)*sy, 0);
      }
#else
      // Circle colors are based on niche preference
      c = Gen[Frame-1].p[i][j].pnf/((double)(NICHES-1));
      PS_Color(f,toByte(red  (c)*.3)/((float)255), toByte(green(c)*.3)/((float)255), toByte(blue (c)*.3)/((float)255));
      if( t >= 0 ) {
        PS_Circle(f, (i*Scale+Scale*.5)*sx, (j*Scale+Scale*.5)*sy, sqrt((Scale*.5)*(Scale*.5)*t)*sy, 1);
      }
      PS_Color(f, toByte(red  (c))/((float)255), toByte(green(c))/((float)255), toByte(blue (c))/((float)255));
      if( t >= 0 ) {
        PS_Circle(f, (i*Scale+Scale*.5)*sx, (j*Scale+Scale*.5)*sy, sqrt((Scale*.5)*(Scale*.5)*t)*sy, 0);
      }
#endif
    }
  }

  // Outline
  PS_Color(f,0.f,0.f,0.f);
  PS_Box(f, 0.f, 0.f, Width*Scale*sx, Height*Scale*sy, 0);

  // Restore origin and write text labels
  PS_Translate(f,-0.5,0.);
  // Close eps file
  PS_End(f);
}

void WriteLandscape()
{
  int i,j;  double t;
  FILE *f;  double sx=4.0/(Width*Scale),sy=4.0/(Height*Scale);

  // Open eps file
  if( !(f = PS_Start("landsfig.eps",Width*Scale*sx,Height*Scale*sy)) ) {
    fprintf(stderr,"Could not open output post-script file! (landsfig.eps)\n");
    exit(1);
  }

  PS_Translate(f,0.,0.);

  // Draw a white background
  PS_Color(f, 1.0, 1.0, 1.0);
  PS_Quad(f,
          0,              0,
          0,              sy*Scale*Height,
          sx*Scale*Width, sy*Scale*Height,
          sx*Scale*Width, 0,
          1);


  // Draw the background patch character
  for(i=0; i<Width; i++) {
    for(j=0; j<Height; j++) {

      t = ((double)Gen[Frame-1].p[i][j].n)/(NICHES-1);

      if( ((int)((double)(t)*((double)(NICHES-1))) == 0) ) PS_Color(f,232/((float)255),220/((float)255),232/((float)255));
      if( ((int)((double)(t)*((double)(NICHES-1))) == 1) ) PS_Color(f,202/((float)255),190/((float)255),202/((float)255));
      if( ((int)((double)(t)*((double)(NICHES-1))) == 2) ) PS_Color(f,172/((float)255),160/((float)255),172/((float)255));
      if( ((int)((double)(t)*((double)(NICHES-1))) == 3) ) PS_Color(f,142/((float)255),130/((float)255),142/((float)255));

      PS_Quad(f,
              i*Scale*sx, j*Scale*sy,
              i*Scale*sx, (j+1)*Scale*sy,
                          (i*Scale+Scale)*sx, (j+1)*Scale*sy,
                          (i*Scale+Scale)*sx, j*Scale*sy,
                          1);
    }
  }

  // Outline
  PS_Color(f,0.f,0.f,0.f);
  PS_Box(f, 0.f, 0.f, Width*Scale*sx, Height*Scale*sy, 0);

  // Restore origin and write text labels
  PS_Translate(f,-0.5,0.);
  // Close eps file
  PS_End(f);
}

void WriteMVK()
{
  FILE *f;  double sx=4.0/MVKx,sy=4.0/MVKy, t;  int i,j;

  // Open eps file
  if( !(f = PS_Start("mvk.eps",MVKx*sx+.5,MVKy*sy+.5)) ) {
    fprintf(stderr,"Could not open output post-script file! (mvk.eps)\n");
    exit(1);
  }

  // Move the graph over so there is room for labels
  PS_Translate(f,.5,0);

  // Draw a white background
  PS_Color(f, 1.0, 1.0, 1.0);
  PS_Quad(f,
	  0,       0,
	  0,       sy*MVKy,
	  sx*MVKx, sy*MVKy,
	  sx*MVKx, 0,
	  1);

  // Draw mvk
  for(i=0; i<MVKx; i++) {
    for(j=0; j<MVKy; j++) {
      t = (((double)Gen[Frame-1].mvk[i][j])/Gen[Frame-1].mmvk) * 255;
      PS_Color(f, 1-toByte(t)/255., 1-toByte(t)/255., 1-toByte(t)/255.);
      PS_Quad(f, 
	      i*sx,     j*sy,
	      i*sx,     (j+1)*sy,
	      (i+1)*sx, (j+1)*sy,
	      (i+1)*sx, j*sy,
	      1);
    }
  }

  // Outline
  PS_Color(f, 0.0, 0.0, 0.0);
  PS_Quad(f,
	  0,       0,
	  0,       sy*MVKy,
	  sx*MVKx, sy*MVKy,
	  sx*MVKx, 0,
	  0);

  // Restore origin and write text labels
  PS_Translate(f,-.5,0);
  PS_Text(f, sx*MVKx/2+.5, sy*MVKy+.25, 12, "c");
  PS_Text(f, .25,          sy*MVKy/2,   12, "k");
  
  // Close eps file
  PS_End(f);
}

void WriteKPN()
{
  FILE *f;  double sx=2.0/KPNx,sy=4.0/KPNy;  int i,j;  double t,x1;  char b[128];

  /*
  if( nGen <= 1 ) {
    if( !(f = PS_Start("kpn.eps",KPNx*sx,KPNy*sy+1)) ) {
      fprintf(stderr,"Could not open output post-script file! (kpn.eps)\n");
    }
    PS_End(f);
  }
  */

  // Open eps file
  if( !(f = PS_Start("kpn.eps",KPNx*sx,KPNy*sy+1)) ) {
    fprintf(stderr,"Could not open output post-script file! (kpn.eps)\n");
    exit(1);
  }

  // Move the graph over so there is room for labels
  PS_Translate(f,0,-.5);

  // Draw a white background
  PS_Color(f, 1.0, 1.0, 1.0); 
  PS_Quad(f,
	  0,       0,
	  0,       sy*KPNy,
	  sx*KPNx, sy*KPNy,
	  sx*KPNx, 0,
	  1);

  // Draw kpn
  if( FPN ) x1 = sx/2.;
  else      x1 = sx;
  for(i=0; i<KPNx; i++) {
    for(j=0; j<KPNy; j++) {
      t = (((double)Gen[Frame-1].kpn[i][j]) / Gen[Frame-1].mkpn);
      PS_Color(f, 1-t, 1-t, 1-t);
      PS_Quad(f, 
	      i*sx,    j*sy,
	      i*sx,    j*sy+sy,
	      i*sx+x1, j*sy+sy,
	      i*sx+x1, j*sy,
	      1);
    }
  }

  // Draw FPN
  if( FPN ) {
    for(i=0; i<FPNx; i++) {
      for(j=0; j<FPNy; j++) {
	t = (((double)Gen[Frame-1].fpn[i][j]) / Gen[Frame-1].mfpn);
	PS_Color(f, 1-t, 1-t, 1-t);
	PS_Quad(f,
		i*sx+x1, j*sy,
		i*sx+x1, j*sy+sy,
		i*sx+sx, j*sy+sy,
		i*sx+sx, j*sy,
		1);
      }
    }
  }

  // Outline
  PS_Color(f, 0.0, 0.0, 0.0); 
  PS_Quad(f,
	  0,       0,
	  0,       sy*KPNy,
	  sx*KPNx, sy*KPNy,
	  sx*KPNx, 0,
	  0);

  // Restore origin and write text labels
  PS_Translate(f,0,.5);
  for(i=0; i<KPNx; i++) {
    // Draw niche number on top
    sprintf(b,"%d",i);
    PS_Text(f, sx*i+sx/2.1, .5/1.5, 12, b);
    // Draw male and female marks on the bottom
    PS_Text(f, sx*i+sx/4.1, FPNy*sy+1.6*.5, 12, "m");
    PS_Text(f, sx*i+sx/1.5, FPNy*sy+1.6*.5, 12, "f");
  }

  // Close eps file
  PS_End(f);
}

void WriteSPN()
{
  FILE *f;
  int i,j,sp;
  double tt,x,y,x1,y1;
  char b[128];

  sp = ((int)(pow((double)(2),(double)(nTraits))));

  // Draw spn
  x = (sp)/((double)SPNx);
  y1 = y = 4/((double)SPNy);
  x1 = x/2.;

  // Open eps file
  if( !(f = PS_Start("spn.eps",SPNx*x+.5,SPNy*y+.5)) ) {
    fprintf(stderr,"Could not open output post-script file! (mvk.eps)\n");
    exit(1);
  }

  // Move the graph over so there is room for labels
  PS_Translate(f,0,-.25);

  // Draw a white background
  PS_Color(f, 1.0, 1.0, 1.0);
  PS_Quad(f,
          0,       0,
          0,       y*SPNy,
          x*SPNx,  y*SPNy,
          x*SPNx,  0,
          1);

  // Draw spn
  for(i=0; i<SPNx; i++) {
    for(j=0; j<SPNy; j++) {
      // Draw SPN block
      tt = (((double)Gen[Frame-1].spn[i][j]) / Gen[Frame-1].mspn) * 255;
      PS_Color(f, 1-toByte(tt)/255., 1-toByte(tt)/255., 1-toByte(tt)/255.);
      PS_Quad(f,
              i*x,    j*y,
              i*x,    j*y+y1,
              i*x+x1, j*y+y1,
              i*x+x1, j*y,
              1);
    }
  }

  for(i=0; i<SFPNx; i++) {
    for(j=0; j<SFPNy; j++) {
      // Draw SPN block
      tt = (((double)Gen[Frame-1].sfpn[i][j]) / Gen[Frame-1].msfpn) * 255;
      PS_Color(f, 1-toByte(tt)/255., 1-toByte(tt)/255., 1-toByte(tt)/255.);
      PS_Quad(f,
              i*x+x1, j*y,
              i*x+x1, j*y+y1,
              i*x+x,  j*y+y1,
              i*x+x,  j*y,
              1);
    }
  }


  // Outline
  PS_Color(f, 0.0, 0.0, 0.0);
  PS_Quad(f,
          0,       0,
          0,       y*SPNy,
          x*SPNx,  y*SPNy,
          x*SPNx,  0,
          0);

  // Restore origin and write text labels
  PS_Translate(f,0,.25);
  for(i=0; i<SPNx; i++) {
    // Draw niche number on top
    sprintf(b,"%d",i);
    PS_Text(f, x*i+x/2.1, .25/1.5, 12, b);
    // Draw male and female marks on the bottom
    PS_Text(f, x*i+x/4.1,  SPNy*y+1.6*.25, 12, "m");
    PS_Text(f, x*i+x/1.5,  SPNy*y+1.6*.25, 12, "f");
  }
  // Close eps file
  PS_End(f);
}

#endif

void DrawMVK(GLWindow *glw, float tx, float ty)
{
  int i,j;  double t,x,y;

#if PRETTY_PS
  // Write to disk if eps file is open
  if( PSFile0 ) {
    WriteMVK();
  }
#endif

  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  // Draw mvk
  x = (8*(20+HS))/((double)MVKx);
  y = (8*(20+HS))/((double)MVKy);
  glBegin(GL_QUADS);
  for(i=0; i<MVKx; i++) {
    for(j=0; j<MVKy; j++) {
      t = (((double)Gen[Frame-1].mvk[i][j]) / Gen[Frame-1].mmvk) * 255;
      glColor3ub(toByte(t), toByte(t), toByte(t));
      glVertex2i(  i  *x,   j  *y);
      glVertex2i(  i  *x, (j+1)*y);
      glVertex2i((i+1)*x, (j+1)*y);
      glVertex2i((i+1)*x,   j  *y);
    }
  }
  glEnd();

  // Outline and labels
  glBegin(GL_LINE_LOOP);
  glColor3ub(255,255,255);
  glVertex2i(0,            0);
  glVertex2i(0,            8*(20+HS));
  glVertex2i(8*(20+HS), 8*(20+HS));
  glVertex2i(8*(20+HS), 0);
  glEnd();
  glRasterPos2f(4*(20+HS)-16, 8*(20+HS)+12.0f);  printGLf(glw->font,"mating");
  glRasterPos2f(-12,          4*(20+HS)-20-5);   printGLf(glw->font,"m");
  glRasterPos2f(-12,          4*(20+HS)-10-5);   printGLf(glw->font,"a");
  glRasterPos2f(-12,          4*(20+HS)-5);      printGLf(glw->font,"r");
  glRasterPos2f(-12,          4*(20+HS)+10-5);   printGLf(glw->font,"k");
  glRasterPos2f(-12,          4*(20+HS)+20-5);   printGLf(glw->font,"e");
  glRasterPos2f(-12,          4*(20+HS)+30-5);   printGLf(glw->font,"r");
}

void DrawKPN(GLWindow *glw, float tx, float ty)
{
  int i,j;  double t,tt,x,y,x1,y1;

#if PRETTY_PS
  // Write to disk if eps file is open
  if( PSFile0 ) {
    WriteKPN();
  }
#endif

  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  // Draw kpn and fpn
  x = (NICHES*(20+HS))/((double)KPNx);
  y1 = y = (8*(20+HS))/((double)KPNy);
  if( FPN ) x1 = x/2.;
  else      x1 = x;

  for(i=0; i<KPNx; i++) {
    for(j=0; j<KPNy; j++) {
      // Draw KPN block
      t  = ((double)i)/(NICHES-1);
      tt = (((double)Gen[Frame-1].kpn[i][j]) / Gen[Frame-1].mkpn);
      // gives segmentation fault here when the computer was updated...
#if BLACK_WHITE
      if( ((int)((double)(t)*((double)(NICHES-1))) == 0) ) glColor3ub(232*tt,220*tt,232*tt);
      if( ((int)((double)(t)*((double)(NICHES-1))) == 1) ) glColor3ub(202*tt,190*tt,202*tt);
      if( ((int)((double)(t)*((double)(NICHES-1))) == 2) ) glColor3ub(172*tt,160*tt,172*tt);
      if( ((int)((double)(t)*((double)(NICHES-1))) == 3) ) glColor3ub(142*tt,130*tt,142*tt);
#else
      glColor3ub(toByte(red(t)*tt), toByte(green(t)*tt), toByte(blue(t)*tt));
#endif
      glBegin(GL_QUADS);
      glVertex2i(i*x,    j*y);
      glVertex2i(i*x,    j*y+y1);
      glVertex2i(i*x+x1, j*y+y1);
      glVertex2i(i*x+x1, j*y);
      glEnd(); 
    }
    // Draw # of species label
    glColor3ub(255, 255, 0);
    glRasterPos2f(i*x+(20+HS)/2., -5);  printGLf(glw->font,"%d",Gen[Frame-1].ns[i]);
  }

  if( FPN ) {
    glBegin(GL_QUADS);
    for(i=0; i<FPNx; i++) {
      for(j=0; j<FPNy; j++) {
        t  = ((double)i)/(NICHES-1);
	tt = (((double)Gen[Frame-1].fpn[i][j]) / Gen[Frame-1].mfpn);
#if BLACK_WHITE
        if( ((int)((double)(t)*((double)(NICHES-1))) == 0) ) glColor3ub(232*tt,220*tt,232*tt);
        if( ((int)((double)(t)*((double)(NICHES-1))) == 1) ) glColor3ub(202*tt,190*tt,202*tt);
        if( ((int)((double)(t)*((double)(NICHES-1))) == 2) ) glColor3ub(172*tt,160*tt,172*tt);
        if( ((int)((double)(t)*((double)(NICHES-1))) == 3) ) glColor3ub(142*tt,130*tt,142*tt);
#else
        glColor3ub(toByte(red(t)*tt), toByte(green(t)*tt), toByte(blue(t)*tt));
#endif
        glVertex2i(i*x+x1, j*y);
	glVertex2i(i*x+x1, j*y+y1);
	glVertex2i(i*x+x,  j*y+y1);
	glVertex2i(i*x+x,  j*y);
      }
    }
    glEnd(); 
  }

  // Outline and labels
  glBegin(GL_LINE_LOOP);
  glColor3ub(255,255,255);
  glVertex2i(0,              0);
  glVertex2i(0,              8*(20+HS));
  glVertex2i(NICHES*(20+HS), 8*(20+HS));
  glVertex2i(NICHES*(20+HS), 0);
  glEnd();
  glRasterPos2f(NICHES/2*(20+HS)-14,    8*(20+HS)+12.0f);  printGLf(glw->font,"per niche");
}

void DrawSPN(GLWindow *glw, float tx, float ty)
{
  int i,j,sp;  double tt,x,y,x1,y1;
  sp = ((int)(pow((double)(2),(double)(nTraits))));

  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  // Draw spn
  x = (sp*(20+HS))/((double)SPNx);
  y1 = y = (8*(20+HS))/((double)SPNy);
  x1 = x/2.;

  for(i=0; i<SPNx; i++) {
    for(j=0; j<SPNy; j++) {
      // Draw SPN block
      tt = (((double)Gen[Frame-1].spn[i][j]) / Gen[Frame-1].mspn) * 255;
      glColor3ub(toByte(tt), toByte(tt), toByte(tt));
      glBegin(GL_QUADS);
      glVertex2i(i*x,    j*y);
      glVertex2i(i*x,    j*y+y1);
      glVertex2i(i*x+x1, j*y+y1);
      glVertex2i(i*x+x1, j*y);
      glEnd();
    }
    glColor3ub(255, 255, 0);
    glRasterPos2f(i*x+(20+HS)/2., -5);  printGLf(glw->font,"%d",Gen[Frame-1].nss[i]);
  }

  for(i=0; i<SFPNx; i++) {
    for(j=0; j<SFPNy; j++) {
      // Draw SPN block
      tt = (((double)Gen[Frame-1].sfpn[i][j]) / Gen[Frame-1].msfpn) * 255;
      glColor3ub(toByte(tt), toByte(tt), toByte(tt));
      glBegin(GL_QUADS);
      glVertex2i(i*x+x1, j*y);
      glVertex2i(i*x+x1, j*y+y1);
      glVertex2i(i*x+x,  j*y+y1);
      glVertex2i(i*x+x,  j*y);
      glEnd();
    }
  }

  // Outline and labels
  glBegin(GL_LINE_LOOP);
  glColor3ub(255,255,255);
  glVertex2i(0,            0);
  glVertex2i(0,            8*(20+HS));
  glVertex2i(sp*(20+HS),   8*(20+HS));
  glVertex2i(sp*(20+HS),   0);
  glEnd();
  //glRasterPos2f(sp*3/4*(20+HS)-14, 8*(20+HS)+12.0f);  printGLf(glw->font,"per species");
}

void DrawGenerationSlider(GLWindow *glw, float tx, float ty)
{
  double t;

  /* Translate */
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  /* Draw generation slider */
  glColor3ub(255, 255, 255);
  glBegin(GL_LINES);
  /* Bar */
  glVertex2i(0, 15.0f);
  glVertex2i(Width*Scale, 15.0f);
  /* Vertical "tick" mark */
  t = (((double)(Frame-1))/(nGen-1))*(Width*Scale);
  glVertex2i(t, 0.0f);
  glVertex2i(t, 30.0f);
  glEnd();
}

void DrawSummaryGraph(GLWindow *glw, float tx, float ty)
{
  int i,j;  static int nb=0; static double *buckets[6];

  if ( nGen <= 1 )
    return;

  // Translate
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  if(nb) {
  draw:
    // Draw graph here from buckets
    glBegin(GL_LINES);
    // fitness
    glColor3ub(255, 155,   0);
    for(i=1; i<nb; i++) {
      glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[0][i-1]);
      glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[0][i]);
    }
    // preference
    glColor3ub(  0, 100,   0);
    for(i=1; i<nb; i++) {
      glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[1][i-1]);
      glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[1][i]);
    }
    // amc
    if( Sexual ) {
      glColor3ub(255, 100, 255);
      for(i=1; i<nb; i++) {
        glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[2][i-1]);
        glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[2][i]);
      }
    }
    // species no
    if( KPN ) {
      glColor3ub(255, 255,   0);
      for(i=1; i<nb; i++) {
        glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[3][i-1]);
        glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[3][i]);
      }
    }
    // species fitness
    glColor3ub(255,   0,   0);
    for(i=1; i<nb; i++) {
      glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[4][i-1]);
      glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[4][i]);
    }


    glEnd();
    // Outline
    glColor3ub(255, 255, 255);
    glBegin(GL_LINE_LOOP);
    glVertex2i(0,0);
    glVertex2i(Width*Scale,0);
    glVertex2i(Width*Scale,150);
    glVertex2i(0,150);
    glEnd();
    // Labels for the summary graph
    i = -40;
    glColor3ub(255, 155,   0);  glRasterPos2f(i+=40, 165);  printGLf(glw->font,"T.Fit");
    glColor3ub(255,   0,   0);  glRasterPos2f(i+=40, 165);  printGLf(glw->font,"S.Fit");
    glColor3ub(  0, 100,   0);  glRasterPos2f(i+=40, 165);  printGLf(glw->font,"Preference");
    if( Sexual ) glColor3ub(255, 100, 255);
    glRasterPos2f(i+=70, 165);  printGLf(glw->font,"Choosiness");
    if( KPN ) glColor3ub(255, 255,   0);
    glRasterPos2f(i+=70, 165);  printGLf(glw->font,"Species");
  } else {
    // Initialize buckets
    nb = ((200<nGen)?(200):(nGen));
    for(i=0; i<6; i++) {
      buckets[i] = malloc(nb*sizeof(double));
      memset(buckets[i], 0, nb*sizeof(double));
    }
    // Fill in buckets
    for(i=0; i<nGen; i++) {
      j = (int)((float)(((((double)(i))/(nGen-1))*(nb-1))));
      // fitness; orange
      buckets[0][j] += Gen[i].acp; //originally .cf
      // preference; green
      buckets[1][j] += Gen[i].apr; // originally .cm
      // Chosiness; pink
      buckets[2][j] += Gen[i].amc;
      // Number of species; yellow
      buckets[3][j] += ((double)Gen[i].tns)/((((KPNy-1)/4)+1)*NICHES);
#if NEWFITNESS
      // Current Migration; red
      buckets[4][j] += Gen[i].sfit; //originally .cf
#endif
      // Just count; used for scaling
      buckets[5][j]++;
    }
    // Scale buckets
    for(i=0; i<6; i++)
      for(j=0; j<nb; j++) {
        buckets[i][j] = 150-((buckets[i][j]/buckets[5][j])*150);
      }

    // Jump tp drawing section to cause the graph to actually be drawn
    goto draw;
  }
}


void DrawRangeMean(GLWindow *glw, float tx, float ty)
{
  int i,j;  static int nb=0;  static double *buckets[17];

  if ( nGen <= 1 )
    return;

  /* Translate */
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  if(nb) {
  draw:
      /* Draw graph here from buckets */
      glBegin(GL_LINES);
  glColor3ub(60, 60, 60);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[0][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[0][i]);
  }
  glColor3ub(255, 0, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[1][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[1][i]);
  }
  glColor3ub(255, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[2][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[2][i]);
  }
  glColor3ub(0, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[3][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[3][i]);
  }
  glColor3ub(100, 0, 100);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[4][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[4][i]);
  }
  glColor3ub(0, 100, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[5][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[5][i]);
  }
  glColor3ub(75, 75, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[6][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[6][i]);
  }
  glColor3ub(0, 100, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[7][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[7][i]);
  }
  glColor3ub(255, 100, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[8][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[8][i]);
  }
  glColor3ub(0, 180, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[9][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[9][i]);
  }
  glColor3ub(0, 255, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[10][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[10][i]);
  }
  glColor3ub(0, 200, 55);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[11][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[11][i]);
  }
  glColor3ub(100, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[12][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[12][i]);
  }
  glColor3ub(0, 255, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[13][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[13][i]);
  }
  glColor3ub(50, 130, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[14][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[14][i]);
  }
  glColor3ub(255,255,255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[15][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[15][i]);
  }

  glEnd();
    // Outline
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(0,0);
  glVertex2i(Width*Scale,0);
  glVertex2i(Width*Scale,150);
  glVertex2i(0,150);
  glEnd();
    // Labels for the summary graph
  i = -40;

  } else {
    // Initialize buckets
    nb = ((200<nGen)?(200):(nGen));
    for(i=0; i<17; i++) {
      buckets[i] = malloc(nb*sizeof(double));
      memset(buckets[i], 0, nb*sizeof(double));
    }
    // Fill in buckets
    for(i=0; i<nGen; i++) {
      j = (int)((float)(((((double)(i))/(nGen-1))*(nb-1))));
      int l;
      for(l=0; l<((int)(pow((double)(2),(double)(nTraits)))); l++){
        // Current Fitness; red
        buckets[l][j] += (((double)(Gen[i].range.meanr[l]))/(((double)(Width))*((double)(Height))));
        //buckets[l][j] += ((double)(Gen[i].range.meanr[l]));
        //printf("\n----%lf",((double)(Gen[i].range.meanr[l]))/(((double)(Width))*((double)(Height))));
      }
      // Just count; used for scaling
      buckets[16][j]++;
    }
    // Scale buckets
    for(i=0; i<17; i++)
      for(j=0; j<nb; j++)
        buckets[i][j] = 150-((buckets[i][j]/buckets[16][j])*150);

    // Jump t0 drawing section to cause the graph to actually be drawn
    goto draw;
  }
}

void DrawRangeMax(GLWindow *glw, float tx, float ty)
{
  int i,j;  static int nb=0;  static double *buckets[17];

  if ( nGen <= 1 )
    return;

  /* Translate */
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  if(nb) {
  draw:
      /* Draw graph here from buckets */
      glBegin(GL_LINES);
  glColor3ub(60, 60, 60);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[0][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[0][i]);
  }
  glColor3ub(255, 0, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[1][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[1][i]);
  }
  glColor3ub(255, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[2][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[2][i]);
  }
  glColor3ub(0, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[3][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[3][i]);
  }
  glColor3ub(100, 0, 100);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[4][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[4][i]);
  }
  glColor3ub(0, 100, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[5][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[5][i]);
  }
  glColor3ub(75, 75, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[6][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[6][i]);
  }
  glColor3ub(0, 100, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[7][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[7][i]);
  }
  glColor3ub(255, 100, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[8][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[8][i]);
  }
  glColor3ub(0, 180, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[9][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[9][i]);
  }
  glColor3ub(0, 255, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[10][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[10][i]);
  }
  glColor3ub(0, 200, 55);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[11][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[11][i]);
  }
  glColor3ub(100, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[12][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[12][i]);
  }
  glColor3ub(0, 255, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[13][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[13][i]);
  }
  glColor3ub(50, 130, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[14][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[14][i]);
  }
  glColor3ub(255,255,255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[15][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[15][i]);
  }

  glEnd();
    // Outline
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(0,0);
  glVertex2i(Width*Scale,0);
  glVertex2i(Width*Scale,150);
  glVertex2i(0,150);
  glEnd();
    // Labels for the summary graph
  i = -40;
  } else {
    /* Initialize buckets */
    nb = ((200<nGen)?(200):(nGen));
    for(i=0; i<17; i++) {
      buckets[i] = malloc(nb*sizeof(double));
      memset(buckets[i], 0, nb*sizeof(double));
    }
    /* Fill in buckets */
    for(i=0; i<nGen; i++) {
      j = (int)((float)(((((double)(i))/(nGen-1))*(nb-1))));

      int l;
      for(l=0; l<((int)(pow((double)(2),(double)(nTraits)))); l++){
        // Current Fitness; red
        buckets[l][j] += (((double)(Gen[i].range.mrange[l]))/(((double)(Width))*((double)(Height))));
        //buckets[l][j] += ((double)(Gen[i].range.meanr[l]));
        //printf("\n----%lf",((double)(Gen[i].range.meanr[l]))/(((double)(Width))*((double)(Height))));
      }
      buckets[16][j]++;
    }
    /* Scale buckets */
    for(i=0; i<17; i++)
      for(j=0; j<nb; j++)
        buckets[i][j] = 150-((buckets[i][j]/buckets[16][j])*150);

    /* Jump t0 drawing section to cause the graph to actually be drawn */
    goto draw;
  }
}

void DrawRangeMaxSp(GLWindow *glw, float tx, float ty)
{
  int i,j;  static int nb=0;  static double *buckets[5];

  if ( nGen <= 1 )
    return;

  /* Translate */
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  if(nb) {
  draw:
  /* Draw graph here from buckets */
  glBegin(GL_LINES);

  glColor3ub(255, 0, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[0][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[0][i]);
  }
  glColor3ub(255, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[1][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[1][i]);
  }
  glColor3ub(100, 0, 100);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[2][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[2][i]);
  }
  glColor3ub(255, 100, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[3][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[3][i]);
  }

  glEnd();
    // Outline
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(0,0);
  glVertex2i(Width*Scale,0);
  glVertex2i(Width*Scale,150);
  glVertex2i(0,150);
  glEnd();
    // Labels for the summary graph
  i = -40;

  } else {
    /* Initialize buckets */
    nb = ((200<nGen)?(200):(nGen));
    for(i=0; i<5; i++) {
      buckets[i] = malloc(nb*sizeof(double));
      memset(buckets[i], 0, nb*sizeof(double));
    }
    /* Fill in buckets */
    for(i=0; i<nGen; i++) {
      j = (int)((float)(((((double)(i))/(nGen-1))*(nb-1))));
      buckets[0][j] += ((double)(Gen[i].range.mrange[1]))/(((double)(Width))*((double)(Height)));
      buckets[1][j] += ((double)(Gen[i].range.mrange[2]))/(((double)(Width))*((double)(Height)));
      buckets[2][j] += ((double)(Gen[i].range.mrange[4]))/(((double)(Width))*((double)(Height)));
      buckets[3][j] += ((double)(Gen[i].range.mrange[8]))/(((double)(Width))*((double)(Height)));

      // Just count; used for scaling
      buckets[4][j]++;
    }
    /* Scale buckets */
    for(i=0; i<5; i++)
      for(j=0; j<nb; j++)
        buckets[i][j] = 150-((buckets[i][j]/buckets[4][j])*150);

    /* Jump t0 drawing section to cause the graph to actually be drawn */
    goto draw;
  }
}

void DrawRangeMaxGn(GLWindow *glw, float tx, float ty)
{
  int i,j;  static int nb=0;  static double *buckets[12];

  if ( nGen <= 1 )
    return;

  /* Translate */
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  if(nb) {
  draw:
      /* Draw graph here from buckets */
      glBegin(GL_LINES);

  glColor3ub(0, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[0][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[0][i]);
  }
  glColor3ub(0, 100, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[1][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[1][i]);
  }
  glColor3ub(75, 75, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[2][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[2][i]);
  }
  glColor3ub(0, 100, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[3][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[3][i]);
  }
  glColor3ub(0, 180, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[4][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[4][i]);
  }
  glColor3ub(0, 255, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[5][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[5][i]);
  }
  glColor3ub(0, 200, 55);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[6][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[6][i]);
  }
  glColor3ub(100, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[7][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[7][i]);
  }
  glColor3ub(0, 255, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[8][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[8][i]);
  }
  glColor3ub(50, 130,  0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[9][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[9][i]);
  }
  glColor3ub(255,255,255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[10][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[10][i]);
  }

  glEnd();
    // Outline
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(0,0);
  glVertex2i(Width*Scale,0);
  glVertex2i(Width*Scale,150);
  glVertex2i(0,150);
  glEnd();
    // Labels for the summary graph
  i = -40;


  } else {
    /* Initialize buckets */
    nb = ((200<nGen)?(200):(nGen));
    for(i=0; i<12; i++) {
      buckets[i] = malloc(nb*sizeof(double));
      memset(buckets[i], 0, nb*sizeof(double));
    }
    /* Fill in buckets */
    for(i=0; i<nGen; i++) {
      j = (int)((float)(((((double)(i))/(nGen-1))*(nb-1))));
      buckets[0][j] += ((double)(Gen[i].range.mrange[3]))/(((double)(Width))*((double)(Height)));
      buckets[1][j] += ((double)(Gen[i].range.mrange[5]))/(((double)(Width))*((double)(Height)));
      buckets[2][j] += ((double)(Gen[i].range.mrange[6]))/(((double)(Width))*((double)(Height)));
      buckets[3][j] += ((double)(Gen[i].range.mrange[7]))/(((double)(Width))*((double)(Height)));
      buckets[4][j] += ((double)(Gen[i].range.mrange[9]))/(((double)(Width))*((double)(Height)));
      buckets[5][j] += ((double)(Gen[i].range.mrange[10]))/(((double)(Width))*((double)(Height)));
      buckets[6][j] += ((double)(Gen[i].range.mrange[11]))/(((double)(Width))*((double)(Height)));
      buckets[7][j] += ((double)(Gen[i].range.mrange[12]))/(((double)(Width))*((double)(Height)));
      buckets[8][j] += ((double)(Gen[i].range.mrange[13]))/(((double)(Width))*((double)(Height)));
      buckets[9][j] += ((double)(Gen[i].range.mrange[14]))/(((double)(Width))*((double)(Height)));
      buckets[10][j] += ((double)(Gen[i].range.mrange[15]))/(((double)(Width))*((double)(Height)));
      
      // Just count; used for scaling
      buckets[11][j]++;
    }
    /* Scale buckets */
    for(i=0; i<12; i++)
      for(j=0; j<nb; j++)
        buckets[i][j] = 150-((buckets[i][j]/buckets[11][j])*150);

    /* Jump t0 drawing section to cause the graph to actually be drawn */
    goto draw;
  }
}
void DrawRangeNum(GLWindow *glw, float tx, float ty)
{
  int i,j;  static int nb=0;  static double *buckets[17];

  if ( nGen <= 1 )
    return;

  /* Translate */
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  if(nb) {
  draw:
      /* Draw graph here from buckets */
      glBegin(GL_LINES);
  glColor3ub(60, 60, 60);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[0][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[0][i]);
  }
  glColor3ub(255, 0, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[1][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[1][i]);
  }
  glColor3ub(255, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[2][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[2][i]);
  }
  glColor3ub(0, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[3][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[3][i]);
  }
  glColor3ub(100, 0, 100);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[4][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[4][i]);
  }
  glColor3ub(0, 100, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[5][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[5][i]);
  }
  glColor3ub(75, 75, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[6][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[6][i]);
  }
  glColor3ub(0, 100, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[7][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[7][i]);
  }
  glColor3ub(255, 100, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[8][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[8][i]);
  }
  glColor3ub(0, 180, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[9][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[9][i]);
  }
  glColor3ub(0, 255, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[10][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[10][i]);
  }
  glColor3ub(0, 200, 55);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[11][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[11][i]);
  }
  glColor3ub(100, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[12][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[12][i]);
  }
  glColor3ub(0, 255, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[13][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[13][i]);
  }
  glColor3ub(50, 130, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[14][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[14][i]);
  }
  glColor3ub(255,255,255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[15][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[15][i]);
  }

  glEnd();
    // Outline
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(0,0);
  glVertex2i(Width*Scale,0);
  glVertex2i(Width*Scale,150);
  glVertex2i(0,150);
  glEnd();
    // Labels for the summary graph
  i = -40;

  } else {
    /* Initialize buckets */
    nb = ((200<nGen)?(200):(nGen));
    for(i=0; i<17; i++) {
      buckets[i] = malloc(nb*sizeof(double));
      memset(buckets[i], 0, nb*sizeof(double));
    }
    /* Fill in buckets */
    for(i=0; i<nGen; i++) {
      j = (int)((float)(((((double)(i))/(nGen-1))*(nb-1))));

      int l;
      for(l=0; l<((int)(pow((double)(2),(double)(nTraits)))); l++){
        // Current Fitness; red
        buckets[l][j] += 10*(((double)(Gen[i].range.noisolate[l]))/(((double)(Width))*((double)(Height))));
        //buckets[l][j] += ((double)(Gen[i].range.meanr[l]));
        //printf("\n----%lf",((double)(Gen[i].range.meanr[l]))/(((double)(Width))*((double)(Height))));
      }

      // Just count; used for scaling
      buckets[16][j]++;
    }
    /* Scale buckets */
    for(i=0; i<17; i++)
      for(j=0; j<nb; j++)
        buckets[i][j] = 150-((buckets[i][j]/buckets[16][j])*150);

    /* Jump t0 drawing section to cause the graph to actually be drawn */
    goto draw;
  }
}

void DrawPopSize(GLWindow *glw, float tx, float ty)
{
  int i,j;  static int nb=0;  static double *buckets[17];

  if ( nGen <= 1 )
    return;

  /* Translate */
  glLoadIdentity();
  glTranslatef(tx, ty, 1.0f);

  if(nb) {
  draw:
      /* Draw graph here from buckets */
      glBegin(GL_LINES);
  glColor3ub(60, 60, 60);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[0][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[0][i]);
  }
  glColor3ub(255, 0, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[1][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[1][i]);
  }
  glColor3ub(255, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[2][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[2][i]);
  }
  glColor3ub(0, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[3][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[3][i]);
  }
  glColor3ub(100, 0, 100);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[4][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[4][i]);
  }
  glColor3ub(0, 100, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[5][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[5][i]);
  }
  glColor3ub(75, 75, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[6][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[6][i]);
  }
  glColor3ub(0, 100, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[7][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[7][i]);
  }
  glColor3ub(255, 100, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[8][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[8][i]);
  }
  glColor3ub(0, 180, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[9][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[9][i]);
  }
  glColor3ub(0, 255, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[10][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[10][i]);
  }
  glColor3ub(0, 200, 55);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[11][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[11][i]);
  }
  glColor3ub(100, 0, 255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[12][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[12][i]);
  }
  glColor3ub(0, 255, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[13][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[13][i]);
  }
  glColor3ub(50, 130, 0);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[14][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[14][i]);
  }
  glColor3ub(255,255,255);
  for(i=1; i<nb; i++) {
    glVertex2d((((double)(i-1))/(nb-1))*Width*Scale, buckets[15][i-1]);
    glVertex2d((((double)(i))/(nb-1))*Width*Scale,   buckets[15][i]);
  }

  glEnd();
    // Outline
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(0,0);
  glVertex2i(Width*Scale,0);
  glVertex2i(Width*Scale,150);
  glVertex2i(0,150);
  glEnd();
    // Labels for the summary graph
  i = -40;

  } else {
    /* Initialize buckets */
    nb = ((200<nGen)?(200):(nGen));
    for(i=0; i<17; i++) {
      buckets[i] = malloc(nb*sizeof(double));
      memset(buckets[i], 0, nb*sizeof(double));
    }
    /* Fill in buckets */
    for(i=0; i<nGen; i++) {
      j = (int)((float)(((((double)(i))/(nGen-1))*(nb-1))));

      int l;
      for(l=0; l<((int)(pow((double)(2),(double)(nTraits)))); l++){
        // Current Fitness; red
        buckets[l][j] += (((double)(Gen[i].range.sppop[l]))/(Max*((double)(Width))*((double)(Height))));
        //buckets[l][j] += ((double)(Gen[i].range.meanr[l]));
        //printf("\n----%lf",((double)(Gen[i].range.meanr[l]))/(((double)(Width))*((double)(Height))));
      }
      buckets[16][j]++;
    }
    /* Scale buckets */
    for(i=0; i<17; i++)
      for(j=0; j<nb; j++)
        buckets[i][j] = 150-((buckets[i][j]/buckets[16][j])*150);

    /* Jump t0 drawing section to cause the graph to actually be drawn */
    goto draw;
  }
}

void DrawOutlineTranslucentBox(int x1, int y1, int x2, int y2)
{
  // Translucent background
  glColor4ub(0, 0, 0, 128);
  glBegin(GL_QUADS);
  glVertex2i(x1,y1);
  glVertex2i(x2,y1);
  glVertex2i(x2,y2);
  glVertex2i(x1,y2);
  glEnd();
  // Outline
  glColor3ub(255, 255, 255);
  glBegin(GL_LINE_LOOP);
  glVertex2i(x1,y1);
  glVertex2i(x2,y1);
  glVertex2i(x2,y2);
  glVertex2i(x1,y2);
  glEnd();
  glEndList();
}

void DrawHoverBox(GLWindow *glw)
{
  int i,j,k,x,y;  char b[1024],buf[4096];

  /* Just return if out of bounds */
  if( ((i=(x=Mx)/((int)Scale)) >= Width) || ((j=(y=My)/((int)Scale)) >= Height) )
    return;

  // Draw the hover box background
  if( Gen[Frame-1].p[i][j].ni ) {
  // Bound and translate
  if( (y+150) > (Height*Scale) ) y = (Height*Scale)-150;
  if( (x+200) > (Width*Scale)  ) x = (Width*Scale) -200;
  glLoadIdentity();
  glTranslatef(x, y, 1.0f);
#if PRESENTATION_MODE
    DrawOutlineTranslucentBox(0,0,250,150);
#else
    DrawOutlineTranslucentBox(0,0,200,150);
#endif
  } else {
  // Bound and translate
  if( (y+50)  > (Height*Scale) ) y = (Height*Scale)-50;
  if( (x+150) > (Width*Scale)  ) x = (Width*Scale) -150;
  glLoadIdentity();
  glTranslatef(x, y, 1.0f);
#if PRESENTATION_MODE
    DrawOutlineTranslucentBox(0,0,200,50);
#else
    DrawOutlineTranslucentBox(0,0,150,50);
#endif
  }

  /* Less info for empty patches */
  if( !Gen[Frame-1].p[i][j].ni ) {
    glColor3ub(255,255,255);
    glRasterPos2f(15.0f,15.0f);
    printGLf(glw->font,"Patch popsize: 0");
    glRasterPos2f(15.0f,35.0f);
    strcpy(buf, "env: ");
    for(k=0; k<nTraits; k++) {
      sprintf(b, "%d ", Gen[Frame-1].p[i][j].l[k]);
      strcat(buf,b);
    }
    printGLf(glw->font,"%s",buf);
  } else {
    /* Normal Stats */
    glColor3ub(255,255,255);
    glRasterPos2f(15.0f,15.0f);
    printGLf(glw->font,"Patch popsize: %llu", Gen[Frame-1].p[i][j].ni);
    //glRasterPos2f(15.0f,35.0f);
    //printGLf(glw->font,"Carrying Capacity: %.1lf", Gen[Frame-1].p[i][j].acp/_b);
    glRasterPos2f(15.0f,35.0f);
    printGLf(glw->font,"Fitness: %.4lf", Gen[Frame-1].p[i][j].acp); // originally .amf
    glRasterPos2f(15.0f,55.0f);
    printGLf(glw->font,"Preference (av): %.4lf", Gen[Frame-1].p[i][j].apr); // originally .amm
    glRasterPos2f(15.0f,75.0f);
    strcpy(buf, "env: ");
    for(k=0; k<nTraits; k++) {
      sprintf(b, "%d ", Gen[Frame-1].p[i][j].l[k]);
      strcat(buf,b);
    }
    printGLf(glw->font,"%s",buf);
    glRasterPos2f(15.0f,95.0f);
    strcpy(buf, "x: ");
    for(k=0; k<nTraits; k++) {
      sprintf(b, "%.2lf ", Gen[Frame-1].p[i][j].af[k]);
      strcat(buf,b);
    }
    printGLf(glw->font,"%s",buf);
    glRasterPos2f(15.0f,115.0f);
    strcpy(buf, "y: ");
    for(k=0; k<nTraits; k++) {
      sprintf(b, "%.2lf ", Gen[Frame-1].p[i][j].am[k]);
      strcat(buf,b);
    }
    printGLf(glw->font,"%s",buf);
    // Print mating and marker
    glRasterPos2f(15.0f,135.0f);
    if( Sexual ) {
      if( Selection ) {
        printGLf(glw->font, "c: %.3lf m: %.3lf f: %.3lf",
           Gen[Frame-1].p[i][j].m, Gen[Frame-1].p[i][j].k, Gen[Frame-1].p[i][j].f);
      } else {
        printGLf(glw->font, "c: %.3lf m: %.3lf", 
           Gen[Frame-1].p[i][j].m, Gen[Frame-1].p[i][j].k);
      }
    }
  }

}

/***************************************************************************************
 * Scene drawing code.  Component layout mostly
 ***************************************************************************************/

static void DrawScene(GLWindow *glw)
{
  Plot.x = 1;
  Plot.y = 480;
  Plot.w = 500;
  Plot.h = 500;

  /* Ugly hackish bugfix */
  if(!Frame) Frame = 1;
  if(Frame > nGen) Frame = nGen;

  /* Clear the old scene */
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Attache 3d plot draw function here
  Draw3DDeme                    (glw);

  /* Draw the deme graph */
  DrawDemeGraph                 (glw, 0.5f, 1.0f);

  /* Draw the niche histogram */

#if READ_HISTO
  DrawSpeciesHistogramI         (glw, Width*Scale+500.0f,   1.0f);
  // this was for SpeciesID>100
  // DrawSpeciesHistogramII     (glw, Width*Scale+500.0f, 101.0f);
#endif
#if _NEWRM
  DrawTrait                     (glw, Width*Scale+500.0f, 520.0f+NICHES*(20+HS) + 20);
  //(glw, Width*Scale+500.0f, 315.0f);
#endif
#if _SPVY
  DrawPreferenceGraphIII        (glw, Width*Scale+500.0f, 110.0f);
#else
  /* Draw preference graph (Cramer's graph) */
  DrawPreferenceGraph           (glw, Width*Scale+500.0f, 210.0f);
  DrawNicheHistogram            (glw, Width*Scale+780.0f, 200.0f+NICHES*(20+HS) + 20);
#endif
  if( KPN ){
    // Draw KPN and FPN matrices
    DrawKPN                     (glw, Width*Scale+210.0f, 460.0f+NICHES*(20+HS));
  }

  if( SPN ){
    // Draw SPN
    DrawSPN                     (glw, Width*Scale+500.0f, 430.0f+(20+HS));
  }

  if( MVK ){
    // Draw MVK matrix (on the right)
    DrawMVK                     (glw, Width*Scale+230.0f+NICHES*(20+HS), 460.0f+NICHES*(20+HS));
  }

  // Draw the text-based statistics
  DrawTextStats                 (glw, Width*Scale+20.0f, 340.0f);
#if READ_HABITAT
  DrawHabitatStats              (glw, Width*Scale+60.0f, 340.0f);
#endif
  //DrawSpeciesLegend           (glw, 0.0f, Height*Scale+220.0f);
  //DrawSpeciesLegend           (glw, 0.0f, Height*Scale+50.0f);

  // Draw gneration slider
  DrawGenerationSlider          (glw, 0.5f, Height*Scale+11.0f);
  //DrawGenerationSlider        (glw, Width*Scale+20.0f, Height*Scale+340.0f);
  DrawGenerationSlider          (glw, Width*Scale+20.0f, 320.0f);

#if !SIMPLE
  /* Draw hover-box */
  DrawHoverBox(glw);

  /* Draw summary graph */
  //DrawSummaryGraph            (glw, 0.0f, Height*Scale+45.0f);
  //DrawRangeMean               (glw, Width*Scale+20.0f, 1.0f);
  //DrawMatch                   (glw, Width*Scale+20.0f, 1.0f);
  //DrawFitness                 (glw, Width*Scale+20.0f, 1.0f);
  DrawRangeMax                  (glw, Width*Scale+20.0f, 1.0f);
  //DrawRangeMaxSp              (glw, Width*Scale+20.0f, 310.0f);
  //DrawRangeMaxGn              (glw, Width*Scale+20.0f, 465.0f);
  //DrawRangeNum                (glw, Width*Scale+20.0f, 620.0f);
  //DrawPopSize                 (glw, Width*Scale+20.0f, 155.0f);
  //DrawRangeNum                (glw, Width*Scale+20.0f, 310.0f);
  DrawSummaryGraph              (glw, Width*Scale+20.0f, 155.0f);

#endif

  /* Swap to display everything */
  glXSwapBuffers(glw->dpy, glw->win);
}

/***************************************************************************************
 * Window creation and event handler
 ***************************************************************************************/

void Deme_Down(const int x, const int y)
{
  // We only care about mouse down events within the widget area
    if( (x > 1) && (x < 501) && 
        (y > 480) && (y < 980) ) {
        // Mouse rotation controll
        if( !Plot.d ) {
          Plot.d  = 1;
          Plot.ox = Plot.nx = x;
          Plot.oy = Plot.ny = y;
        }
  }
}

void Deme_Up(const int x, const int y)
{
  if( Plot.d ) {
    Plot.d  = 0;
    Plot.nx = x;
    Plot.ny = y;
    Plot.rot.z += Plot.nx - Plot.ox;
    Plot.rot.x += Plot.ny - Plot.oy;
    Plot.ox = Plot.nx;
    Plot.oy = Plot.ny;
  }
}

void Deme_Move(const int x, const int y)
{
  // Mouse rotation controll
  if( Plot.d ) {
    Plot.nx = x;
    Plot.ny = y;
  }
}

void Graph()
{
  XEvent event;  GLWindow glw;  KeySym key;  
  char keys[255],d=0,b[1024],b_nGen[1024];  char *title = "niche_view";
  double st=0,ct=0;  struct timeval tv;
  int i,j,t,done=0,x=0,y=0,in=0,oin=0,update=1;
  FILE *gnuplot;

  /* Create window and initialize it */
  Frame  = (int)nGen;
  glw.id = 0;
  CreateGLWindow(&glw, title, X, Y);
  InitGLWindow(&glw);

  /* Process events */
  while (!done){
    if (XCheckIfEvent(glw.dpy, &event, AnyEvent, NULL)){
      switch (event.type) {
      case Expose:
	if (event.xexpose.count != 0)
	  break;
	update = 1;
	break;
      case ConfigureNotify:
	if ((event.xconfigure.width != glw.width) || (event.xconfigure.height != glw.height)) {
	  glw.width = event.xconfigure.width;
	  glw.height = event.xconfigure.height;
	  ResizeGLScene(event.xconfigure.width, event.xconfigure.height);
	}
	break;
      case ClientMessage:    
	if (*XGetAtomName(glw.dpy, event.xclient.message_type) == *"WM_PROTOCOLS")
	  done = 1;
	break;
      case KeyPress:
	if( XLookupString(&event.xkey, keys, 255, &key, 0) == 1 ) {
	  switch (*keys){
	  case '+': 
	    Speed++; 
	    if (Speed <  0) Speed =  0;
	    if (Speed > 60) Speed = 60;
	    update = 1;
	    break;
	  case '-':
	    Speed--; 
	    if (Speed <  0) Speed =  0;
	    if (Speed > 60) Speed = 60;
	    update = 1;
	    break;
	  case '<':
	    if( --Scale < 1 )
	      Scale = 1;
	    update = 1;
	    break;
	  case '>':
	    Scale++;
	    update = 1;
	    break;
	  case ',':
	    if(Frame > 1) Frame--;
	    update = 1;
	    break;
	  case '.':
	    if(Frame < nGen) Frame++;
	    update = 1;
	    break;
	  case ' ':
	    Speed = 0;
	    update = 1;
	    break;
	  case 'c':
	    ColorMode ^= 1;
	    update = 1;
	    break;
	  }
	}
	break;
      case ButtonRelease:
	// Attache 3d plot button here
	Deme_Up(event.xbutton.x,event.xbutton.y);
	d = 0;
	break;
      case MotionNotify:
	// Attache 3d plot button here
	Deme_Move(event.xbutton.x,event.xbutton.y);
	Mx = event.xbutton.x;
	My = event.xbutton.y;
	if(d) {
	  Trans[0] += event.xbutton.x-x;
	  Trans[1] += event.xbutton.y-y;
	  x = event.xbutton.x;
	  y = event.xbutton.y;
	  Frame = (((double)x)/(Width*Scale))*nGen;
	  update = 1;
	}
	if( (Mx >= 0) && (Mx < Width*Scale) && (My >=0) && (My < Height*Scale) ) {
	  in = 1;
	  update = 1;
	} else {
	  in = 0;
	}
	if( oin != in ) {
	  update = 1;
	  oin = in;
	}
	break;
      case ButtonPress:
	// Attache 3d plot button here
	Deme_Down(event.xbutton.x,event.xbutton.y);
	switch (event.xbutton.button){
	case 1:
	  x = event.xbutton.x; 
	  y = event.xbutton.y;
	  // Mouse down over the niche vs. marker graph has special meaning
	  if( (x > Width*Scale+20.0f) && (x < NICHES*(20+HS)+Width*Scale+20.0f) &&
	      (y > 340.0f+NICHES*(20+HS)+20) && (y < 8*(20+HS)+340.0f+NICHES*(20+HS)+20) ) {
	    i = (int)((x-(Width*Scale+20.0))/(20.0+HS));
	    if( !(gnuplot=fopen("gnuplotdata","w")) ) {
	      printf("Could not open gnuplot file!\n");
	      exit(7);
	    }
	    for(j=0; j<KPNy; j++)
	      fprintf(gnuplot,"%d %lf\n",j,(((double)Gen[Frame-1].kpn[i][j])/Gen[Frame-1].mkpn));
	    fclose(gnuplot);
	    if( !(gnuplot=fopen("gnuplotcommands","w")) ) {
	      printf("Could not open gnuplot file!\n");
	      exit(8);
	    } 
	    fprintf(gnuplot,"plot \"gnuplotdata\" with boxes\n");
	    fclose(gnuplot);
	    system("gnuplot -persist gnuplotcommands");
	    system("rm gnuplotdata gnuplotcommands");
	    update = 1;
	  }
	  // Mouse down over the niche histogram has special meaning
	  if( (x > Width*Scale+20.0f) && (x < NICHES*(20+HS)+Width*Scale+20.0f) &&
	      (y > 220) && (y<320) ) {
	    Filter[(int)((x-(Width*Scale+20.0f))/(20+HS))] ^= 1;
	    update = 1;
	  }
	  // Slider
	  if( (x < Width*Scale) && (y > Height*Scale+10) && (y < Height*Scale+40) ) {
	    d = 1;
	    Frame = (((double)x)/(Width*Scale))*nGen;
	    update = 1;
	  }
	  break;
	case 3: 
	  Frame = 0;
	  gettimeofday(&tv,NULL);
	  st = tv.tv_sec + tv.tv_usec/1000000.;
	  break;
	case 4:
	  if( --Scale < 1 )
	    Scale = 1;
	  update = 1;
	  break;
	case 5:
	  Scale++;
	  update = 1;
	  break;
	}
      }
    } else {
      /* Dump if needed and exit */
      if(dump) {
	if(dump < 0) dump = nGen-1;
	t = Frame;
	for(i=0; i<nGen; i++) {
	  if( !dump || ( !(i%dump) && (i || dump!=nGen-1) ) ) {
	    Frame = i+1;
	    if( !(PSFile0 = PS_Start("spc.eps",(nTraits)/2.+.25,(nTraits)/2.+.25)) ) {
	      fprintf(stderr,"Could not open output post-script file! (tmp1.eps)\n");
	      exit(1);
	    }
	    if( !(PSFile1 = PS_Start("deme.eps",Width/2.,Height/2.)) ) {
	      fprintf(stderr,"Could not open output post-script file! (tmp0.eps)\n");
	      exit(1);
	    }
	    DrawScene(&glw);
	    sprintf(b,"%llu",Gen[Frame-1].g);
	    sprintf(b_nGen,"%llu",nGen);
	    t = strlen(b);
	    b[0] = 0;
	    while(t++<strlen(b_nGen))
	      strcat(b,"0");
	    sprintf(b_nGen,"%s",b);
	    PS_End(PSFile0);
	    PS_End(PSFile1);
	    PSFile0 = NULL;
	    PSFile1 = NULL;
	  }
	}
	Frame = t;
	exit(0);
      }
      if ( (Frame < nGen) && (Speed) ){
	gettimeofday(&tv,NULL);
	ct = tv.tv_sec + tv.tv_usec/1000000.;
	if( (!d) && (ct-st > 1./Speed) ) {
	  Frame++;
	  DrawScene(&glw);
	  update = 0;
	  st = ct;
	} else {
	  /* Not ready to draw next frame yet / dragging */
	  // Updated if needed, otherwise yeild.
	  if( update ) {
	    DrawScene(&glw);
	    update = 0;
	  } else {
	    DrawScene(&glw);
	    yeild();
	  }
	}
      } else {
	/* Not ready to draw next frame yet / stopped */
	// Updated if needed, otherwise yeild.
	if( update ) {
	  DrawScene(&glw);
	  update = 0;
	} else {
	  DrawScene(&glw);
	  yeild();
	}
      }
    }

  }

  // We are done, kill the window
  KillGLWindow(&glw);
}

/***************************************************************************************
 * Application entry point
 ***************************************************************************************/

void DumpDetailed()
{
  int i;

  for(i=0; i<nGen; i++) {
    printf("%lf ",Gen[i].cf);                                      // Current Fitness:          red 
    printf("%lf ",Gen[i].amc);                                     // Average mating character: purple
    printf("%lf ",((double)Gen[i].tns)/((((KPNy-1)/4)+1)*NICHES)); // Number of species:        yellow
    printf("\n");
  }
}

void Dumpall()
{
  if( nGen ) {
    Frame = nGen;
  } else {
    printf("error: frame < 0.\n");  
    return;
  }

  printf("Gen:%llui\n", Gen[Frame-1].g);          // Generation
  printf("pop:%llui\n",  Gen[Frame-1].Popsize);   // MaxPopSize

  int l, z, count, ccount;
  count  = 0;
  ccount = 0;
  z      = 0;
  for(l=0; l<(((int)(pow((double)(2),(double)(nTraits))))+1); l++){
    if (Gen[Frame-1].range.mrange[l] > 0){
      count++;
    }
  }

  /*
  // not necessray to print this, looks at positive ranges at the end of simulation 
  for(l=0; l<(((int)(pow((double)(2),(double)(nTraits))))+1); l++){
    if (Gen[Frame-1].range.idspecies[l] != 0){
      printf("species%dmaxrange: %di\nspecies%dmaxduration: %di\n", 
             l, Gen[Frame-1].range.mrange[l], l, z);
    } else {
      printf("species%dmaxrange: %di\nspecies%dmaxduration: %di\n",
             l, 0, l, z);
    }
  }
  */

  printf("totalco: %ff, maxco: %ff, meanco: %ff, nooftc: %ff\n", Coexisttotal, Coexistmax, Coexistmean, Coexistcount);
  printf("totalms: %ff, maxms: %ff, meanms: %ff, nooftms: %ff\n", Multitotal, Multimax, Multimean, Multicount);
  printf("finaltsno: %di\n", count);
  for(z=0; z<16; z++){
    printf("species%drangeattheendofsim: %di\n", z, Gen[Frame-1].range.mrange[z]);
  }
  printf("maxatonce: %di\n", Maxmultinos);
  printf("timetoecologicalsp: %di\n", Timetospec);
  for(z=0; z<16; z++){
    printf("species%dtspec: %di\n", z, Tspec[z]);
  }
  printf("timetosexualsp: %di\n", Tsexuals);
  int times = 0;
  for(z=0; z<16; z++){
    times = Tss[z];
    printf("species%drange: %di-tspec: %di-maxnoofsexualspecies: %di-noofsexualspeciesatend: %di\n", z, Gen[times].range.mrange[z], Tss[z], Maxss[z], Gen[Frame-1].nss[z]);
  }
  for(l=0; l<(((int)(pow((double)(2),(double)(nTraits))))+1); l++){
    if( Tspec[l] > 0 ){
      ccount++;
    }
  }
  printf("totalsp: %di\n", ccount);
  printf("resources:%di\n", Gen[Frame-1].hab.tcount);
  printf("Simpson's:%ff\n", Gen[Frame-1].hab.tsip);
  printf("Percentedges:%ff\n", Gen[Frame-1].hab.frag);
  printf("max/mean(resource%di): %di, %ff\n", Gen[Frame-1].hab.idpatch[0]+1, Gen[Frame-1].hab.mpatchs[0], Gen[Frame-1].hab.meanpatchs[0]);
  printf("max/mean(resource%di): %di, %ff\n", Gen[Frame-1].hab.idpatch[1]+1, Gen[Frame-1].hab.mpatchs[1], Gen[Frame-1].hab.meanpatchs[1]);
  printf("max/mean(resource%di): %di, %ff\n", Gen[Frame-1].hab.idpatch[2]+1, Gen[Frame-1].hab.mpatchs[2], Gen[Frame-1].hab.meanpatchs[2]);
  printf("max/mean(resource%di): %di, %ff\n", Gen[Frame-1].hab.idpatch[3]+1, Gen[Frame-1].hab.mpatchs[3], Gen[Frame-1].hab.meanpatchs[3]);

  WriteMVK();
  WriteKPN();
  WriteSPN();
  WriteDemes();
  WriteLandscape();
}

void UseageError()
{
  fprintf(stderr,"Useage\n\tview <dataset> [-x <width>] [-y <height>] [-dump [interval]]\n");
  exit(1);
}

int main(int argc, char **argv)
{
  int i;

  /* Check command line args and read data file */
  if ( (argc < 2) || (argc > 7))
    UseageError();
  FNPrefix = strdup(argv[1]);
  for(i=2; i<argc; i++) {
    if(!strcmp(argv[i],"-x")) {
      /* width of window */
      if(i+1 >= argc)
        UseageError();
      if(sscanf(argv[i+1],"%d",&X) != 1)
        UseageError();
      i++;
    }
    if(!strcmp(argv[i],"-y")) {
      /* height of window */
      if(i+1 >= argc)
        UseageError();
      if(sscanf(argv[i+1],"%d",&Y) != 1)
        UseageError();
      i++;
    }
    if(!strcmp(argv[i],"-dump")) {
      dump = -1;
      /* silent/dump mode */
      if(i+1 < argc)
        if(sscanf(argv[i+1],"%d",&dump) == 1)
          i++;
    }
    if(!strcmp(argv[i],"-amc")) {
      // Dump the number of generations until amc >= .5
      if( (i=readDataFile(argv[1])) ) {
        printf("-1\n");
        exit(i);
      }
      printf("%d\n",Amcg);
      return 0;
    }
    if(!strcmp(argv[i],"-gens")) {
      // Dump the number of generations in the data file
      if( (i=readDataFile(argv[1])) ) {
        printf("-1\n");
        exit(i);
      }
      printf("%llu\n",Gen[nGen-1].g);
      return 0;
    }
    if(!strcmp(argv[i],"-clean")) {
      // Dump a flag for whether or not the data files represent a run that exited cleanly.
      // Really, this dumps 1 if the trailer is present or 0 otherwise
      if( (i=readDataFile(argv[1])) ) {
        printf("-1\n");
        exit(i);
      }
      printf("%d\n",Clean);
      return 0;
    }
    if(!strcmp(argv[i],"-spcs")) {
      // Dumps the number of species in the last gen
      if( (i=readDataFile(argv[1])) ) {
        printf("-1\n");
        exit(i);
      }
      printf("%d\n",Gen[nGen-1].tns);
      return 0;
    }
    if(!strcmp(argv[i],"-fn")) {
      // Dumps the number of "filled" niches in the last gen
      if( (i=readDataFile(argv[1])) ) {
        printf("-1\n");
        exit(i);
      }
      printf("%d\n",Gen[nGen-1].fn);
      return 0;
    }
    if(!strcmp(argv[i],"-kpnvar")) {
      // Dumps the average variance of the even ... read the changelog/code ...
      if( (i=readDataFile(argv[1])) ) {
        printf("-1\n");
        exit(i);
      }
      printf("%lf\n",Gen[nGen-1].vark);
      return 0;
    }
    if(!strcmp(argv[i],"-spcspfn")) {
      // Dumps the number of species per filled niche in the last gen
      if( (i=readDataFile(argv[1])) ) {
        printf("-1\n");
        exit(i);
      }
      printf("%lf\n",Gen[nGen-1].spfn);
      return 0;
    }
    if(!strcmp(argv[i],"-dumpall")) {
      // Dumps a large number of statistics in html format
      if( (i=readDataFile(argv[1])) ) {
        printf("Read of data file failed.\n");
        exit(i);
      }

      WriteGNUPlot();
      WriteResource();
      WritePopSizePlot();
      WriteSummaryPlot();
      WriteTrans();
      WriteXvss();
      CountDuration();
      CountSpecies();
#if DETAILED_HISTO
      WriteTraitPlot();
#endif
      Dumpall();

      //printf("%f, %f, %f, %f\n", Coexistmean, Coexistmax, Coexisttotal, Coexistcount);
      return 0;
    }
    if(!strcmp(argv[i],"-dumpdetailed")) {
      // Dumps a large number of statistics in html format
      if( (i=readDataFile(argv[1])) ) {
        printf("Read of data file failed.\n");
        exit(i);
      }
      DumpDetailed();
      return 0;
    }
  }

  // read data
  if( (i=readDataFile(argv[1])) )
    exit(i);

  // Run the graph
  Graph();

  // Return success
  return 0;
}
