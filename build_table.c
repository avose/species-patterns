// gcc -Wall -O2 -o build_table build_table.c

#include <stdio.h>
#include <stdlib.h>
#include <math.h> 
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

////////////////////////////////////////////////////////////

// Flags for the result type field
#define RT_HIDDEN 1
#define RT_FLOAT  2
#define RT_INT    3
#define RT_BOOL   4

////////////////////////////////////////////////////////////

typedef struct {
  char   *name;
  double  value;
} param;

typedef struct {
  char   *name;
  double  value;
  char   *string;
  int     type;
} result;

typedef struct {
  // Invalid flag
  int      iv;
  // Arbiter index number
  int      w;
  // Data points / stats
  result  *results;
  int      nresults;
  // Image data
  char   **images;
  int      nimages;
  // Raw data
  char   **data;
  int      ndata;
  // Files
  char    *chkpntf;
  char    *rsltf;
} run;

typedef struct {
  // Parameters
  char    *ps;
  param   *params;
  int      nparams;
  // Data points / stats
  result  *results;
  int      nresults;
  // Runs
  run     *runs;
  int      nruns;
  // File
  char    *setf;
} set;

typedef struct {
  char    *jobname;
  set     *sets;
  int      nsets;
} result_data;

////////////////////////////////////////////////////////////

result_data Results;
int         Nparams;

static int  FindSet(char *ps);
static int  AddSet(char *ps);
static void ParseParameterString(set *s, char *ps);
static void ParseSetLine(char *line);
static void ParseRunLine(char *line); 
static void ParseCheckpointLine(char *line);
static void MountResultball(char *fn);
static int  FindLogFile(char *fn);
static int  BuildImages(set *s, run *r, char *fn);
static int  BuildStats(set *s, run *r, char *fn);
static int  UnpackRun(run *r);
static void AquireData();
static void ProcessData();
static void WriteHTML();
static void BuildTable();
static void UsageError();
static void Cleanup();
       int  main(int argc, char **argv);

////////////////////////////////////////////////////////////

static int FindSet(char *ps)
{
  int i;

  // Search for matching parameter string
  for(i=0; i<Results.nsets; i++) {
    if( !strcmp(ps,Results.sets[i].ps) )
      return i;
  }

  // Return not found
  return -1;
}

static int AddSet(char *ps)
{
  set *s;  char buf[256];

  // Set doesn't exist yet; add it
  if( !(Results.sets = (set*)realloc(Results.sets, (Results.nsets+1)*sizeof(set))) ) {
    fprintf(stderr,"Could not allocate space for a new set.\n");
    exit(1);
  }
  s = &Results.sets[Results.nsets++];
  s->runs     = NULL;
  s->nruns    = 0;
  s->results  = NULL;
  s->nresults = 0;

  // Copy set file name to new set structure
  sprintf(buf,"%s.par.tar.gz",ps);
  if( !(s->setf=strdup(buf)) ) {
    fprintf(stderr,"Could not allocate space for a new set filename.\n");
    exit(1);
  }

  // Copy the parameter string into the set
  if( !(s->ps=strdup(ps)) ) {
    fprintf(stderr,"Could not allocate space for a new set parameter string.\n");
    exit(1);
  }

  // Parse the parameter string into the set's data structure
  ParseParameterString(s,ps);

  // Return new set index
  return Results.nsets-1;
}

////////////////////////////////////////////////////////////

static void ParseParameterString(set *s, char *ps)
{
  char *t;

  // Parse the parameter names and values from the string
  if( !(t=strtok(ps, "-")) ) {
    fprintf(stderr,"Error parsing parameter string start.\n");
    exit(1);
  }
  s->nparams = 1;
  if( !(s->params = (param*)malloc(sizeof(param))) ) {
    fprintf(stderr,"Could not allocate space for a new parameter entry.\n");
    exit(1);
  }
  if( !(s->params[0].name=strdup(t)) ) {
    fprintf(stderr,"Could not allocate space for a new parameter name.\n");
    exit(1);
  }
  while(1) {
    // Read param value
    if( !(t=strtok(NULL, "-")) ) {
      fprintf(stderr,"Error parsing parameter string middle.\n");
      exit(1);
    } 
    if( sscanf(t,"%lf",&s->params[s->nparams-1].value) != 1 ) {
      fprintf(stderr,"Error parsing parameter string middle value.\n");
      exit(1);
    }
    // Read next param name
    if( !(t=strtok(NULL, "-")) ) {
      if( !Nparams ) Nparams = s->nparams;
      if( Nparams != s->nparams ) {
	fprintf(stderr,"Error: Nparams mismatch.\n");
	exit(1);
      }
      break;
    }
    // Install new param with new name
    s->nparams++;
    if( !(s->params = (param*)realloc(s->params,s->nparams*sizeof(param))) ) {
      fprintf(stderr,"Could not allocate space for a new parameter entry middle.\n");
      exit(1);
    }
    if( !(s->params[s->nparams-1].name=strdup(t)) ) {
      fprintf(stderr,"Could not allocate space for a new parameter name middle.\n");
      exit(1);
    }
  }
}

static void ParseSetLine(char *line)
{
  char *a,*b,*t,ps[256];

  // Verify complete ending
  if( !(b=strstr(line,".par.tar.gz")) ){
     fprintf(stderr,"Error parsing set line: incorrect suffix.\n");
     exit(1);
  }

  // Build a parameter string for this set
  a = line;
  for(t=ps; a<b; a++,t++)
    *t = *a;
  *t = '\0';

  // See if this set is already in the list of sets
  if( FindSet(ps) == -1 )
    AddSet(ps);
}

static void ParseRunLine(char *line) 
{
  char *a,*b,*c,*t,ps[256];  int rn,i;  run *r;  set *s;

  // Verify complete ending and prefix
  if( !(b=strstr(line,".par_")) ) {
     fprintf(stderr,"Error parsing run line: incorrect suffix.\n");
     exit(1);
  }
  if( !(a=strstr(line,"_")) ) {
     fprintf(stderr,"Error parsing run line: incorrect prefix.\n");
     exit(1);
  }
  a++;

  // Read in run # from end of string
  c = b + 5;
  if( sscanf(c,"%d.tar.gz",&rn) != 1 ) {
    fprintf(stderr,"Error parsing run line: run number.\n");
    exit(1);
  }

  // Build a parameter string for this set
  for(t=ps; a<b; a++,t++)
    *t = *a;
  *t = '\0';

  // If this set is not already in the list of sets, add it.
  if( (i=FindSet(ps)) == -1 )
    i = AddSet(ps);

  // Add the run to the set
  s = &Results.sets[i];
  s->nruns++;
  if( !(s->runs=(run*)realloc(s->runs,s->nruns*sizeof(run))) ) {
    fprintf(stderr,"Could not allocate space for a new run.\n");
    exit(1);
  }
  r = &s->runs[s->nruns-1];
  r->iv       = 0;
  r->w        = rn;
  r->chkpntf  = NULL;
  r->results  = NULL;
  r->nresults = 0;
  if( !(r->rsltf=strdup(line)) ) {
    fprintf(stderr,"Could not allocate space for a new run filename.\n");
    exit(1);
  }
}

static void ParseCheckpointLine(char *line)
{
  // !!av: Fill this in later!
}

static void MountResultball(char *fn)
{
  char *a,*b,*t,buf[256];  FILE *cmd;

  // Get stdout started off
  printf("Evaluating result tarball: ");
  fflush(stdout);

  // Extract result tarball, parsing output
  sprintf(buf,"tar -zxvf %s",fn);
  if( !(cmd=popen(buf, "r")) ) {
    fprintf(stderr,"Could not execute tar.\n");
    exit(1);
  }

  // Parse output
  buf[0] = '\0';
  while( fgets(buf,sizeof(buf),cmd) ) {
    // Find the end of the job name
    a = buf;
    if( !(b = strstr(buf,"/")) ) {
      fprintf(stderr,"Error parsing tar output: no '/'.\n");
      exit(1);
    }
    // Read / Verify the job name
    if( !Results.jobname ) {
      // Jobname not set, allocate space for and set it.
      if( !(Results.jobname = (char*) malloc(b-a+1)) ) {
	fprintf(stderr,"Could not allocate space for job name.\n");
	exit(1);
      }
      for(t=Results.jobname; a<b; a++,t++)
	*t = *a;
      *t = '\0';
    } else {
      // Test to make sure jobname matches
      for(t=Results.jobname; a<b; a++,t++)
	if( *a != *t ) {
	  fprintf(stderr,"Error parsing tar output: job name mismatch.\n");
	  exit(1);
	}
    }
    // Classify the line type by substring matching
    b++;
    if( b[0] && (b[strlen(b)-1] == '\n') )
      b[strlen(b)-1] = '\0';
    if( strstr(b,".par.") ) {
      ParseSetLine(b);
    } else if( strstr(b,"-chkpnt") ) {
      ParseCheckpointLine(b);
    } else if( strstr(b,".par_") ) {
      ParseRunLine(b);
    } else if( b[0] == '\0' ) {
      // Skip this header line
    } else {
      fprintf(stderr,"Error parsing tar output: trash files in resultball.\n");
      fprintf(stderr,"%s\n",b);
      exit(1);
    }

  }
  if( !feof(cmd) || !buf[0] ) {
    fprintf(stderr,"Error parsing tar output: EOF.\n");
    exit(1);
  }

  // Close the pipe to tar's output
  pclose(cmd);

  // Change into the result's subdir
  if( chdir(Results.jobname) == -1 ) {
    fprintf(stderr,"Could not change to result data directory.\n");
    exit(1);
  }

  // Print out finished comment
  printf("Done.\n\n");
}

////////////////////////////////////////////////////////////

static int FindLogFile(char *fn)
{
  DIR *d;  struct dirent *de;  int num;

  // Open current dir for file list
  if( !(d=opendir(".")) ) {
    fprintf(stderr,"Could not open result directory.\n");
    exit(1);
  }

  // Scan for any files that look like they're result files
  while( (de=readdir(d)) ) {
    if( strstr(de->d_name,".sys") ) {
      // Result file found
      if( sscanf(de->d_name,"%d.sys",&num) != 1 ) {
        fprintf(stderr,"Error finding result file.");
        exit(1);
      }
      sprintf(fn,"%d",num);
      closedir(d);
      return 1;
    }
  }

  // Close the current dir and return failure
  closedir(d);
  return 0;
}

static int BuildStats(set *s, run *r, char *fn)
{
  FILE *cmd;  char buf[256],*a,c;  double v;  int p;

  // Copy viewer into result dir
  system("cp ~/range/view .");
  system("cp ~/range/plot .");
  system("cp ~/range/durationplot .");
  system("cp ~/range/maxdurationplot .");
  system("cp ~/range/coexistenceplot .");
  system("cp ~/range/popsizeplot .");
  system("cp ~/range/summaryplot .");
  system("cp ~/range/traitplot .");
  system("cp ~/range/transplot2 .");

  // Open a pipe to the output of the sim in numerical dump mode
  sprintf(buf,"./view %s -dumpall",fn);
  if( !(cmd=popen(buf, "r")) ) {
    fprintf(stderr,"Could not execute tar.\n");
    exit(1);
  }

  // Parse output of sim to build stats table
  while( fgets(buf,sizeof(buf),cmd) ) {
    // Make sure the data line has the correct format
    if( !(a=strstr(buf,":")) ) {
      pclose(cmd);
      return 0;
    }
    // Read in value
    if( sscanf(a+1,"%lf%c%n",&v,&c,&p) != 2 ) {
      pclose(cmd);
      return 0;
    }
    switch(c) {
    case '_':
      // File is "hidden".  Put this value in the sub tables with
      // runs, but not in the master table
      c = RT_HIDDEN;
      break;
    case 'f':
      // Float value for the master table: print with decimal places
      c = RT_FLOAT;
      break;
    case 'i':
      // Integer value for the master table: print without decimals
      c = RT_INT;
      break;
    case 'b':
      // Boolean flag: print in master table in count/runs form.
      c = RT_BOOL;
      break;
    default:
      // No type was given or unknown type, assume this is a float
      c = RT_FLOAT;
      break;
    }

    // Read in result name
    *a = '\0';
    // Allocate a new result struct and fill it in
    r->nresults++;
    if( !(r->results=(result*)realloc(r->results,r->nresults*sizeof(result))) ) {
      fprintf(stderr,"Error allocating space for a new result struct.\n");
      exit(1);
    }

    r->results[r->nresults-1].name   = strdup(buf);
    r->results[r->nresults-1].value  = v;
    r->results[r->nresults-1].type   = c;

    // See if there is an extra "custom" string at the end
    if( (a+1)[p] == '\n' ) {
      r->results[r->nresults-1].string = NULL;
    } else {
      r->results[r->nresults-1].string = strdup(&((a+1)[p]));
    }
  }
  if( !feof(cmd) ) {
    pclose(cmd);
    return 0;
  }

  // Close command pipe
  pclose(cmd);

  return 1;
}

#define NIMAGES 11
#define NDATA   3

//
// This function requires that BuildStats() be called beforehand.
//
static int BuildImages(set *s, run *r, char *fn)
{
  char *images[] = {"landsfig.eps", "demesfig.eps", "plot.eps", "durationfig.eps", "coexist.eps", "kpn.eps", "mvk.eps", "summary.eps", "trans.eps", "spn.eps", "resource.eps"};
  char *data[]   = { "durationdata", "maxdurationdata", "traitdata" };
  char  buf[256];
  int   i,rv;

  // Allocate space for image metadata
  r->nimages = NIMAGES;
  if( !(r->images = (char**)malloc(r->nimages*sizeof(char*))) ) {
    fprintf(stderr,"Could not allocate space for image name pointers.\n");
    exit(1);
  }

  // Copy them to a reasonable persistant name/location
  for(i=0; i<r->nimages; i++) {
    sprintf(buf,"convert %s ../%s-%s-%d.png",images[i],images[i],s->ps,r->w);
    if( ((rv=system(buf)) == -1) || WEXITSTATUS(rv) ) {
      // Return failure
      return 0;
    }
    // Register image with the memory data structures.
    sprintf(buf,"%s-%s-%d.png",images[i],s->ps,r->w);
    r->images[i] = strdup(buf);
  }

  // Allocate space for data metadata
  r->ndata = NDATA;
  if( !(r->data = (char**)malloc(r->ndata*sizeof(char*))) ) {
    fprintf(stderr,"Could not allocate space for data name pointers.\n");
    exit(1);
  }

  // Copy them to a reasonable persistant name/location
  for(i=0; i<r->ndata; i++) {
    sprintf(buf,"cp %s ../%s-%s-%d.data",data[i],data[i],s->ps,r->w);
    if( ((rv=system(buf)) == -1) || WEXITSTATUS(rv) ) {
      // Return failure
      return 0;
    }
    // Register data with the memory data structures.
    sprintf(buf,"%s-%s-%d",data[i],s->ps,r->w);
    r->data[i] = strdup(buf);
  }

  // Return success
  return 1;
}

static int UnpackRun(run *r)
{
  char buf[256];  int rv;

  // Untar the file
  sprintf(buf,"tar -zxf %s",r->rsltf);
  if( ((rv=system(buf)) == -1) || WEXITSTATUS(rv) ) {
    // Return failure
    return 0;
  }

  // Return success
  return 1;
}

static void AquireData()
{
  int i,j,k;  char fn[256];  set *s;  run *r;  param *p;  char buf[256];

  for(i=0; i<Results.nsets; i++) {
    s = &Results.sets[i];
    printf("setfn: %s\n",s->setf);
    printf("prmst: %s\n",s->ps);
    printf("nparm: %d\n",s->nparams);
    for(j=0; j<s->nparams; j++) {
      p = &s->params[j];
      printf("   %s = %lf\n",p->name,p->value);
    }
    printf("nruns: %d\n",s->nruns);
    for(j=0; j<s->nruns; j++) {
      r = &s->runs[j];
      printf("   run-#: %d\n",r->w);
      printf("   rsltf: %s\n",r->rsltf);
      // Extract run result file
      printf("   expnd: ");
      fflush(stdout);
      if( !UnpackRun(r) ) {
	fprintf(stderr,"Failure.\n   !! Skipping run.\n");
	r->iv = 1;
	continue;
      }
      printf("Done.\n");
      /*
      // Compile the viewer application, patching the parameters.h file first.
      printf("   cmpil: ");
      fflush(stdout);
      if( !(f=fopen("parameters.h","a")) ) {
	fprintf(stderr,"Failure patching parameters.\n   !! Skipping run.\n");
	r->iv = 1;
	continue;
      }
      fprintf(f,"\n\n#ifndef SCRPT_PATCH\n#define SCRPT_PATCH\n#undef U_ARBITER\n#define U_ARBITER 0\n#undef U_GUI\n#define U_GUI 1\n#endif\n");
      fclose(f);
      sprintf(buf,"mv parameters.h params ; cp -f ../../* . > /dev/null 2> /dev/null ; mv params parameters.h ; make > /dev/null 2> /dev/null");
      if( ((rv=system(buf)) == -1) || WEXITSTATUS(rv) ) {
	fprintf(stderr,"Failure.\n   !! Skipping run.\n");
	r->iv = 1;
	continue;
      }
      printf("Done.\n");
      */
      // Figure out the name of the log file.
      if( !FindLogFile(fn) ) {
	fprintf(stderr,"   Error locating log file.\n   !! Skipping run.\n");
	r->iv = 1;
	continue;
      }
      printf("   dataf: %s\n",fn);
      // Read statistical data
      printf("   statd: ");
      fflush(stdout);
      if( !BuildStats(s,r,fn) ) {
	fprintf(stderr,"Data Failure.\n   !! Skipping run.\n");
        //exit(1);
        r->iv = 1;
	continue;
      }
      printf("Done.\n");
      if( !BuildImages(s,r,fn) ) {
	fprintf(stderr,"Image Failure.\n   !! Skipping run.\n");
	r->iv = 1;
	continue;
      }
      for(k=0; k<r->nimages; k++) {
	printf("%s",r->images[k]);
	if( (k+1) < r->nimages )
	  printf(", ");
      }
      printf("\n");
      // Do some cleanup:
      sprintf(buf,"rm -f *.c *.o simulation ranges *.log checkpoint *.eps Changelog > /dev/null 2> /dev/null");
      system(buf);
    }
    printf("\n");
  }
}

static void ProcessData()
{
  int i,j,k,vr,valid;  set *s;  run *r;

  // Compute average of all results for each set
  for(i=0; i<Results.nsets; i++) {
    s = &Results.sets[i];
    valid = 0;
    // Skip empty sets
    if( !s->nruns )
      continue;

    // this part is added later to make sure that if the 1st run fails
    // there is still some other runs to copy the results from
    // as in the original code it was copying values, names, types etc from runs[0]
    // so go through the sets
    for(k=0; k<s->nruns; k++){
      r = &s->runs[k];
      if( !r->iv ){
        valid = k;
        break;
      }
    }
    // Clone a run's result size
    // here!!!
    // find first run that has ->iv == 0,  so finding the vr index
    // s->nresults = s->runs[vr].nresults;
    // original line:
    // s->nresults = s->runs[0].nresults;
    s->nresults = s->runs[valid].nresults;
    if( !(s->results=(result*)malloc(s->nresults*sizeof(result))) ) {
      fprintf(stderr,"Could not allocate space for set's average data.\n");
      exit(1);
    }
    // Clone a run's result names and zero set's result values for an average
    for(j=0; j<s->nresults; j++) {
      // original line: 
      // if( !(s->results[j].name=strdup(s->runs[0].results[j].name)) ) {
      if( !(s->results[j].name=strdup(s->runs[valid].results[j].name)) ) {
        fprintf(stderr,"Could not allocate space for set's average data name.\n");
        exit(1);
      }
      // original line: 
      // s->results[j].type  = s->runs[0].results[j].type;
      s->results[j].type  = s->runs[valid].results[j].type;
      s->results[j].value = 0.0;
    }
    // Sum for average
    for(j=0; j<s->nresults; j++) {
      vr = 0;
      for(k=0; k<s->nruns; k++) {
        r = &s->runs[k];
        if( !r->iv ) {
          s->results[j].value += r->results[j].value;
          vr++;
        }
      }
      // Divide for average
      s->results[j].value /= vr;
    }
  }
}

static void WriteHTML()
{
  FILE *t,*f;  int i,j,k,l,vr;  char line[1024],buf[256];  set *s;  run *r;

  //
  // Build set tables first
  //
  for(i=0; i<Results.nsets; i++) {
    s = &Results.sets[i];
    // Open
    sprintf(buf,"../%s.html",s->ps);
    if( !(f=fopen(buf,"w")) ) {
      fprintf(stderr,"Could not open/create set HTML output file.\n");
      exit(1);
    }
    // Write header
    fprintf(f,"<html><body>%s<br><br>\n\n",s->ps);

    // Write some description to make this page easier to understand
    fprintf(f,
            "<p> Color tones for each figure:<br>\n"
            "---gray:         not a species;<br>\n"
            "---red tones:    specialist (4 species: 1,2,4,8);<br>\n"
            "---blue tones:   2-resource generalist (6 species: 3,5,6,9,10,12);<br>\n"
            "---green tones:  3-resource generalist (4 species: 7,11,13,14);<br>\n"
            "---white:        4-resource generalist (1 species: 15).<br>\n\n"
            "Figure 1: Spatial landscape with array of patches, each of which contains a single resource.<br>\n"
            "There are four different resources shown in gray scale.<br>\n"
            "Figure 2: Same as in Fig.1 with populations shown as circles, the size of which is proportional to the population size.<br>\n"
            "Figure 3: maximum range sizes of species over time.<br>\n\n"
            "Figure 4: average species duration vs. mean maximum range.<br>\n\n"
            "Figure 5: no of species and coexistence of species sharing the same resource across time, black dots represent coexistence (boolean: 1 = coexistence).<br>\n\n"
            "Figure 6: male display trait and female preference trait in four different resource patches.)<br>\n"
            "Figure 7: male display trait vs. female choosiness trait.<br>\n\n"
            "Figure 8: summary plot (fitness, number of sexual species, etc).<br>\n\n"
            "Figure 9: x traits and preferences along with fitness and resource distribution across transects in the landscape.<br>\n\n"
            "Figure 10: male display trait and female preference trait per ecological species.<br>\n\n"
            "Figure 11: Row sum and column sum of resources across the landscape.<br>\n\n"
            "DATA: durationdata, maxdurationdata, traitdata<br>\n"
            "FIGURES: landsfig.eps, demesfig.eps, plot.eps, durationfig.eps, coexist.eps, kpn.eps, mvk.eps, summary.eps, trans.eps, spn.eps, resource.eps</p><br>\n"
           );

    // Write run images to this file
    for(j=0; j<s->nruns; j++) {
      r = &s->runs[j];
      if( r->iv ) 
	continue;
      fprintf(f,"%d <br>\n",r->w);
      for(k=0; k<r->nresults; k++) {
	fprintf(f,"%s: %.2lf ",r->results[k].name,r->results[k].value);
	if( r->results[k].string )
	  fprintf(f,"string: %s",r->results[k].string);
	fprintf(f,"<br>\n");
      }
      // here
      for(k=0; k<r->ndata; k++)
        fprintf(f,"<br><a href=\"%s.data\">data</a>\n ",r->data[k]);
      fprintf(f,"</br>\n");
      for(k=0; k<r->nimages; k++)
	fprintf(f,"<img src=\"%s\">&nbsp\n",r->images[k]);
      fprintf(f,"<br><br><br><br>\n");
    }
    // Close the HTML file
    fprintf(f,"\n</html></body>\n");
    fclose(f);
  }

  //
  // Build master table from template
  //
  if( !(t=fopen("../index.template","r")) ) {
    fprintf(stderr,"Could not open HTML template file.\n");
    exit(1);
  }
  if( !(f=fopen("../index.html","w")) ) {
    fprintf(stderr,"Could not open/create HTML output file.\n");
    exit(1);
  }

  // Copy template over to index, translating data tags
  while( fgets(line,sizeof(line),t) ) {
    // See if there is a parameter string match
    for(i=0; i<Results.nsets; i++) {
      s = &Results.sets[i];

      if( strstr(line,s->ps) ) {
        for(vr=-1,j=l=0; j<s->nruns; j++) {
          l |=  (!(s->runs[j].iv));
          if( !s->runs[j].iv && (vr == -1) ) {
            vr = j;
          }
        }
        if( l ) {
          // Found a match, translate this line
          fprintf(f,"<td align=\"center\" valign=\"middle\"><table align=\"center\" valign=\"middle\">");
          fprintf(f,"<tr><td><b>r:</b></td><td>%d</td></tr>",s->nruns);
          for(k=0,j=0; j<s->nresults; j++) {
            // Consider the result data type
            switch(s->results[j].type) {
            case RT_HIDDEN:
              // Skip "hidden" result data for main table
              continue;
            case RT_FLOAT:
              if( k%2 ) {
                //fprintf(f,"<td><b>%s:</b></td><td>%.1lf</td></tr>",s->results[j].name,s->results[j].value);
              } else {
                //fprintf(f,"<tr><td><b>%s:</b></td><td>%.1lf</td>",s->results[j].name,s->results[j].value);
              }
              k++;
              break;
            case RT_BOOL:
            case RT_INT:
              if( k%2 ) {
                //fprintf(f,"<td><b>%s:</b></td><td>%d</td></tr>",s->results[j].name,((int)s->results[j].value));
              } else {
                //fprintf(f,"<tr><td><b>%s:</b></td><td>%d</td>",s->results[j].name,((int)s->results[j].value));
              }
              k++;
              break;
            }
          }
          if( k%2 )
            fprintf(f,"<td></td><td></td></tr>");
          fprintf(f,"</table><img src=\"%s\"><br><img src=\"%s\"><br><a href=\"%s.html\">images</a></td>\n",s->runs[vr].images[0],s->runs[vr].images[1],s->ps);
          goto next_line;
        } else {
          // This set has no runs (no data).  Print out a stub.
          fprintf(f,"<td align=\"center\" valign=\"middle\">n/a</td>");
          goto next_line;
        }
      }
    }
    // No match, copy the line unchanged
    fprintf(f,"%s",line);
    // Skip over to next line
  next_line:
    continue;
  }
  if( !feof(t) ) {
    fprintf(stderr,"Error processing template file.\n");
    exit(1);
  }

  // Close the HTML table and template
  fclose(f);
  fclose(t);
}

static void BuildTable()
{
  // For the first phase, read all needed data into data structures in memory.
  printf("Parsing result data:\n\n");
  AquireData();

  // For the second phase, use the data in memory to compute any needed secondary stats.
  ProcessData();

  // For the third phase, build HTML data table of results.
  printf("Generating HTML output: ");
  fflush(stdout);
  WriteHTML();
  printf("Done.\n\n");
}

static void UsageError()
{
  fprintf(stderr,"usage:\n\tbuild_table <arbiter_result_file>\n");
  exit(1);
}

static void Cleanup()
{
  char buf[512];

  // Change back to the starting directory
  if( chdir("..") == -1 ) {
    fprintf(stderr,"Could not change to starting directory.\n");
    exit(1);
  }

  // Erase tmp directory
  printf("\nRemoving temporary files / cleanup: ");
  fflush(stdout);
  sprintf(buf,"rm -rf %s",Results.jobname);
  system(buf);
  sprintf(buf,"mkdir %s",Results.jobname);
  system(buf);
  sprintf(buf,"mv *.png *.html *.data %s",Results.jobname);
  system(buf);
  printf("Done.\n");
}

int main(int argc, char **argv)
{

  // Check command line args
  if( argc < 2 )
    UsageError();

  // "mount" the resultball
  MountResultball(argv[1]);

  // Go through each set to average/sum/analize it's properties,
  // collect image dumps, and write this to an HTML data table.
  //
  BuildTable();

  // Erase temp files, etc.
  Cleanup();

  return 0;
}
