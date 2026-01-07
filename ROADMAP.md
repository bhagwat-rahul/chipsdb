# ROADMAP

This is the long-term roadmap for building `chipsdb` into a fast,
predictable, RL-operable replacement for the OpenROAD/OpenDB-style stack.

The guiding idea:

- Keep **design state** in a few tight, data-oriented in-memory databases.
- Make **actions** cheap (apply a batch of edits).
- Make **observations + rewards** cheap (incremental cache updates).
- Run **full expensive checks** at controlled points (end of episode, eval
  mode, regression tests).

## Principles

- **C11, arenas, SoA, stable IDs**: no hidden allocation, pointer chasing, or
  per-object ownership in hot loops.
- **Two-tier evaluation**:
  - **Fast surrogates** for RL inner loops (HPWL, congestion proxies, simple
    legality, approximate timing).
  - **Exact tools** (STA/DRC) used sparingly, and with clear boundaries.
- **Determinism**: same inputs → same outputs, to keep RL training stable.

## Phase 0: Foundation (complete)

- [x] Arena allocator + frame markers.
- [x] Core type system: IDs, result codes.
- [x] Placed-netlist core DB:
  - [x] Instances, pins, nets (SoA).
  - [x] Net adjacency.
  - [x] Derived caches (bbox, HPWL).
  - [x] Batch edit entry points for RL steps.

Exit criteria met (current code):

- Arena allocator with frame markers.
- Stable 32-bit IDs + explicit `DbResult` error codes.
- Placed DB with SoA tables and explicit reserve/build APIs.
- Net adjacency + derived net caches (bbox/HPWL).
- Batch edits with incremental cache updates, including instance moves and in-batch net pinlist redefinition.
- Cheap invariant checking via `placed_db_validate()` for early bug-catching.

## Phase 1: Tech + library ingestion (enables real geometry)

Goal: stop “making up” pin offsets and dimensions; use real library data.

- [ ] LEF/tech LEF parsing:
  - [ ] Sites/rows, via rules.
  - [x] Routing layers, tracks (minimal: direction/pitch/width/spacing + TRACKS).
  - [x] Cell abstracts (partial: size, pin direction, pin bbox center from RECTs).
- [ ] Liberty parsing (minimal subset):
  - [ ] Pin direction, function (optional early), timing arcs (later).

Outputs:

- [x] `TechDb`: layers/tracks (minimal).
- [x] `LibDb`: cell masters + pin direction + pin centers (partial geometry).

## Phase 2: Floorplan DB (makes placement physically meaningful)

Goal: represent the physical canvas and basic placement constraints.

- [ ] Die/core, sites, rows.
- [ ] Blockages, regions, keepouts.
- [ ] Macro definitions and macro placement constraints.

Key APIs:

- [ ] Fast “is legal” checks for RL moves.
- [ ] Fast snapping to sites/rows.

## Phase 3: Placement engine surfaces (RL + classic algorithms)

Goal: make placement a first-class RL environment.

- [ ] Actions:
  - [ ] Move instance, swap instances, nudge groups.
  - [ ] Macro moves with constraints.
- [ ] Observations:
  - [ ] Neighborhood graph features (net degrees, local density).
  - [ ] Spatial bin features (utilization, macro proximity, proxy congestion).
- [ ] Rewards (tiered):
  - [ ] Inner loop: HPWL + density + simple legality penalties.
  - [ ] Periodic: congestion proxy, coarse timing proxy.
  - [ ] Eval: full legality + timing + route metrics.

Classic passes (callable in between RL steps):

- [ ] Global placement.
- [ ] Legalization.
- [ ] Detailed placement.

## Phase 4: Routing DB (from connectivity to wires)

Goal: represent route resources and wires in a way that supports both routing
algorithms and RL interventions.

- [ ] Track grid representation (per layer).
- [ ] Obstacles from placed instances + blockages.
- [ ] Net routing guides + wire segments + vias.

Derived caches:

- [ ] Congestion per bin/layer.
- [ ] Estimated wirelength, via counts.

Algorithms:

- [ ] Global routing.
- [ ] Detailed routing (initially minimal; evolve over time).

RL hooks:

- [ ] Guide editing, rip-up and reroute decisions, congestion-aware actions.

## Phase 5: CTS + timing-lite DB

Goal: expose clock structure and timing signals to guide RL or heuristics.

- [ ] Clock nets, sinks, buffer/inverter insertion primitives.
- [ ] Timing-lite propagation:
  - [ ] Use wirelength-based delay proxies first.
  - [ ] Later integrate extracted RC models and liberty arcs.

RL hooks:

- [ ] Buffer placement, skew/cost trade-offs, congestion-aware CTS decisions.

## Phase 6: Extraction + STA integration

Goal: bridge from fast proxies to accurate signoff metrics.

- [ ] RC extraction (incremental where possible).
- [ ] STA:
  - [ ] Start with a minimal internal engine or a clean integration boundary.
  - [ ] Use it selectively (episode end / evaluation runs).

## Phase 7: DRC/LVS boundaries + GDSII output

Goal: produce manufacturable output.

- [ ] Geometry DB completeness:
  - [ ] Cell placements → shapes.
  - [ ] Routed wires/vias → shapes.
  - [ ] Fill/metal density (later).
- [ ] GDSII writer.
- [ ] DRC/LVS integration boundary:
  - [ ] Internal minimal checks early (spacing/width on-route).
  - [ ] External signoff tools for validation in CI/eval (optional).

## Phase 8: Full Verilog → GDSII flow orchestration

Goal: a single reproducible pipeline with RL control points.

- [ ] Import:
  - [ ] Verilog → synth → mapped netlist.
  - [ ] LEF/DEF (optional interoperability).
- [ ] Build DBs:
  - [ ] `TechDb` + `LibDb` + `LogicalDb` + `FloorplanDb`.
- [ ] Place:
  - [ ] RL actions + classical stabilization passes.
- [ ] CTS.
- [ ] Route.
- [ ] Extract + STA.
- [ ] Export GDSII.

Deliverables:

- [ ] A CLI pipeline (deterministic).
- [ ] A “gym-like” RL interface:
  - [ ] `reset(seed)`
  - [ ] `step(action_batch)`
  - [ ] `observe()`
  - [ ] `reward()`
  - [ ] `done()`

## Ongoing: Performance and correctness guardrails

- [ ] Microbenchmarks for hot queries and batch updates.
- [ ] Corpus of designs for regression (small → large).
- [ ] Deterministic replay logs for RL episodes.
- [ ] Profiling-driven layout changes (SoA/packing/binning) when needed.
