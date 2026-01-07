# chipsdb

A database for chip design tools

Attempting to build a replacement for [OpenROAD](https://github.com/The-OpenROAD-Project/OpenROAD).

Starting off with a faster version of [OpenDB](https://github.com/The-OpenROAD-Project/OpenROAD/tree/master/src/odb).

The original [OpenROAD Paper](https://vlsicad.ucsd.edu/Publications/Conferences/370/c370.pdf) describes using reinforcement learning and ml models to explore the design space.
However, this idea was never really explored too deeply.
To enable letting an agent take actions on designs we first need to speed up the placement - clock tree synthesis - routing - STA cycle.
This is an attempt at doing that by building a cleaner more performant memory model and parallelising every tool and the way it interfaces with the in-memory design to allow faster parsing and editing.

> NOTE:- This ai-experiments branch is me ***vibe-coding*** a small part of the mvp before actually going and manually building this.
Doing this only to get some insight into the final design, this is a throwaway branch and none of this code will be used since we need to hand-write all of this if we want more performance than existing tools.
