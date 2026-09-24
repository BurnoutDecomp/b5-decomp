"""FX-AINAN2: NaN polarity of BrnAI::IsInsideSectionFast (BrnAIUtils.cpp).

Extracts the production body and feeds it a NaN probe point and NaN edge data. See
FxAinan2AIUtils.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_ai_utils.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, definition, parse_args  # noqa: E402

UTILS = "src/GameSource/World/AI/BrnAIUtils.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(UTILS)
    chunks = ["namespace BrnAI {",
              definition(source, "    bool IsInsideSectionFast(const void* lpSectionEdges, f32 lfX, f32 lfY)"),
              "}"]
    sys.exit(compile_and_run("FxAinan2AIUtils.cpp", chunks, prefix="brn_fxainan2_utils_"))


if __name__ == "__main__":
    main()
