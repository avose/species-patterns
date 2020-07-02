#!/bin/sh

  #cleanup all dangling runs
  echo "Cleaning up dangling runs"
  pkill 'doall'
  n=`qstat | wc -l `
  let "n -=2 ";
  for i in `qstat | tail -n $n | awk '{print $1}'`; do
    qdel $i;
  done
  rm ~/*.o[0-9]* ~/*.e[0-9]*

