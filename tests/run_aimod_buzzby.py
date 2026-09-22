"""FX-AIMOD G05-D1: replay the production BuzzBy::Prepare and the first buzz-by choices after it.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aimod_buzzby.py [--rev <b5 rev>]
"""
import sys
sys.dont_write_bytecode = True
from aimod_common import Tree, parse_args, compile_and_run, definition, constants, REPO

BUZZBY = "src/GameSource/World/AI/BrnAIBuzzBy.cpp"


def main():
    source = Tree(parse_args().rev).read(BUZZBY)
    chunks = ["namespace BrnAI {",
              constants(source, r"^    const f32 KF_\w+\s*=[^;]+;"),
              constants(source, r"^    static CgsNumeric::Random mRandom;"),
              definition(source, "    void BuzzBy::Prepare("),
              definition(source, "    void BuzzBy::ChooseAheadOrBehind("),
              "}"]
    sys.exit(compile_and_run("AIModBuzzBy.cpp", chunks,
                             [REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"], prefix="brn_aimod_buzz_"))


if __name__ == "__main__":
    main()
