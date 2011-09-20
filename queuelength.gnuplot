set xlabel "time"
set ylabel "length"
set title "Messages"
set output './tmp/queuelength-HOM.png'
set terminal png
plot 'out-HOM' using 1:2 with linespoints title 'link0', 'out-HOM' using 1:3 with linespoints title 'link1'
reset

set xlabel "time"
set ylabel "length"
set title "Messages"
set output './tmp/queuelength-SB.png'
set terminal png
plot 'out-SB' using 1:2 with linespoints title 'link0', 'out-SB' using 1:3 with linespoints title 'link1', 'out-SB' using 1:4 with linespoints title 'link2'
reset

set xlabel "time"
set ylabel "length"
set title "Messages"
set output './tmp/queuelength-SLS.png'
set terminal png
plot 'out-SLS' using 1:2 with linespoints title 'link0', 'out-SLS' using 1:3 with linespoints title 'link1'
reset
