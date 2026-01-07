# Usage (OpenROAD does not accept extra args after cmd_file):
#   TECH_LEF=... LEF=... DEF=... openroad -exit tests/openroad_hpwl.tcl
#
# Prints:
#   openroad_hpwl total=<int>

proc getenv_or {key def} {
  if {[info exists ::env($key)]} {
    return $::env($key)
  }
  return $def
}

set tech_lef_path [getenv_or TECH_LEF "data/openroad_tech.lef"]
set lef_path      [getenv_or LEF      "build/bench/bench.lef"]
set def_path      [getenv_or DEF      "build/bench/bench.def"]

if {[file exists $tech_lef_path]} {
  read_lef $tech_lef_path
}
read_lef $lef_path
read_def $def_path

set out ""
if {[llength [info commands report_hpwl]]} {
  set out [report_hpwl]
} else {
  # Fallback: compute HPWL directly from OpenDB objects (no routing/tracks needed).
  proc hascmd {name} { expr {[llength [info commands $name]] != 0} }

  set block ""
  if {[hascmd ord::get_db_block]} {
    set block [ord::get_db_block]
  }
  if {$block eq ""} {
    puts "openroad_hpwl error=no_db_block"
    exit 2
  }

  set nets ""
  if {[hascmd odb::dbBlock_getNets]} {
    set nets [odb::dbBlock_getNets $block]
  } else {
    catch { set nets [$block getNets] }
  }
  if {$nets eq ""} {
    puts "openroad_hpwl error=no_nets"
    exit 2
  }

  set total 0
  foreach net $nets {
    set iterms ""
    if {[hascmd odb::dbNet_getITerms]} {
      set iterms [odb::dbNet_getITerms $net]
    } else {
      catch { set iterms [$net getITerms] }
    }
    if {$iterms eq ""} {
      continue
    }

    set first 1
    set minx 0
    set maxx 0
    set miny 0
    set maxy 0

    foreach it $iterms {
      set x ""
      set y ""
      set xy ""

      if {[hascmd odb::dbITerm_getAvgXY]} {
        catch { set xy [odb::dbITerm_getAvgXY $it] }
      } else {
        catch { set xy [$it getAvgXY] }
      }

      if {$xy ne ""} {
        catch { lassign $xy x y }
      } else {
        # Try bbox-based fallback.
        set bb ""
        if {[hascmd odb::dbITerm_getBBox]} {
          catch { set bb [odb::dbITerm_getBBox $it] }
        } else {
          catch { set bb [$it getBBox] }
        }
        if {$bb ne ""} {
          # If bbox is a 4-tuple, use its center.
          if {[llength $bb] == 4} {
            lassign $bb x1 y1 x2 y2
            set x [expr {($x1 + $x2) / 2}]
            set y [expr {($y1 + $y2) / 2}]
          }
        }
      }

      if {$x eq "" || $y eq ""} {
        continue
      }

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
      set hpwl [expr {($maxx - $minx) + ($maxy - $miny)}]
      set total [expr {$total + $hpwl}]
    }
  }

  puts "openroad_hpwl total=$total"
  exit
}

# Try to extract the last integer from the output as total HPWL.
set nums [regexp -all -inline {[0-9]+} $out]
set total [lindex $nums end]
puts "openroad_hpwl total=$total"

exit
