"""FX-RCEM4 (crash parity 2026-09-24): RaceCarEntityModule::PostSceneUpdate's Showtime traffic publish
(ARTIST 0x822FE4BC..0x822FE554) -- SetFlag(E_FLAG_PLAYER_IS_IN_SHOWTIME_ON_GROUND,
CrashPlayManager::IsPlayerInShowtimeOnGround()) and SetShowtimeTrafficDensityScale(
CrashPlayManager::GetShowtimeTrafficDensityScale()) on the race-car -> traffic interface.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_showtime_traffic.py [--pre-fix <b5 rev>]
Extracts PostSceneUpdate, IsPlayerInShowtimeOnGround and the two interface setters into
tests/FxRcem4ShowtimeTraffic.cpp. A piece the source lacks (or only declares) is replayed as an
empty stub.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, definition, pre_fix_rev, read

CRASHEXIT = RCEM + "BrnRaceCarEntityModule_CrashExit.cpp"
CRASHPLAY = RCEM + "CrashPlay/BrnCrashPlayManager.cpp"
TRAFFIC_IF = RCEM + "SharedIO/BrnRaceCarToTrafficInterface.h"


def bodied(source, signature):
    """The brace-balanced definition starting at `signature` when it is DEFINED there (the next
    code token after the parameter list is `{`), else ''."""
    start = source.find(signature)
    while start >= 0:
        tail = re.sub(r"//[^\n]*", "", source[start + len(signature):start + len(signature) + 400])
        if tail.lstrip().startswith("{"):
            return definition(source[start:], signature)
        start = source.find(signature, start + 1)
    return ""


def main():
    rev = pre_fix_rev(sys.argv)
    onground = bodied(read(CRASHPLAY, rev), "bool CrashPlayManager::IsPlayerInShowtimeOnGround() const")
    print(("found   " if onground else "MISSING ") + "CrashPlayManager::IsPlayerInShowtimeOnGround")
    if not onground:
        onground = "bool CrashPlayManager::IsPlayerInShowtimeOnGround() const { return false; }"
    header = read(TRAFFIC_IF, rev)
    setflag = bodied(header, "void SetFlag(Flag leFlag, bool lbValue)")
    setscale = bodied(header, "void SetShowtimeTrafficDensityScale(f32 lfScale)")
    print(("found   " if setflag else "MISSING ") + "RaceCarToTrafficInterface::SetFlag body")
    print(("found   " if setscale else "MISSING ") + "RaceCarToTrafficInterface::SetShowtimeTrafficDensityScale body")
    setters = (setflag or "void SetFlag(Flag, bool) {}") + "\n" + (setscale or "void SetShowtimeTrafficDensityScale(f32) {}")
    pieces = {
        "fxrcem4_st_onground.inc": onground,
        "fxrcem4_st_setters.inc": setters,
        "fxrcem4_st_postscene.inc": definition(read(CRASHEXIT, rev), "void RaceCarEntityModule::PostSceneUpdate("),
    }
    rc = build_and_run(REPO / "tests" / "FxRcem4ShowtimeTraffic.cpp", pieces, "fxrcem4_st",
                       extra_sources=(REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp",))
    print(f"harness rc={rc}")
    sys.exit(1 if rc else 0)


if __name__ == "__main__":
    main()
