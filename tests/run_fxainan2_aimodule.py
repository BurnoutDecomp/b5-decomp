"""FX-AINAN2: NaN polarity of AIModule::SetSuitabilityForAggression (BrnAIModule_Drive.cpp).

Extracts the production body and feeds it a NaN car speed and a NaN race timer. See
FxAinan2AIModule.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_aimodule.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, definition, parse_args  # noqa: E402

DRIVE = "src/GameSource/World/AI/BrnAIModule_Drive.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(DRIVE)
    chunks = ["namespace BrnAI {",
              definition(source, "void AIModule::SetSuitabilityForAggression("),
              "}"]
    sys.exit(compile_and_run("FxAinan2AIModule.cpp", chunks, prefix="brn_fxainan2_aimod_"))


if __name__ == "__main__":
    main()
