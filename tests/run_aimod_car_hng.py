"""FX-AIMOD G07-D1: replay the production ResetOnTrackManager::TestCarHNG geometry.

Also (FX-NANPOL 2026-09-24, the G07-D1 leftover): TestCarHNG's :2216 assert -- 0x82790C40
`bl 0x8276AC48` (BrnMath::IsNormal(Vector2)) ; `bne` past FireAssert(r3 = 0x82019E68
"BrnMath::IsNormal( lDirection )", r5 = 0x8A8). Structurally it must follow the :2215 assert; the
numeric part fires it with a non-unit heading and checks a NaN heading does not (IsNormal reads NaN
as normal). BrnMathUtils.cpp is linked for the IsNormal body.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_aimod_car_hng.py [--rev <b5 rev>]
"""
import re
import sys
sys.dont_write_bytecode = True
from aimod_common import Tree, parse_args, compile_and_run, definition, REPO

AVOID = "src/GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager_AvoidObstacles.cpp"


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def main():
    source = Tree(parse_args().rev).read(AVOID)
    body = definition(source, "bool ResetOnTrackManager::TestCarHNG(")
    structural = re.search(r'CGS_ASSERT\(\s*lpAISection\s*!=\s*0\s*,\s*"lpAISection != NULL"\s*\)\s*;\s*'
                           r'CGS_ASSERT\(\s*BrnMath::IsNormal\(\s*lDirection\s*\)\s*,\s*'
                           r'"BrnMath::IsNormal\( lDirection \)"\s*\)\s*;', code_only(body))
    if structural is None:
        print("FAIL: structural -- TestCarHNG's :2216 CGS_ASSERT(BrnMath::IsNormal(lDirection)) must follow :2215 "
              "(0x82790C40 bl IsNormal(Vector2) ; bne ; FireAssert 0x8A8)")
    chunks = ["namespace BrnAI {", body, "}"]
    rc = compile_and_run("AIModCarHNG.cpp", chunks,
                         [REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",
                          REPO / "src/GameSource/Math/BrnMathUtils.cpp"],
                         prefix="brn_aimod_carhng_")
    print(f"structural: 1 check, {0 if structural else 1} failures")
    sys.exit(1 if (rc or structural is None) else 0)


if __name__ == "__main__":
    main()
