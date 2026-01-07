# Agent Development Guide

This project is written in pure C11 and is intended to be fast, predictable, and easy to reason about. Code should be Casey Muratori, Jonathan Blow style.
Giving examples of these game devs since we need that kind of performance and simplicity.
The primary goal is performance with clarity, not abstraction or generality.
If something is clever, generic, or “nice” but costs mental or runtime overhead, it is probably wrong.

Core Philosophy

- Prefer simple, explicit code over abstraction.
- Prefer data-oriented design over object-oriented patterns.
- Prefer compile-time decisions over runtime flexibility.
- Optimize for debuggability and predictability, not extensibility.
- This codebase follows a game-engine / handmade-style approach inspired by low-level systems programming.

Language & Tooling Rules

- C11 only
- No C++
- No STL-style containers
- No external libraries (except libc)
- No macros that hide control flow
- No build-time code generation

If it cannot be understood by reading one .c file top to bottom, it is suspect.

Memory Model

- All memory ownership is explicit
- No hidden allocations
- No implicit lifetime extension
- Allocation Strategy
- Use arenas as the primary allocator
- Prefer linear / bump allocation
- No per-object malloc/free in hot paths
- Free memory in bulk by resetting arenas

Typical pattern:

```c
Arena temp_arena = arena_begin_frame(&global_arena);
/* work */
arena_end_frame(&temp_arena);
```

Forbidden

- Reference counting
- Garbage collection
- Smart pointers
- Implicit heap allocation inside helpers
- Data Layout Rules
- Data lives in plain structs
- Prefer SoA over AoS when iterating
- Arrays are contiguous
- Indices are preferred over pointers for stable references

Example:

```c
typedef struct {
    uint32_t *node_ids;
    uint32_t *fanin_offsets;
    uint32_t *fanin_counts;
    uint32_t count;
} Netlist;
```

Performance Invariants

Assume:

- This code runs on large designs
- Hot paths execute millions of times
- Cache behavior matters more than abstraction

Therefore:

- Avoid virtual dispatch
- Avoid function pointers in hot loops
- Avoid unnecessary indirection
- Avoid unpredictable branches
- Measure first, but write as if performance matters everywhere.
- Error Handling
- No exceptions
- No hidden control flow
- Errors are values

Preferred:

```c
typedef enum {
    DB_OK,
    DB_ERR_OOM,
    DB_ERR_INVALID_INPUT,
} DbResult;
```

Crashing is acceptable if invariants are violated.

API Design Rules:-

- APIs expose data, not behavior
- Callers control allocation
- Functions do one thing

Bad:

```c
db_add_cell(db, name);
```

Good:

```c
Cell *cell = db_reserve_cell(db);
cell->name = name;
```

Code Style

- Flat files over deep hierarchies
- One concept per file
- Clear naming over short naming
- Comments explain why, not what

Avoid “framework” thinking. This is a program, not a platform.

What This Project Is NOT:-

- Not a reusable database library
- Not a generic ECS
- Not a plugin system
- Not a scripting host
- Not future-proof by design

It is allowed to be opinionated, rigid, and specialized.

If a change makes the code:

- harder to step through in a debugger
- harder to reason about memory lifetime
- harder to predict performance
- Don’t make the change.

> **Diff Directive:** Whenever you make a change (diff), keep it small and targeted. Only make a single, incremental modification per diff, so that every line can be carefully reviewed and audited; avoid bundling multiple unrelated changes in one go.
