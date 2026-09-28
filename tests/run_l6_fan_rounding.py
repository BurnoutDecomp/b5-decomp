"""L6 AIDRIVE (owner list 2026-09-27): ROUNDING_RULE rule 3 in the steering fan's two avoidance contributors.

The console FUSES two multiply-adds the PC spelt as a product and a sum:
  * SteeringFan::IncludeConstantBearing @0x827873A0 -- both traffic rows are lerped toward this frame's value
    with the inlined SteeringFan::Interpolate, `fsubs f0, new, old ; fmadds f0, f0, f20 (0.2), old`
    @0x82787890/0x82787894 (eFan_AvoidTraffic) and @0x827878A0/0x827878A4 (eFan_AvoidOncomingTraffic);
    it runs on every fan update (with no traffic the rows decay by it), and since the AI traffic feed is
    live the rows carry real values.
  * SteeringFan::IncludeHardNoGo @0x82779D98 -- the ExitHNG rescale into [0.5, 1],
    `fsubs f11, v, min ; fmadds f11, f11, f12, f13 (0.5)` at all 17 sites 0x8277A108 .. 0x8277A1E0.
  * SteeringFan::AccumulateWeightings @0x82779088 -- the fold of the 14 rows by kfBias, `fmadds acc, weight,
    bias, acc` at 0x827790FC .. 0x82779194, and the 0.5 lerp of the cumulative (0x82779208 .. 0x827792AC): the
    column GetBestIndex picks the driving ray from, every fan update.
  * SteeringFan::FanIntersectsEdge @0x8277A208 -- the three 2D crosses `fmuls p, a, b ; fmsubs r, c, d, p`
    (0x8277A260 denominator, 0x8277A2AC edge parameter, 0x8277A2CC ray parameter) of the road-edge test.
ROUNDING_RULE rule 3: one rounding, std::fmaf with the console's operand order.

  1. WIRING -- IncludeConstantBearing still lerps both rows through InterpolateTraffic(old, new, 0.2).
  2. NUMERIC -- tests/L6FanRounding.cpp runs the extracted InterpolateTraffic, IncludeHardNoGo and FanIntersectsEdge
     (with the HNG TU's constants) and AccumulateWeightings (with kfBias and its KF_*_MAX pool) on inputs where one
     rounding and two differ, and checks the one-rounding result bit for bit.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_l6_fan_rounding.py [--rev <b5 rev>]
    (--src-dir DIR reads the three fan TUs from DIR instead: the shadow gate before a copy-in)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, STRSTREAM_CPP, code_only, compile_and_run, definition, report

FAN = "src/GameSource/World/AI/RacingLine/"
TRAFFIC = FAN + "BrnAISteeringFan_Traffic.cpp"
HNG = FAN + "BrnAISteeringFan_HNG.cpp"
TARGET = FAN + "BrnAISteeringFan_Target.cpp"
NUMERIC_CHECKS = 26
POOL = ("KF_CENTRE_TRACKING_MAX", "KF_HARD_NO_GO_BAD_MAX", "KF_HARD_NO_GO_GOOD_MAX", "KF_TRAFFIC_MAX",
        "KF_ONCOMING_TRAFFIC_MAX", "KF_EDGE_INTERSECTION_MAX", "KF_PARALLEL_MAX", "KF_SLAM_PLAYER_MAX")


class DirTree(Tree):
    """The two fan TUs from a directory (the shadow copy); everything else as the working tree."""

    def __init__(self, directory):
        super().__init__(None)
        self.directory = Path(directory)

    def read(self, relative):
        local = self.directory / Path(relative).name
        if relative in (TRAFFIC, HNG, TARGET) and local.exists():
            return local.read_text(encoding="utf-8-sig")
        return super().read(relative)


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    traffic = tree.read(TRAFFIC).replace("\r\n", "\n")
    try:
        body = squash(definition(traffic, "void SteeringFan::IncludeConstantBearing("))
    except ValueError:
        body = ""
    yield ("IncludeConstantBearing lerps eFan_AvoidTraffic through InterpolateTraffic(old, new, 0.2) "
           "(0x82787890/0x82787894)",
           "*lfAvoidTraffic=InterpolateTraffic(lfOldTraffic,*lfAvoidTraffic,KF_TRAFFIC_BIAS_LERP);" in body)
    yield ("IncludeConstantBearing lerps eFan_AvoidOncomingTraffic through InterpolateTraffic(old, new, 0.2) "
           "(0x827878A0/0x827878A4)",
           "*lfAvoidOncomingTraffic=InterpolateTraffic(lfOldOncoming,*lfAvoidOncomingTraffic,"
           "KF_TRAFFIC_BIAS_LERP);" in body)


def numeric(tree):
    traffic = tree.read(TRAFFIC).replace("\r\n", "\n")
    hng = tree.read(HNG).replace("\r\n", "\n")
    target = tree.read(TARGET).replace("\r\n", "\n")
    try:
        pool = []
        for name in POOL:
            match = re.search(r"^const f32 " + name + r"\s*=[^;]+;", target, re.M)
            if match is None:
                raise ValueError("no " + name)
            pool.append(match.group(0))
        chunks = ["namespace BrnAI {",
                  definition(hng, "namespace\n{"),
                  "namespace {",
                  definition(traffic, "f32 InterpolateTraffic("),
                  "}",
                  "\n".join(pool),
                  definition(target, "f32 kfBias[E_BIAS_MODE_COUNT][E_FAN_CONTRIBUTORS_COUNT] =") + ";",
                  definition(target, "SteeringFan* SteeringFan::AccumulateWeightings()"),
                  definition(hng, "void SteeringFan::IncludeHardNoGo("),
                  definition(hng, "f32 SteeringFan::FanIntersectsEdge("),
                  "}"]
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("L6FanRounding.cpp"), "l6_fan_rounding.inc",
                           "\n".join(chunks) + "\n", "L6FanRounding", extra_sources=(STRSTREAM_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-dir", help="read the three fan TUs from this directory (shadow gate)")
    args = parser.parse_args()
    tree = DirTree(args.src_dir) if args.src_dir else Tree(args.rev)
    return report("run_l6_fan_rounding", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
