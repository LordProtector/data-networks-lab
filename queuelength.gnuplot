set xlabel "time"
set ylabel "length"
set title "Queuelength"
set output './tmp/queuelength.png'
set terminal png
plot './tmp/queuelength' using 1:4 with lines title 'link1', '' using 1:5 with lines title 'link2'
reset

