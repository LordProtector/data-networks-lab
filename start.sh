#!/bin/sh

rm *.o
#$1 = period of execution
#$2 = topology file

cnet -W -s -T -e $2 $1
