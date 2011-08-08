#!/bin/bash

rm *.o
mkdir tmp
cnet -W -s -T -e $2 -f 1s $1 > tmp/out
analyze/build/analyze tmp/out tmp/results
gnuplot tmp/results.gnuplot
xv tmp/results-messages.png &
xv tmp/results-latency.png &
xv tmp/results-throughput.png &