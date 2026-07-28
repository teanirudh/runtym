set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p results app/build/results

DIST=(uniform zipfian)
CONT=(no high)
INVL=(10000 8000 6000 4000 2000 1000 800 600 400 200 100 50)

for dist in "${DIST[@]}"; do
  for cont in "${CONT[@]}"; do
    out=results/${dist}_${cont}_cont.res
    rm -f "$out"
    for i in "${INVL[@]}"; do
      make run MODE=release BENCH=ycsb CORES=8 INTERVAL=exp:$i \
        SOURCE="../ycsb/gen-log/ycsb_${dist}_${cont}_cont.txt"
      sort -n app/build/results/exp:$i-latency.log | uniq -c > app/build/results/exp:$i.txt
      python3 scripts/calc.py app/build/results/exp:$i.txt >> "$out"
    done
  done
done
