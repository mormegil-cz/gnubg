set xrange [0:1]
set xlabel "Wins"
set ylabel "Equity"

livecube1(x)=-1.196 + x * (-1+1.196)/0.23794
livecube2(x)=-1 + 2*(x-0.23794)/(0.75977-0.23794)
livecube3(x)= 1 + (x-0.75077) * (1.229-1)/(1-0.75077)


cubeless(x)=-1.196+x*(1.229+1.196)
livecube(x)=( x < 0.23794 ? livecube1(x) : \
            ( x < 0.75077 ? livecube2(x) : livecube3(x) ) )
cubeful(x)=0.32*cubeless(x)+0.68*livecube(x)

set terminal dumb
set output "mgtp.txt"
plot cubeless(x) title "cubeless", \
     livecube(x) title "live cube", \
     cubeful(x)  title "cubeful"

set terminal postscript eps enhanced color dashed
set output "mgtp.eps"
plot cubeless(x) title "cubeless", \
     livecube(x) title "live cube", \
     cubeful(x)  title "cubeful"
