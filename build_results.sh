#!/bin/bash

# This script is designed to take a directory of results from kraken and turn them into
# a "resultball" of the form that would have been returned by the arbiter. This should
# allow the results that come from kraken to analized using all the regular arbiter scripts.

# The resultball will be built and placed here:

resultdir=//data/scratch/abirand/
jobname=aysegul-short
builddir=$resultdir/$jobname

mkdir $builddir

# 1)
# Look at all the set directories that came back from kraken and use them to create (touch)
# "setballs" for use in the "resultball".
#
# I don't think the arbiter build_table program looks at the contents of these "setballs" in
# any way; I think it just looks to see if they are there and to see what the file names are.
# I'm fairly certain it just reads the parameter space from the file names of the "setballs" 

for ksetdir in set-* ; do
    touch $builddir/`echo -n $ksetdir | tail -c 31`.par.tar.gz
done

# 2)
# Now that some fake setballs representing the desired parameter space have been created,
# individual tarballs need to be built from each created run and placed in the builddir.
# That will be done now. 

for set in set-* ; do
    cd $set
    for run in run[0123456789] ; do
        cd $run
        tarball=$builddir/${jobname}_`echo -n $set | tail -c 31`.par_`echo -n
$run | tail -c 1`.tar.gz
        tar -zcvf $tarball *
        cd ..
    done
    cd ..
done



# what does this do?
#
# it:
#
# 1) craetes files with the right name for set parameters,
# 2) creates the run tarballs
#
# what you need to do (besides fix all the code here)
# is:
#
# Tar all the set tar.gz and run tar.gz files into one tarball with no directory substructure