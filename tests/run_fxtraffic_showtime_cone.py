"""FX-TRAFFIC (crash parity 2026-09-23, G58-X1): the Showtime sympathetic-crash cone's recip-Y
scale (TrafficEntityModule +0x726F0, lane z).

  Construct @0x82740220: r11 = this+0x726F0 at 0x827405D4 and untouched to 0x82740680; lane 2 is
  flt_820BA544 (0x3E800000 == 0.25f) -- lfs 0x82740658, stfs 0x82740670, stvx128 0x82740680.
  The PC seeded 0.0f, so in Showtime every crashing thing's height offset was zeroed before the
  cone test (IsPointWithinSquishedCone 0x827147A0 multiplies diff.y by lane z) and airborne /
  elevated crashers started sympathetic crashes the console rejects.

Numeric: tests/FxTrafficShowtimeCone.cpp applies the PRODUCTION seed statement (extracted from
Construct) through the production SetTuningLanes and tests points with the production
header-inline IsPointWithinSquishedCone.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic_showtime_cone.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
NUMERIC_CHECKS = 7


def numeric(tree):
    source = tree.read(MODULE_CPP)
    try:
        helper = definition(source, "inline void SetTuningLanes(Vector4& lrOut")
    except ValueError:
        print("NUMERIC: cannot build -- SetTuningLanes absent")
        return None
    seed = re.search(r"SetTuningLanes\(kfParamSympatheticConeShowTime_CosAngle_Length_RecipYScale_W,[^;]*;", source)
    if seed is None:
        print("NUMERIC: cannot build -- the Showtime cone seed statement is absent")
        return None
    body = seed[0].replace("kfParamSympatheticConeShowTime_CosAngle_Length_RecipYScale_W", "lrCone", 1)
    inc = helper + "\ninline void SeedShowtimeCone(Vector4& lrCone)\n{\n    " + body + "\n}\n"
    return compile_and_run(Path(__file__).with_name("FxTrafficShowtimeCone.cpp"), "showtime_cone.inc", inc,
                           "FxTrafficShowtimeCone")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxtraffic_showtime_cone", [], numeric(Tree(args.rev)), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
