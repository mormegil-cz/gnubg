set logscale xy
set xrange [1:1e9]
anneal(a,n,x) = a/(x/1000+1)**n
set xlabel "Positions"
set ylabel "Alpha"

set terminal dumb
set output "annealing.txt"
plot anneal(0.5,0.5,x) title "Alpha 0.5, anneal 0.5", \
     anneal(0.1,0.1,x) title "Alpha 0.1, anneal 0.1"

set terminal postscript eps enhanced color dashed
set output "annealing.eps"
set xtics ("1" 1, "" 5, "10" 10, "" 50, "100" 100, "" 500, "1000" 1000, \
           "" 5000, "10^4" 1e4, "" 5e4, "10^5" 1e5, "" 5e5, \
           "10^6" 1e6, "" 5e6, "10^7" 1e7, "" 5e7, "10^8" 1e8, "" 5e8, \
           "10^9" 1e9)
plot anneal(0.5,0.5,x) title "Alpha 0.5, anneal 0.5", \
     anneal(0.1,0.1,x) title "Alpha 0.1, anneal 0.1"
