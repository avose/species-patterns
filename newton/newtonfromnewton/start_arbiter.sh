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
    echo "#$ -q medium*"                                              >> arbiter.sh
    echo                                                              >> arbiter.sh
    echo "cd /home/abirand/arbiter/"                                  >> arbiter.sh
    echo "mkdir -p /data/scratch/abirand/data$i"                      >> arbiter.sh
    echo "./arbiter 1337:lubo.bio.utk.edu  /data/scratch/abirand/data$i"  >> arbiter.sh    
    echo "rm -rf /data/scratch/abirand/data$i"                        >> arbiter.sh    

    # Start the job
    qsub < arbiter.sh

    # Cleanup
    rm arbiter.sh
done

