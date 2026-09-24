"""FX-AINAN2: NaN polarity of the three Aggressiveness speed-match setter asserts.

Extracts Aggressiveness::SetProximityToSpeedMatch / SetAcclerationRateForSpeedMatch /
SetTimeForSpeedMatch from BrnAIAggressiveness.cpp and counts the asserts a NaN and the ordered
values fire. See FxAinan2Aggressiveness.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_aggressiveness.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, definition, parse_args  # noqa: E402

AGGRESSIVENESS = "src/GameSource/World/AI/BrnAIAggressiveness.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(AGGRESSIVENESS)
    chunks = ["namespace BrnAI {",
              definition(source, "    void Aggressiveness::SetProximityToSpeedMatch("),
              definition(source, "    void Aggressiveness::SetAcclerationRateForSpeedMatch("),
              definition(source, "    void Aggressiveness::SetTimeForSpeedMatch("),
              "}"]
    sys.exit(compile_and_run("FxAinan2Aggressiveness.cpp", chunks, prefix="brn_fxainan2_aggr_"))


if __name__ == "__main__":
    main()
