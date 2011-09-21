#set xlabel "time"
#set ylabel "length"
#set title "Messages"
set output './tmp/timeout.png'

set terminal png
plot 'tmp/timeout' using 1:12 with lines title 'timeout', '' using 1:8 with lines title 'estimatedRTT', '' using 1:10 with lines title 'deviation'
reset

