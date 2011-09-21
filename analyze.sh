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
if [ ! -e tmp ]; then
	mkdir tmp
fi

#$1 = period of execution
#$2 = topology file

cnet -W -s -T -e $2 -f 1s $1 -o out-%n > tmp/out
analyze/build/analyze tmp/out tmp/results
gnuplot tmp/results.gnuplot
xv tmp/results-messages.png &
xv tmp/results-latency.png &
xv tmp/results-throughput.png &

grep -e queue out-SB > tmp/queuelength
gnuplot queuelength.gnuplot
xv tmp/queuelength.png &

grep timeout out-SB | grep "to_node: 96" > tmp/timeout
gnuplot timeout.gnuplot
xv tmp/timeout.png&

grep window out-SB | grep "to_node: 96" > tmp/window
gnuplot window.gnuplot
xv tmp/window.png&
