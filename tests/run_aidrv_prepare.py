"""FX-AIDRV G03-D1: replay the production AIDriver::Prepare (ARTIST @0x82792CA8) centre-line draw.

Every driver draws RandomFloat(KF_CENTRE_LINE_AHEAD_CLOSE, KF_CENTRE_LINE_AHEAD_FAR) from the shared
&AIModule::mRandom into RacingLine::mfCentreLineAhead (+0xC04) and 1/(1-v) into
mfCentreLineAheadRecip (+0xC08). See AIDrvPrepare.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aidrv_prepare.py [--rev <rev>]
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, definition, parse_args  # noqa: E402

AIDRIVER = "src/GameSource/World/AI/BrnAIDriver.cpp"


def main():
    args = parse_args()
    source = Tree(args.rev).read(AIDRIVER)
    chunks = ["namespace BrnAI {",
              definition(source, "    void AIDriver::Prepare(AISectionsData* lpSectionsData,"),
              "}"]
    extra = [REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"]
    sys.exit(compile_and_run("AIDrvPrepare.cpp", chunks, extra, prefix="brn_aidrv_prepare_"))


if __name__ == "__main__":
    main()
