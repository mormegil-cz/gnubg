set xrange [0:1]
set xlabel "Wins"
set ylabel "Cubeful equity (fully live cube)"

set terminal dumb
set output "mgtp.txt"
plot "mgtp.data" notitle with lines

set terminal postscript eps enhanced color dashed
set output "mgtp.eps"
plot "mgtp.data" notitle with lines
