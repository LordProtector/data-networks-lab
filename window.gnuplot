set xlabel "time"
set ylabel "length"
set title "Window"
set output './tmp/window.png'
set terminal png
plot './tmp/window' using 1:6 with lines title 'threshold', '' using 1:8 with lines title 'windowsize', '' using 1:10 with lines title 'numOutSeg'
reset

