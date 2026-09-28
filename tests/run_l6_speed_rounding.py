"""L6 AIDRIVE (owner list 2026-09-27): ROUNDING_RULE rule 3 on the AI speed path.

The console FUSES three multiply-adds the PC spelt as a product and a sum:
  * RaceBalancingRoute::GetAISectionSpeed @0x827697A0 -- the rubber band's par speed, the section's speed band
    lerped by the graph's ratio: `fsubs f13, max, min ; fmadds f1, f13, f31, f0` @0x8276980C/0x82769810.
  * AIAggression::CalcSpeedMatchSpeed @0x8278B7A8 -- the speed-match acceleration cap
    `fmadds f0, knob, 15.0 (flt_820C4238), 5.0 (flt_820C488C)` @0x8278B824.
  * AICar::CalcDesiredSpeed @0x82796078 -- the meBehaviour == 1 opponent-rank speed
    `fnmsubs f1, index, scale, base` @0x82796138 (Road Rage) / @0x82796168 (otherwise).
ROUNDING_RULE rule 3: one rounding, std::fmaf with the console's operand order (fnmsubs as the negated fma).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l6_speed_rounding.py [--rev <b5 rev>]
    (--src-dir DIR reads the three TUs from DIR/<file name> instead: the shadow gate before a copy-in)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, compile_and_run, definition, report

AI = "src/GameSource/World/AI/"
ROUTE = AI + "RaceBalancing/BrnRaceBalancingRoute.cpp"
AGGRESSION = AI + "BrnAIAggression.cpp"
AICAR = AI + "BrnAICar.cpp"
NUMERIC_CHECKS = 8


class DirTree(Tree):
    """The three TUs from a directory (the shadow copy); everything else as the working tree."""

    def __init__(self, directory):
        super().__init__(None)
        self.directory = Path(directory)

    def read(self, relative):
        local = self.directory / Path(relative).name
        if relative in (ROUTE, AGGRESSION, AICAR) and local.exists():
            return local.read_text(encoding="utf-8-sig")
        return super().read(relative)


def numeric(tree):
    route = tree.read(ROUTE).replace("\r\n", "\n")
    aggression = tree.read(AGGRESSION).replace("\r\n", "\n")
    car = tree.read(AICAR).replace("\r\n", "\n")
    try:
        locals_ = []
        for name in ("KF_DESIRED_PLAYER_DRIVEN_MUL", "KF_DESIRED_DEFAULT_MUL"):   # CalcDesiredSpeed's TU constants
            match = re.search(r"^[ \t]*const f32 " + name + r"\s*=[^;]+;", car, re.M)
            if match is None:
                raise ValueError("no " + name)
            locals_.append(match.group(0).strip())
        chunks = ["namespace BrnAI {",
                  "namespace {", "\n".join(locals_), "}",
                  definition(route, "f32 RaceBalancingRoute::GetAISectionSpeed("),
                  definition(aggression, "f32 AIAggression::CalcSpeedMatchSpeed("),
                  definition(car, "f32 AICar::CalcDesiredSpeed("),
                  "}"]
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("L6SpeedRounding.cpp"), "l6_speed_rounding.inc",
                           "\n".join(chunks) + "\n", "L6SpeedRounding")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-dir", help="read the three TUs from this directory (shadow gate)")
    args = parser.parse_args()
    tree = DirTree(args.src_dir) if args.src_dir else Tree(args.rev)
    return report("run_l6_speed_rounding", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
