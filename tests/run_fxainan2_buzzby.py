"""FX-AINAN2: BuzzBy's no-buzz zones (the two image tables) and AICarCanBuzz's NaN polarity.

Extracts the production KA_NO_BUZZ_ZONE_CENTRES / KAF_NO_BUZZ_ZONE_RADII tables and the
BuzzBy::IsPositionInNoBuzzZone / BuzzBy::AICarCanBuzz bodies from BrnAIBuzzBy.cpp. See
FxAinan2BuzzBy.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_buzzby.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import re
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, definition, parse_args  # noqa: E402

BUZZBY = "src/GameSource/World/AI/BrnAIBuzzBy.cpp"


def table(source, name):
    """One `static const <type> NAME[...] = ...;` table, single- or multi-line."""
    match = re.search(r"static const \w+\s+" + re.escape(name) + r"\[[^\]]*\]\s*=[^;]*;", source)
    if match is None:
        raise ValueError(f"no table {name}")
    return match[0]


def main():
    args = parse_args()
    source = Tree(args.rev).read(BUZZBY)
    chunks = ["namespace BrnAI {",
              table(source, "KA_NO_BUZZ_ZONE_CENTRES"),
              table(source, "KAF_NO_BUZZ_ZONE_RADII"),
              definition(source, "    bool BuzzBy::AICarCanBuzz("),
              definition(source, "    bool BuzzBy::IsPositionInNoBuzzZone("),
              "}"]
    sys.exit(compile_and_run("FxAinan2BuzzBy.cpp", chunks, prefix="brn_fxainan2_buzz_"))


if __name__ == "__main__":
    main()
