set xrange [0:1]
set xlabel "Wins"
set ylabel "Equity"

livecube1(x)=-1.196 + x * (-1+1.196)/0.23794
livecube2(x)=-1 + 2*(x-0.23794)/(0.75977-0.23794)
livecube3(x)= 1 + (x-0.75077) * (1.229-1)/(1-0.75077)

livecube4(x)=-1.196 + x * (-1+1.196)/0.23794
livecube5(x)=-1 + (x-0.23794)*(1.229+1)/(1.0-0.23794)

livecube6(x)=-1.196 + (x-0.00000)*(1+1.196)/(0.75077-0.00000)
livecube7(x)= 1     + (x-0.75077)*(1.229-1)/(1.00000-0.75077)

cubeless(x)=-1.196+x*(1.229+1.196)
livecube(x)=( x < 0.23794 ? livecube1(x) : \
            ( x < 0.75077 ? livecube2(x) : livecube3(x) ) )
cubeful(x)=0.32*cubeless(x)+0.68*livecube(x)

livecube8(x)=( x < 0.23794 ? livecube4(x) : livecube5(x))
cubeful2(x)=2*(0.32*cubeless(x)+0.68*livecube8(x))

livecube9(x)=( x < 0.75077 ? livecube6(x) : livecube7(x))
cubeful3(x)=2*(0.32*cubeless(x)+0.68*livecube9(x))

set terminal dumb
set output "mgcd.txt"
plot cubeful(x)  title "centered cube", \
     cubeful2(x) title "O owns 2-cube", \
     cubeful3(x) title "X owns 2-cube"

set terminal postscript eps enhanced color dashed
set output "mgcd.eps"
plot cubeful(x)  title "centered cube", \
     cubeful2(x) title "O owns 2-cube", \
     cubeful3(x) title "X owns 2-cube"
