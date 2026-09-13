"""Exercise the production takedown consumers with controlled vehicle state."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings
from run_rival_recovery_camera import definition


def main():
    source = (REPO / "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp").read_text(encoding="utf-8")
    methods = [definition(source, "s32 RaceCarEntityModule::GetDamagedCarCount() const"),
               definition(source, "void RaceCarEntityModule::ProcessTakedownEvents(")]
    boost = definition(source, "void RaceCarEntityModule::UpdateBoost(")
    start = boost.index("    const RaceCarEntityModuleIO::TakedownEventQueue*")
    end = boost.index("    const BrnGameState::GameStateModuleIO::CarScoreData&", start)
    # Extract the event-consumer block verbatim; unrelated throttle/speed inputs are
    # supplied by the live game regression rather than emulated in this fixture.
    event_block = boost[start:end]
    trace_start = event_block.index("            // FLAG PC diagnostic:")
    trace_end = event_block.index("        }", trace_start)
    event_block = event_block[:trace_start] + event_block[trace_end:]
    methods.append("void RaceCarEntityModule::UpdateBoostTakedowns(const Input* lpInput) {\n"
                   + event_block + "\n}")
    with tempfile.TemporaryDirectory(prefix="brn_rival_damage_") as directory:
        output = Path(directory)
        (output / "rival_damage_methods.inc").write_text("\n".join(methods), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("RivalDamage.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=output, check=True)


if __name__ == "__main__":
    main()
