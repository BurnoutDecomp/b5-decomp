"""FX-AINAN2: NaN polarity of AICarOutputInterface::SetAICarDistanceToCheckpoint's assert
(SharedIO/BrnAICarOutputInterface.cpp).

Extracts the production body and counts the asserts a NaN / negative / ordered distance fires.
See FxAinan2AICarOutput.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_ai_car_output.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, definition, parse_args  # noqa: E402

OUTPUT = "src/GameSource/World/AI/SharedIO/BrnAICarOutputInterface.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(OUTPUT)
    chunks = ["namespace BrnAI { namespace AIModuleIO {",
              definition(source, "    void AICarOutputInterface::SetAICarDistanceToCheckpoint("),
              "} }"]
    sys.exit(compile_and_run("FxAinan2AICarOutput.cpp", chunks, prefix="brn_fxainan2_aiout_"))


if __name__ == "__main__":
    main()
