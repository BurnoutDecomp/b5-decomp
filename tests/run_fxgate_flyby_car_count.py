"""FX-GATE item 4: BrnGameState::ModeManager::GetNumberOfCarsInFlyby (X360 0x82311E38).

The console body selects the flyby manager with IsOnlineGameMode and calls its vtable slot 1: the offline
manager's is `li r3, 0 ; blr` (0x827E2F38), the online manager's is CalculateNumberOfCarsInFlyby
@0x82357F08, clamp(GetNumberOfNetworkPlayersStillConnected() - 1, 0, 3), on the ModeManager's own
scoring system. The body is EXTRACTED from BrnModeManager_Accessors.cpp and compiled into
FxGateFlybyCarCount.cpp. Two wiring checks pin what the extracted body leans on: GetScoringSystem()
is `return &mScoringSystem;` (ModeManager_gUI_00.cpp) and GameMode::GetIntroDurationSeconds still
calls it in its online arm (BrnGameMode.cpp, @0x82315AE4).

usage: run_fxgate_flyby_car_count.py [--rev <b5-decomp git revision>]
    default   the working tree's BrnModeManager_Accessors.cpp
    --rev R   the body as of revision R (e.g. the fix commit's parent, to show it fails)
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

SOURCE = "src/GameSource/GameState/ModeManager/BrnModeManager_Accessors.cpp"
SIGNATURE = "\ns32 ModeManager::GetNumberOfCarsInFlyby()\n"
SCORING = "src/GameSource/GameState/ModeManager/ModeManager_gUI_00.cpp"
CONSUMER = "src/GameSource/GameState/ModeManager/GameModes/BrnGameMode.cpp"


def read(path, rev=None):
    if rev:
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], check=True,
                              capture_output=True, text=True, encoding="utf-8").stdout
    return (REPO / path).read_text(encoding="utf-8-sig")


def wiring():
    failures = 0
    scoring = read(SCORING).replace("\r\n", "\n")
    getter = definition(scoring, "ScoringSystem* ModeManager::GetScoringSystem()")
    if not re.search(r"return\s+&mScoringSystem\s*;", getter):
        print("FAIL: ModeManager::GetScoringSystem() is no longer `return &mScoringSystem;` -- the online "
              "leaf must read the ModeManager's own scoring system (gsm + 0x1DD0)")
        failures += 1
    consumer = definition(read(CONSUMER).replace("\r\n", "\n"), "f32 GameMode::GetIntroDurationSeconds() const")
    if "mpModeManager->GetNumberOfCarsInFlyby()" not in consumer:
        print("FAIL: GameMode::GetIntroDurationSeconds no longer calls GetNumberOfCarsInFlyby (0x82315AE4)")
        failures += 1
    return failures


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="b5-decomp revision to take BrnModeManager_Accessors.cpp from")
    args = parser.parse_args()
    text = read(SOURCE, args.rev).replace("\r\n", "\n")
    start = text.index(SIGNATURE) + 1
    body = definition(text[start:], SIGNATURE.strip())
    failures = wiring()
    with tempfile.TemporaryDirectory(prefix="brn_fxgate_flyby_") as directory:
        output = Path(directory)
        (output / "extracted.inc").write_text(body + "\n", encoding="utf-8")
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + f' /I"{output}" '
                   + f'"{Path(__file__).with_name("FxGateFlybyCarCount.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + ' >build.log 2>&1\nif errorlevel 1 (type build.log & exit /b 1)\n'
                          + 'regression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output).returncode
    print(f"wiring: {2 - failures}/2")
    sys.exit(1 if (result or failures) else 0)


if __name__ == "__main__":
    main()
