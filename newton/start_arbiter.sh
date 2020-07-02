#!/bin/bash

#
# Parse command line args
#
if [ -z "$1" ] ; then
    echo
    echo "usage: start_arbiter.sh <number of nodes>" 
    echo
    exit
fi
nodes="$1"

#
# Start arbiters
#
for((i=0; i<$nodes; i++)) ; do
    # Create a job script for the new run
    echo "#!/bin/bash"                                                >  arbiter.sh
    echo "#BSUB -q medium"                                            >> arbiter.sh
    echo                                                              >> arbiter.sh
    echo "cd /home/eduenezg/arbiter/"                                 >> arbiter.sh
    echo "mkdir -p /state/partition1/data$i"                          >> arbiter.sh
    echo "./arbiter 1337:lubo.bio.utk.edu  /state/partition1/data$i"  >> arbiter.sh    
    echo "rm -rf /state/partition1/data$i"                            >> arbiter.sh    

    # Start the job
    bsub < arbiter.sh

    # Cleanup
    rm arbiter.sh
done
