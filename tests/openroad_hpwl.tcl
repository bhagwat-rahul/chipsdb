# Usage (OpenROAD does not accept extra args after cmd_file):
#   TECH_LEF=... LEF=... DEF=... ITERS=... openroad -exit tests/openroad_hpwl.tcl
#
# Prints:
#   openroad_hpwl total=<int> iters=<int> us_total=<int> ns_per_iter=<int> nets=<int> method=<str>

proc getenv_or {key def} {
  if {[info exists ::env($key)]} { return $::env($key) }
  return $def
}

proc hascmd {name} { expr {[llength [info commands $name]] != 0} }

set tech_lef_path [getenv_or TECH_LEF "data/openroad_tech.lef"]
set lef_path      [getenv_or LEF      "build/bench/bench.lef"]
set def_path      [getenv_or DEF      "build/bench/bench.def"]
set iters_str     [getenv_or ITERS    "1000"]
set iters         [expr {int($iters_str)}]
if {$iters < 1} { set iters 1 }

if {[file exists $tech_lef_path]} { read_lef $tech_lef_path }
read_lef $lef_path
read_def $def_path

proc compute_hpwl_total {} {
  # Prefer OpenDB traversal (no routing/tracks required).
  set block ""
  if {[hascmd ord::get_db_block]} { set block [ord::get_db_block] }
  if {$block eq ""} { return -1 }

  set nets ""
  if {[hascmd odb::dbBlock_getNets]} { set nets [odb::dbBlock_getNets $block] }
  if {$nets eq ""} { catch { set nets [$block getNets] } }
  if {$nets eq ""} { return -2 }

  set total 0
  foreach net $nets {
    set iterms ""
    if {[hascmd odb::dbNet_getITerms]} { set iterms [odb::dbNet_getITerms $net] }
    if {$iterms eq ""} { catch { set iterms [$net getITerms] } }
    if {$iterms eq ""} { continue }

    set first 1
    set minx 0; set maxx 0
    set miny 0; set maxy 0

    foreach it $iterms {
      set x ""; set y ""; set xy ""

      if {[hascmd odb::dbITerm_getAvgXY]} {
        catch { set xy [odb::dbITerm_getAvgXY $it] }
      } else {
        catch { set xy [$it getAvgXY] }
      }

      if {$xy ne ""} {
        catch { lassign $xy x y }
      } else {
        set bb ""
        if {[hascmd odb::dbITerm_getBBox]} {
          catch { set bb [odb::dbITerm_getBBox $it] }
        } else {
          catch { set bb [$it getBBox] }
        }
        if {$bb ne "" && [llength $bb] == 4} {
          lassign $bb x1 y1 x2 y2
          set x [expr {($x1 + $x2) / 2}]
          set y [expr {($y1 + $y2) / 2}]
        }
      }

      if {$x eq "" || $y eq ""} { continue }

      if {$first} {
        set minx $x; set maxx $x
        set miny $y; set maxy $y
        set first 0
      } else {
        if {$x < $minx} { set minx $x }
        if {$x > $maxx} { set maxx $x }
        if {$y < $miny} { set miny $y }
        if {$y > $maxy} { set maxy $y }
      }
    }

    if {!$first} {
      set total [expr {$total + (($maxx - $minx) + ($maxy - $miny))}]
    }
  }

  return $total
}

set total [compute_hpwl_total]
if {$total < 0} {
  if {$total == -1} { puts "openroad_hpwl error=no_db_block" }
  if {$total == -2} { puts "openroad_hpwl error=no_nets" }
  exit 2
}

# Warmup (ignore timing).
set _warm [compute_hpwl_total]

set t0 [clock microseconds]
for {set i 0} {$i < $iters} {incr i} {
  set total [compute_hpwl_total]
}
set t1 [clock microseconds]

set us_total [expr {$t1 - $t0}]
set ns_per_iter [expr {int(($us_total * 1000.0) / $iters)}]

set block [ord::get_db_block]
set nets [odb::dbBlock_getNets $block]
set net_count [llength $nets]

puts "openroad_hpwl total=$total iters=$iters us_total=$us_total ns_per_iter=$ns_per_iter nets=$net_count method=odb_tcl"
exit

