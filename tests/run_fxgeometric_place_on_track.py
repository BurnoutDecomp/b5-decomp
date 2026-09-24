"""FX-GEOMETRIC (crash parity 2026-09-24, item 2): BrnWorld::PlaceOnTrackManager::PostSceneUpdate (ARTIST
0x822D3168) -- the PRODUCER of the place-on-track fine line test -- its call from RaceCarEntityModule::
PostSceneUpdate (0x822FE58C..0x822FE598, `this + 0x17850`), the retirement of the PC-only
ApplyPendingRequestsWithoutSceneQueryBringUp in the same change (the console walk does not re-test
mbToBePlacedOnTrack, so producer + bring-up would place every car twice) and the write accessor's
"Not locked for writing" tripwire (0x822B5560, BrnRaceCarEntityModuleIO.h:386).

The numeric half compiles the production PostSceneUpdate body (and the constants it names) into
FxGeometricPlaceOnTrack.cpp against the REAL InEventLineTestFine; the structural half reads the call, the
retirement and the tripwire from the sources.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgeometric_place_on_track.py [--pre-fix <b5 rev>]
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, REPO, build_and_run, code_mask, optional_definition, pre_fix_rev, read

MANAGER_CPP = "src/GameSource/World/BrnPlaceOnTrackManager.cpp"
MANAGER_H = "src/GameSource/World/BrnPlaceOnTrackManager.h"
CRASH_EXIT_CPP = RCEM + "BrnRaceCarEntityModule_CrashExit.cpp"
RCEM_IO_CPP = RCEM + "BrnRaceCarEntityModuleIO.cpp"

STUB_BODY = ("void PlaceOnTrackManager::PostSceneUpdate(RaceCarEntityModuleIO::OutputBuffer_PostScene*) "
             "{ /* absent on this revision */ }")


def constant(source, name, fallback):
    """The `... NAME = value;` declaration line, or `fallback` (a revision without it)."""
    match = re.search(r"^[^\n/]*\b" + name + r"\s*=\s*[^;]+;", code_mask(source), re.M)
    if not match:
        print("MISSING " + name)
        return fallback
    return source[match.start():match.end()].strip()


def main():
    rev = pre_fix_rev(sys.argv)
    cpp = read(MANAGER_CPP, rev)
    header = read(MANAGER_H, rev)
    crash_exit = read(CRASH_EXIT_CPP, rev)
    rcem_io = read(RCEM_IO_CPP, rev)
    failures = 0

    def check(passed, name):
        nonlocal failures
        print(("PASS " if passed else "FAIL ") + name)
        failures += 0 if passed else 1

    # --- structural ------------------------------------------------------------------------------
    post_scene = optional_definition(crash_exit, "void RaceCarEntityModule::PostSceneUpdate(")
    masked = code_mask(post_scene)
    power = masked.find("ProcessPowerParking(")
    call = re.search(r"\bmPlaceOnTrackManager\s*\.\s*PostSceneUpdate\s*\(\s*lpOutput\s*\)", masked)
    send = masked.find("SendResetOnTrackRequests(")
    check(call is not None and 0 <= power < call.start() < send,
          "S1 RaceCarEntityModule::PostSceneUpdate calls mPlaceOnTrackManager.PostSceneUpdate(lpOutput) between "
          "ProcessPowerParking and SendResetOnTrackRequests (0x822FE588 / 0x822FE598 / 0x822FE5A4)")

    retired = re.search(r"\bApplyPendingRequestsWithoutSceneQueryBringUp\b", code_mask(cpp) + code_mask(header))
    check(retired is None,
          "S2 ApplyPendingRequestsWithoutSceneQueryBringUp is gone from PlaceOnTrackManager (no declaration, body "
          "or call): the scene's answer is the only placement")

    writer = optional_definition(rcem_io, "OutputBuffer_PostScene::GetSceneFineLineTestQueue()\n{")
    check(re.search(r"CGS_ASSERT\s*\(\s*IsBufferLockedForWriting\s*\(\s*\)", code_mask(writer)) is not None,
          "S3 the non-const GetSceneFineLineTestQueue() carries the write-lock tripwire (0x822B5560, "
          "`rlwinm 29,31,31 ; bne`, BrnRaceCarEntityModuleIO.h:386)")

    declared = re.search(r"void\s+PostSceneUpdate\s*\(\s*RaceCarEntityModuleIO::OutputBuffer_PostScene\s*\*",
                         code_mask(header))
    check(declared is not None,
          "S4 PlaceOnTrackManager declares PostSceneUpdate(OutputBuffer_PostScene*) (DWARF BrnPlaceOnTrackManager.h:49)")

    # --- numeric ---------------------------------------------------------------------------------
    body = optional_definition(cpp, "void PlaceOnTrackManager::PostSceneUpdate(")
    print(("found   " if body else "MISSING ") + "PlaceOnTrackManager::PostSceneUpdate")
    consts = "\n".join([
        constant(header, "KF_PLACE_ON_TRACK_LINE_TEST_LENGTH", "const f32 KF_PLACE_ON_TRACK_LINE_TEST_LENGTH = 0.0f;"),
        constant(header, "KI_PLACE_ON_TRACK_LINE_TEST_OWNER", "const u8 KI_PLACE_ON_TRACK_LINE_TEST_OWNER = 0;"),
        constant(cpp, "K_ENTITY_TYPE_FLAG_WORLD", "static const u32 K_ENTITY_TYPE_FLAG_WORLD = 0u;"),
        constant(cpp, "KU8_ALL_VOLUME_TYPE_FLAGS", "static const u8 KU8_ALL_VOLUME_TYPE_FLAGS = 0u;"),
    ])
    pieces = {"fxg_pot_consts.inc": consts, "fxg_pot_body.inc": body or STUB_BODY}
    rc = build_and_run(REPO / "tests" / "FxGeometricPlaceOnTrack.cpp", pieces, "fxg_pot")
    print(f"harness rc={rc}; structural failures={failures}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
