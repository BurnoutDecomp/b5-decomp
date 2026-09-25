"""FX-TAILS-A item 7 (crash parity 2026-09-24): CollideLineAgainstPolySoupList's result-block overrun tripwire.

REVIEW-E risk 2: the all-hits line driver (ARTIST 0x82812AE0) fills a result block sized for 80-byte records
(PrepareNewPrimitiveTestResultsList, Malloc((5 * max) << 4) @0x828108AC) with 112-byte soup records and never tests
`found < max` before calling the kernel (0x82812CE4 / 0x82813178). That console behaviour is kept; the tripwire is a
default-on, one-shot `[scene] ... overran` log line (NOT IN THE X360 BINARY) so a PC overrun is visible.

Extracts the PRODUCTION body, KF_SHORT_LINE_LENGTH_SQ, LeafOverlapsBoxXYZ and NoteLineSoupListOverrun (a no-op stub
when the revision has none) and runs them against a recording kernel and a capturing gpDebugPrint. See
FxTailsASceneOverrun.cpp.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_scene_overrun.py [--rev <rev>]
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, body_or_stub, compile_and_run, constant, definition, parse_args  # noqa: E402

GENERATOR = "src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.cpp"
EXTRA = [REPO / "src/GameShared/GameClasses/Geometric/Primitives/CgsLine.cpp",
         REPO / "src/GameShared/GameClasses/Geometric/Intersection/CgsLineTests.cpp",
         REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"]


def main():
    args = parse_args()
    source = Tree(args.rev).read(GENERATOR)
    chunks = ["namespace CgsSceneManager { namespace CgsCollision {",
              "namespace {",
              constant(source, "KF_SHORT_LINE_LENGTH_SQ"),
              definition(source, "inline bool LeafOverlapsBoxXYZ("),
              body_or_stub(source, "void NoteLineSoupListOverrun(",
                           "void NoteLineSoupListOverrun(s32, u16) {}   // absent in this revision"),
              # The line box's VMX max / min (2026-09-25, FX-FOLLOWUPS); a revision before them has none to call.
              body_or_stub(source, "inline f32 VmxMaxFp(", ""),
              body_or_stub(source, "inline f32 VmxMinFp(", ""),
              "}",
              definition(source, "u16 BaseCollisionGenerator::CollideLineAgainstPolySoupList("),
              "} }"]
    sys.exit(compile_and_run("FxTailsASceneOverrun.cpp", chunks, extra_sources=EXTRA,
                             prefix="brn_fxtailsa_sceneov_"))


if __name__ == "__main__":
    main()
