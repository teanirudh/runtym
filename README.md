# runtym

Compares **round-robin** and **home-core** worker placement for deterministic parallel transactions.

Built on [DORADD](https://github.com/doradd-rt/doradd)’s dispatcher pipeline and [Verona-RT](https://github.com/microsoft/verona-rt). Toggle with `HC` at build time (`0` = round-robin, `1` = home-core).

## Scheduling Policy

When a procedure becomes ready, place it on a worker chosen from its **hottest** declared resource (warm/cold ignored):

1. Pick `resource_id = argmax access(resource)`
2. Map `home = resource_id mod N`
3. Enqueue on that worker’s queue

Same hot resource → same home. Distinct hot resources spread across workers.

Round-robin (`HC=0`) ignores resources and rotates cores instead.

## Experiment Workload

Tiered YCSB: each transaction samples a fixed mix of hot / warm / cold keys (`HT` / `WM` / `CD`, with `HT+WM+CD = RD+WR`). The generator puts the hottest key first so home-core can use slot 0.

## Quick Start

Ubuntu 22.04. Needs `build-essential`, `cmake`, `ninja-build`, `python3`.

```bash
git clone https://github.com/teanirudh/runtym.git
cd runtym

make gen-ycsb-tier RD=5 WR=5 HT=1 WM=2 CD=7

make build-ycsb RD=5 WR=5 HC=0
make run-ycsb NC=8 IN=exp:10 LG=rd05_wr05_ht01_wm02_cd07.txt

make build-ycsb RD=5 WR=5 HC=1
make run-ycsb NC=8 IN=exp:10 LG=rd05_wr05_ht01_wm02_cd07.txt
```

