"""FX-RCEM2 (crash-parity 2026-09-23, G13-X5): BrnGui::OfflineRivalShutdown (POST_RIVAL), the
free-burn rival-shutdown presentation the screen flow enters on GUI 373 ("TO_RVL_POST").

  structural  every console body exists in BrnOfflineRivalShutdown.cpp (ARTIST 0x824B94D0 ..
              0x824DAFE0), the TU is mounted in tools/build/build_game_exe.bat, and the
              LANDING-ORDER GUARD holds: the game-state translator's 120 -> GUI 373 arm may exist
              only once GuiCache::RecEvent writes mShutdownCarID (case 373) -- otherwise this
              state's GetShutdownCarID() asserts on kCGSID_NULL the moment it gets the cache.
  numeric     the WHOLE production `namespace BrnGui { ... }` body is extracted verbatim and run
              inside tests/Rcem2RivalShutdown.cpp's recording fixture (the presentation ladder,
              every post, every console diagnostic).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem2_rival_shutdown.py [--pre-fix <b5 rev>]
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import settings, REPO, WORKFLOW
from run_rcem_takedown_actions import switch_arm
from run_showtime_impulse import definition

STATE = "src/GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineRivalShutdown.cpp"
TRANSLATOR = "src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp"
CACHE = "src/GameSource/Gui/BrnGuiCache.cpp"
MOUNT = WORKFLOW / "tools/build/build_game_exe.bat"
BODIES = ("Construct", "OnEnter", "OnLeave", "Update", "HandleIncomingEvents", "AppendExpectedComponents",
          "SetupComponents", "HandleAptTriggers", "HandleControllerInput", "HandleControllerInputPressed")


def read(rel):
    """The file at --pre-fix <rev>, else the working tree; None when it does not exist there."""
    if "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
        result = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                                text=True, encoding="utf-8")
        return result.stdout.replace("\r\n", "\n") if result.returncode == 0 else None
    path = REPO / rel
    return path.read_text(encoding="utf-8-sig").replace("\r\n", "\n") if path.exists() else None


def main():
    state = read(STATE)
    failures = []

    # ---- structural ---------------------------------------------------------------------------
    for name in BODIES:
        if state is None or not re.search(r"^\s*(?:void|bool)\s+OfflineRivalShutdown::" + name + r"\(", state, re.M):
            failures.append(f"OfflineRivalShutdown::{name} has no body in {STATE}")
    mounted = "BrnOfflineRivalShutdown.cpp" in MOUNT.read_text(encoding="utf-8", errors="replace")
    if "--pre-fix" not in sys.argv and not mounted:
        failures.append("BrnOfflineRivalShutdown.cpp is not mounted in tools/build/build_game_exe.bat "
                        "(its bodies would never be compiled)")
    translator = read(TRANSLATOR) or ""
    arm = None
    if "void BrnGameModule::TranslateGameActionsToGuiEvents(" in translator:
        arm = switch_arm(definition(translator, "void BrnGameModule::TranslateGameActionsToGuiEvents("),
                         "E_ACTION_SHUTDOWN")
    cache = read(CACHE) or ""
    writes_shutdown_id = re.search(r"\bmShutdownCarID\s*=(?!=)", cache) is not None
    if arm and not writes_shutdown_id:
        failures.append("the translator's 120 -> GUI 373 arm is landed but GuiCache never writes mShutdownCarID "
                        "(RecEvent case 373): OfflineRivalShutdown would assert kCGSID_NULL on entry")
    for failure in failures:
        print("FAIL (structural):", failure, flush=True)
    print(f"structural: {len(BODIES) + 2} checks, {len(failures)} failures"
          f"  [120 arm {'present' if arm else 'absent'}, mShutdownCarID writer {'present' if writes_shutdown_id else 'absent'}]", flush=True)

    if state is None or "namespace BrnGui\n{\n" not in state:
        print("numeric: NOT BUILDABLE -- the production TU does not exist at this revision")
        sys.exit(1)

    # ---- numeric: the whole production namespace body, verbatim ---------------------------------
    start = state.index("namespace BrnGui\n{\n") + len("namespace BrnGui\n{\n")
    end = state.rstrip().rindex("}")
    body = state[start:end]
    with tempfile.TemporaryDirectory(prefix="brn_rcem2_rival_shutdown_") as directory:
        output = Path(directory)
        (output / "rcem2_rival_shutdown.inc").write_text(body + "\n", encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("Rcem2RivalShutdown.cpp")}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + ' >build.log 2>&1\nif errorlevel 1 (type build.log & exit /b 1)\n'
                          + 'regression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output)
    sys.exit(1 if (failures or result.returncode) else 0)


if __name__ == "__main__":
    main()
