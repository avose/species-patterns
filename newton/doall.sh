#!/bin/sh

sub=0

function unfinished () {
  #Check if there are unfinished runs
  echo "Checking for unfinished runs"
  rm tmp;
  for i in `ls -d set*/run*`; do
    if [ ! -e $i/.done ] ; then echo $i; fi
  done > tmp;
  cat tmp;
}

function cleanup () {
  #cleanup all dangling runs
  echo "Cleaning up dangling runs"
  n=`qstat | wc -l `
  let "n -=2 ";
  for i in `qstat | tail -n $n | awk '{print $1}'`; do
    qdel $i;
  done
  rm ~/*.o[0-9]* ~/*.e[0-9]*
  sleep 10s;
}

cleanup;
unfinished;

#exit

#While we are not done with all runs
while [ `cat tmp | wc -l` -ne 0 ]; do


  for d in `cat tmp`; do
    echo "Processing $d"
    #prepare to resume the run
    #cp $d/checkpoint.tar.gz $d/checkpoint 2> /dev/null
    ./simulation.sh $d;
    let "sub++";
    if [ $sub -eq 300 ] ; then
      echo "Sleeping until next set";
      sleep 90m;
      sub=0;
      cleanup;
    fi
  done

  sleep 90m;
  sub=0;
  cleanup;
  unfinished;

done

