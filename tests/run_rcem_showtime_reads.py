"""FX-RCEM (crash-parity 2026-09-22): the showtime reads of RaceCarEntityModule's reset pump.

WriteUpdatedAIData @0x822D1FC8 and CheckForResetOnTrackConditions @0x822CE9E0 both read module
+0x1823D, which is mCrashPlayManager (+0x180F0) . mbIsInShowtime (+0x14D). Until 2026-09-22 both
read a phantom `mbIsInShowtimeMode` carved out of the module's tail pad that nothing wrote, so
the player's slot never published showtime to the AI and the in-air reset was never suppressed.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rcem_showtime_reads.py [ResetPump.cpp] [Module.h]
Optional arguments: pre-fix snapshots of BrnRaceCarEntityModule_ResetPump.cpp and
BrnRaceCarEntityModule.h (the RED side).
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

MODULE_DIR = REPO / "src/GameSource/World/EntityModules/RaceCarEntityModule"
PUMP = MODULE_DIR / "BrnRaceCarEntityModule_ResetPump.cpp"
HEADER = MODULE_DIR / "BrnRaceCarEntityModule.h"


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def structural(header_text):
    """The phantom must be gone, and the pad run between the active-car array and
    mfCurrentTailgateDuration must still span +0x100E0..+0x182F0 with no member inside."""
    failures = []
    code = code_only(header_text)
    if re.search(r"\bbool\s+mbIsInShowtimeMode\s*;", code):
        failures.append("header still declares the phantom `bool mbIsInShowtimeMode` (+0x1823D is "
                        "mCrashPlayManager.mbIsInShowtime)")
    start = code.index("ActiveRaceCar maActiveRaceCars[")
    end = code.index("f32 mfCurrentTailgateDuration", start)
    span = code[code.index(";", start) + 1:end]
    total = 0
    for declaration in [d.strip() for d in span.split(";") if d.strip()]:
        pad = re.fullmatch(r"u8\s+maTailPadA0\w*\s*\[\s*(0x[0-9A-Fa-f]+)\s*-\s*(0x[0-9A-Fa-f]+)\s*\]", declaration)
        if pad:
            total += int(pad.group(1), 16) - int(pad.group(2), 16)
        elif re.fullmatch(r"bool\s+mbIsInShowtimeMode", declaration):
            total += 1
        else:
            failures.append("unexpected declaration in the +0x100E0..+0x182F0 run: " + declaration)
    if total != 0x182F0 - 0x100E0:
        failures.append(f"tail run spans 0x{total:X} bytes, console span is 0x{0x182F0 - 0x100E0:X}")
    return failures


def main():
    pump_path = Path(sys.argv[1]) if len(sys.argv) > 1 else PUMP
    header_path = Path(sys.argv[2]) if len(sys.argv) > 2 else HEADER
    pump = pump_path.read_text(encoding="utf-8-sig")

    write_ai = definition(pump, "void RaceCarEntityModule::WriteUpdatedAIData(")
    expression = re.search(r"const bool lbIsInShowtime\s*=\s*([^;]+);", code_only(write_ai)).group(1).strip()
    check = definition(pump, "void RaceCarEntityModule::CheckForResetOnTrackConditions()")
    in_air = definition(check, "if( lpState->mfTimeInAir > KF_MAX_TIME_IN_AIR")
    constants = [re.search(r"const f32 KF_MAX_TIME_IN_AIR\s*=[^;]+;", pump).group(0),
                 re.search(r"const s32 gsiDebugSuppressInAirReset\s*=[^;]+;", pump).group(0)]
    print("showtime expression:", expression)

    failures = structural(header_path.read_text(encoding="utf-8-sig"))
    for failure in failures:
        print("FAIL (structural):", failure)
    print(f"structural: 2 checks, {len(failures)} failures")

    methods = ("bool RaceCarEntityModule::PublishedShowtime(EActiveRaceCarIndex leCar) const\n{\n"
               f"    const bool lbIsInShowtime = {expression};\n    return lbIsInShowtime;\n}}\n"
               "bool RaceCarEntityModule::InAirVerdict(const RaceCarState* lpState, bool lbNeedsReset) const\n{\n"
               + in_air + "\n    return lbNeedsReset;\n}\n")
    with tempfile.TemporaryDirectory(prefix="brn_rcem_showtime_") as directory:
        output = Path(directory)
        (output / "rcem_showtime_constants.inc").write_text("\n".join(constants) + "\n", encoding="utf-8")
        (output / "rcem_showtime_reads.inc").write_text(methods, encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / entry}"' for entry in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes
                   + f' /I"{output}" "{Path(__file__).with_name("RcemShowtimeReads.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output)
    sys.exit(1 if (failures or result.returncode) else 0)


if __name__ == "__main__":
    main()
