#!/bin/sh

#
# start.sh
#
# @autors Stefan Tombers, Alexander Bunte, Jonas Bürse
#
# Short script to start cnet with a topology file and an execution period.
#

rm *.o
#$1 = period of execution
#$2 = topology file

time cnet -W -s -T -e $2 $1 -f 10s
