"""Regression for the Road Rage / Survivor / Marked Man persistent-damage chain (crash parity
G67-D1 / G67-D2 / G67-D3).

Run from the workflow checkout:
    python b5-decomp/tests/run_persistent_damage.py [--pre-fix <b5 rev>]

1. PersistentDamage.cpp compiles the shipped RaceCar::IncreasePersistentDamage (inlined at
   0x822F4264), RaceCarEntityModule::GetPersistentDamageCarCount @0x822A4A38 and the taken-down AI
   block of ProcessRaceCarCrashCompleteEvents (0x822F40C8..0x822F4390), then drives takedowns
   through them. Pre-fix the block only logged a PARK line and neither helper had a body: a rival
   never carried damage into its respawn.
2. Structural: ResetActiveRaceCar's non-player arm (0x822F4A7C..0x822F4AFC) hands the car's
   mfPersistentDamage to ResetRaceCar as lfHowCloseToTotalled. Pre-fix it was a PARK, so even a
   damaged rival respawned clean.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings
from run_showtime_impulse import definition

DIR = "src/GameSource/World/EntityModules/RaceCarEntityModule/"
BLOCK_START = "// 0x822F40C8..0x822F4390 -- the taken-down AI persistent-damage block."
BLOCK_END = "// 0x822F4394 -- the merge point of both arms; always cleared."


def read(rel, rev):
    if rev:
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
    return (REPO / rel).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


def main():
    rev = sys.argv[sys.argv.index("--pre-fix") + 1] if "--pre-fix" in sys.argv else None
    failures = []
    racecar = read(DIR + "BrnRaceCar.cpp", rev)
    crashexit = read(DIR + "BrnRaceCarEntityModule_CrashExit.cpp", rev)
    module = read(DIR + "BrnRaceCarEntityModule.cpp", rev)

    reset = definition(module, "void RaceCarEntityModule::ResetActiveRaceCar(")
    if ("KU_FLAG_AI_PERSISTENT_DAMAGE" not in reset
            or "lfHowCloseToTotalled = lpGlobalRaceCar->GetPersistentDamage()" not in reset):
        failures.append("G67-D3 ResetActiveRaceCar must hand a non-player car's persistent damage to ResetRaceCar")

    pieces = {}
    try:
        consts = [l for l in racecar.splitlines() if l.startswith("static const f32 KF_PERSISTENT_DAMAGE_")]
        pieces["increase.inc"] = "\n".join(consts) + "\n" + definition(racecar, "bool RaceCar::IncreasePersistentDamage()")
    except ValueError:
        failures.append("G67-D1 RaceCar::IncreasePersistentDamage has no body")
    try:
        pieces["count.inc"] = definition(crashexit, "s32 RaceCarEntityModule::GetPersistentDamageCarCount() const")
    except ValueError:
        failures.append("G67-D2 GetPersistentDamageCarCount has no body")
    if BLOCK_START in crashexit and BLOCK_END in crashexit:
        i = crashexit.index(BLOCK_START)
        pieces["block.inc"] = crashexit[i:crashexit.index(BLOCK_END, i)]
    else:
        failures.append("G67-D1 ProcessRaceCarCrashCompleteEvents has no taken-down persistent-damage block")

    rc = 1
    if len(pieces) == 3:
        with tempfile.TemporaryDirectory(prefix="brn_persist_") as directory:
            out = Path(directory)
            for name, text in pieces.items():
                (out / name).write_text(text + "\n", encoding="utf-8")
            cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + f' /I"{out}" "'
                   + str(Path(__file__).with_name("PersistentDamage.cpp")) + '" /Fe:regression.exe /link /OPT:REF')
            script = out / "run.cmd"
            script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                              'if errorlevel 1 exit /b 1\n' + cmd + ' >build.log 2>&1\n'
                              'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                              '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
            rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    for f in failures:
        print("FAIL:", f)
    print(f"structural/extraction: {len(failures)} failures; harness rc={rc}")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
