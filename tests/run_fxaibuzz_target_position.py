"""FX-AIBUZZ item 3 (crash parity 2026-09-24): AIDriver::GetTargetPosition @0x8277CBF8's direct-target arm fuses
the normalise's last multiply with the position add -- `vmaddcfp128 v0, v13(dir.xz), v0(y2), v127(pos.xz)`
@0x8277CCC4, pos + dir * y2 with one rounding (FX-TAILS-A follow-up). The PC normalised first and then added.

Extracts the PRODUCTION Normalize2D / To2D helpers and AIDriver::GetTargetPosition from BrnAIDriver.cpp and runs
tests/FxAiBuzzTargetPosition.cpp on inputs where the two spellings differ.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxaibuzz_target_position.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import sys
sys.dont_write_bytecode = True
from aidrv_common import REPO, Tree, compile_and_run, definition, parse_args  # noqa: E402

AIDRIVER = "src/GameSource/World/AI/BrnAIDriver.cpp"
STRSTREAM = REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"   # the [ai-road] diag's operator<<


def main():
    args = parse_args()
    driver = Tree(args.rev).read(AIDRIVER)
    chunks = ["namespace BrnAI {",
              definition(driver, "    static Vector2 Normalize2D(Vector2 lVector)"),
              definition(driver, "    static Vector2 To2D(Vector3 lVector)"),
              definition(driver, "    Vector2 AIDriver::GetTargetPosition()"),
              "}"]
    sys.exit(compile_and_run("FxAiBuzzTargetPosition.cpp", chunks, [STRSTREAM], prefix="brn_fxaibuzz_target_"))


if __name__ == "__main__":
    main()
