set xrange [0:1]
set xlabel "Wins"
set ylabel "Cubeful MWC (fully live cube)"

set terminal dumb
set output "mptp.txt"
plot "mptp.data" notitle with lines

set terminal postscript eps enhanced color dashed
set output "mptp.eps"
plot "mptp.data" notitle with lines
