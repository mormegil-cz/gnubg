set xrange [0:1]
set xlabel "Wins"
set ylabel "MWC"

livecube1(x)=0.22+(x-0.000)*(0.24-0.22)/(0.245-0.000)
livecube2(x)=0.24+(x-0.245)*(0.36-0.24)/(0.727-0.245)
livecube3(x)=0.36+(x-0.727)*(0.41-0.36)/(1.000-0.727)

cubeless(x)=0.22+x*(0.41-0.22)
livecube(x)=( x < 0.245 ? livecube1(x) : \
            ( x < 0.727 ? livecube2(x) : livecube3(x) ) )
cubeful(x)=0.32*cubeless(x)+0.68*livecube(x)

set terminal dumb
set output "mptp.txt"
plot cubeless(x) title "cubeless", \
     livecube(x) title "live cube", \
     cubeful(x)  title "cubeful"

set terminal postscript eps enhanced color dashed
set output "mptp.eps"
plot cubeless(x) title "cubeless", \
     livecube(x) title "live cube", \
     cubeful(x)  title "cubeful"
