import statistics
import sys

latency, throughput = [], []
for line in open(sys.argv[1]):
    count, value = line.split()
    if "." in value:
        throughput.append(float(value))
    else:
        latency.append((int(count), int(value)))

if not throughput:
    print(10000000, 0)
    raise SystemExit

samples = throughput[1:] if len(throughput) > 1 else throughput
tp = int(statistics.mean(samples))

total = sum(c for c, _ in latency)
cum = 0
p99 = latency[-1][1]
for count, value in latency:
    cum += count
    if cum / total >= 0.99:
        p99 = value
        break

print(p99, tp)
