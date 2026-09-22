"""FX-RCEM (crash-parity 2026-09-22): the Showtime arm of
RaceCarEntityModule::HandlePrepareForModeAction (ARTIST 0x823092F0, 0x82309980..0x823099D4) stores
1 into the player car's +0x782 (ActiveRaceCar::mbIsWrecked). Until 2026-09-22 the PC arm stored
ActiveRaceCar::SetInShowtime(true) (+0x788) there instead and never latched the wreck.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem_showtime_wreck_latch.py [ModeArming.cpp snapshot]
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

SOURCE = REPO / "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_ModeArming.cpp"


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else SOURCE
    source = path.read_text(encoding="utf-8-sig")
    handler = definition(source, "void RaceCarEntityModule::HandlePrepareForModeAction(")
    arm = re.search(r"if \(lpGameModeParams->GetFlag\(\s*BrnGameState::GameModeParams::"
                    r"KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR\)\)", handler)
    block = definition(handler[arm.start():], "if (lpGameModeParams->GetFlag(")
    print("extracted showtime arm:", len(block.splitlines()), "lines")
    body = ("void RaceCarEntityModule::ShowtimeArm(const GameModeParamsFixture* lpGameModeParams)\n{\n"
            + block + "\n}\n")
    with tempfile.TemporaryDirectory(prefix="brn_rcem_showtime_wreck_") as directory:
        output = Path(directory)
        (output / "rcem_showtime_wreck_latch.inc").write_text(body, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("RcemShowtimeWreckLatch.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output)
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
