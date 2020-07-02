#!/bin/bash

#
# Params and values
#

#define _H                    0.1
#define _DRANGE	              3
#define _BETA                 0.20


H="0.1 0.9"
# be careful with the digit difference in DRA=27 compile separetely
DRA="3 9"
BET="0.20 0.40 0.60"

#
# Setup .par files
#
i="0"
    for bs in $H ; do 
       for dr in $DRA ; do 
          for B in $BET ; do
              # Copy base params
	      file="H-$bs-DR-$dr-B-$B.par"
	      cp parameters.base $file
		
              # Fill in set params
	      echo                               >> $file
	      echo "#define _H           $bs"    >> $file
	      echo "#define _DRANGE      $dr"    >> $file
	      echo "#define _BETA        $B"     >> $file
	      echo "#endif"                      >> $file
		
              # Inc to next set
	      i=$(($i+1))
           done
       done
    done