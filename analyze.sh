#!/bin/bash

#
# analyze.sz
#
# @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
#
# Short script to start cnet with a topology file and an execution period.
# Additionally it creates statistics for message, latency and throughput.
#

rm *.o
mkdir tmp

#$1 = period of execution
#$2 = topology file

cnet -W -s -T -e $2 -f 1s $1 > tmp/out
analyze/build/analyze tmp/out tmp/results
gnuplot tmp/results.gnuplot
xv tmp/results-messages.png &
xv tmp/results-latency.png &
xv tmp/results-throughput.png &
