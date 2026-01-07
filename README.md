# chipsdb

## About

A database for chip design tools

Attempting to build a replacement for [OpenROAD](https://github.com/The-OpenROAD-Project/OpenROAD).

Starting off with a faster version of [OpenDB](https://github.com/The-OpenROAD-Project/OpenROAD/tree/master/src/odb).

The original [OpenROAD Paper](https://vlsicad.ucsd.edu/Publications/Conferences/370/c370.pdf) describes using reinforcement learning and ml models to explore the design space.
However, this idea was never really explored too deeply.
To enable letting an agent take actions on designs we first need to speed up the placement - clock tree synthesis - routing - STA cycle.
This is an attempt at doing that by building a cleaner more performant memory model and parallelising every tool and the way it interfaces with the in-memory design to allow faster parsing and editing.

## Comparison with OpenROAD

Below we have a benchmark of a small test case that reports a ~500x speedup in this current implementation compared to OpenROAD. You can run this test yourself by cloning this repo, and running:-

```sh
bash ./test.sh bench_compare_hpwl
```

> You will need OpenRoad installed to compare with it. You can install it by following the official documentation: [Install OpenROAD](https://openroad.readthedocs.io/en/latest/user/Build.html)

### Benchmark: HPWL (chipsdb vs OpenROAD)

|tool|mode|N(inst)|M(nets)|K(pins/net)|P(pins/inst)|ns_per_net|nets_per_s|ns_per_step|steps_per_s|notes|
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
|chipsdb|full_hpwl|20000|20000|4|4|12.96|77190274.03|||in-memory|
|chipsdb|step_hpwl|20000|20000|4|4|||1279.50|781555.30|moves=16,steps=1000|
|openroad|full_hpwl|20000|20000|4|4|7547.645|132491.658|||iters=10,nets=20000(odb_tcl)|
|compare|full_hpwl|20000|20000|4|4|||||speedup(openroad/chipsdb)=582.38x|

Caveats:

- The OpenROAD number is **HPWL computed via Tcl/OpenDB iteration** (`odb_tcl`), not necessarily the fastest internal OpenROAD HPWL routine (if one exists).
- This is **compute-only HPWL throughput** after the DB is loaded; it does not include LEF/DEF parsing cost or full flow stages.
- The testcase is synthetic/minimal (generated LEF/DEF); good for stressing inner-loop HPWL and batch updates, not representative of full OpenROAD QoR or routing/timing. However it represents the the kind of cheap proxy computation an RL loop needs, which is what we are optimising for.

## Note

> NOTE:- This ai-experiments branch is me ***vibe-coding*** a small part of the mvp before actually going and manually building this.
Doing this only to get some insight into the final design, this is a throwaway branch and none of this code will be used since we need to hand-write all of this if we want more performance than existing tools. This was just a quick way to get something running and prove the speedup thesis with a benchmark.
