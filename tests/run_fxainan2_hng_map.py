"""FX-AINAN2: NaN polarity of the hard-no-go map (RacingLine/BrnHardNoGoMap.cpp).

Extracts the production HardNoGoMap::WriteIntoMap, SpreadHNGAlongTrack and
SpreadHNGIntoPreviousSection (with SetCorners / ClearMap / SectionLength / MapSquareOccupiedFast /
SetMapSquare and the constant namespace of that file) plus
RacingLineGenerator::HasSpreadHardNoGoLinesFinished (BrnRacingLineGenerator_Query.cpp) and feeds
them NaNs. GetHNGInterpXY is a
fixture so the interpolants can be set directly. See FxAinan2HngMap.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_hng_map.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, constant, definition, parse_args  # noqa: E402

HNG = "src/GameSource/World/AI/RacingLine/BrnHardNoGoMap.cpp"
QUERY = "src/GameSource/World/AI/RacingLine/BrnRacingLineGenerator_Query.cpp"


def main():
    args = parse_args()
    tree = Tree(args.rev)
    source, query = tree.read(HNG), tree.read(QUERY)
    chunks = ["namespace BrnAI {",
              definition(source, "namespace\n{"),
              definition(source, "void HardNoGoMap::SetCorners("),
              definition(source, "void HardNoGoMap::ClearMap("),
              definition(source, "f32 HardNoGoMap::SectionLength("),
              definition(source, "bool HardNoGoMap::MapSquareOccupiedFast("),
              definition(source, "void HardNoGoMap::SetMapSquare("),
              definition(source, "void HardNoGoMap::WriteIntoMap("),
              definition(source, "void HardNoGoMap::SpreadHNGAlongTrack("),
              definition(source, "void HardNoGoMap::SpreadHNGIntoPreviousSection("),
              "namespace {", constant(query, "KI_HNG_STRETCH_COUNT"), "}",
              definition(query, "bool RacingLineGenerator::HasSpreadHardNoGoLinesFinished("),
              "}"]
    sys.exit(compile_and_run("FxAinan2HngMap.cpp", chunks, prefix="brn_fxainan2_hng_"))


if __name__ == "__main__":
    main()
