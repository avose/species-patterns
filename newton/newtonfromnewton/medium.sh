#!/bin/bash

#
# Parse command line args
#
if [ -z "$1" ] ; then
    echo "Usage: `basename $0` <set_directory> [generations]"
    exit 1
fi

gens=$2
if [ -z "$2" ] ; then
    gens=200000;
fi

sets="$1"

# Create a job script for the new run
(
  echo "#!/bin/bash"
  echo "#$ -q medium*"
  echo "#$ -N ${sets:0:8}"
  echo ""
  echo "cd /data/scratch/abirand/$sets"
  echo "./simulation $gens > $gens.out"
) > run.sh

# Start the job
qsub run.sh

# Cleanup
rm run.sh

exit

