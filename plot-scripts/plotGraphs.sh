#! /bin/bash
for f in *.txt ; do
    grep "UDP-TIMING" $f > $f.udp
    beginning=($(grep -n ": 0 0" $f.udp | cut -f1 -d:))
    echo "Begins at ${beginning[-1]}" # pick the last one
    tail -n +${beginning[-1]} $f.udp | cut -d " " -f 2,4 > $f.latencies
    set -v 
    set -x
    wc -l $f.latencies
    set +v 
    set +x
    python plotLatencies.py $f.latencies > $f.plot
    python latencyHist.py $f.latencies > $f.hist
    cp $f.plot plot.data
    cp $f.hist hist.data
    gnuplot -p plotHist
    gnuplot -p plotGraph
    cp plot.png $f-plot.png
    cp hist.png $f-hist.png
done
rm plot.png hist.png
