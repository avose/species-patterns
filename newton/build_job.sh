#!/bin/bash

#
# Check command line args
#
if [ "$1" == "" ] ; then
    echo "usage:"
    echo "       build_job.sh <JOB_ID> [<replicates>]"
    exit 1
fi
JOB="$1"

N=10;
if [ $# -eq 2 ]; then
  N=$2;
fi

rm -R set* $JOB.tar.gz

#
# Build setballs
#
for pars in *.par ; do
    mkdir          set$pars
    cp ../*.c      set$pars
    cp ../*.h      set$pars
    cp -f $pars    set$pars/parameters.h
    cp ../makefile set$pars

    echo "Processing set$pars"
    cd set$pars
    make
    rm *.h *.c makefile *.o view niche get_CPUS
    cd ..

    for(( i = 0; i < N; i++ )); do
      mkdir set$pars/run_$i;
      cp set$pars/simulation set$pars/run_$i
    done
done

#
# Build joball
#
tar -zcvf $JOB.tar.gz set*

