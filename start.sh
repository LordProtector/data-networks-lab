#!/bin/sh

rm *.o
cnet -W -s -T -e $2 $1
