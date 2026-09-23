"""Regression for the detached-wheel vs car pair (crash parity G22-D1 / G22-D2).

Run from the workflow checkout:
    python b5-decomp/tests/run_wheel_car_pair.py [--pre-fix <b5 rev>]

1. WheelCarPair.cpp links the shipped CgsPrimitivePairListBuilder.cpp and appends a CYLINDER-vs-BOX
   pair through AddPrimitivePair(Cylinder*, Box*) @0x828149F8 (pre-fix: the overload does not exist
   and the harness fails to compile).
2. Structural: DeformationManager::AddRaceCarWheelPair @0x82605BE8 appends that pair (pre-fix it
   was a logging GATE), and ReadPotentialVehicleWorldContact gates on the contact NORMAL (+0x20,
   0x826045AC), not mPointOnA (G22-D2).
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings

BUILDER = "src/GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.cpp"
BRIDGES = "src/GameSource/Physics/DeformationManager/BrnDeformationManager_ContactBridges.cpp"


def read(rel, rev):
    if rev:
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout
    return (REPO / rel).read_text(encoding="utf-8-sig")


def body(src, sig):
    start = src.index(sig)
    i = src.index("{", start)
    depth = 0
    while True:
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start:i + 1]
        i += 1


def main():
    rev = sys.argv[sys.argv.index("--pre-fix") + 1] if "--pre-fix" in sys.argv else None
    failures = []
    bridges = read(BRIDGES, rev)
    wheel = body(bridges, "void DeformationManager::AddRaceCarWheelPair(")
    if "lpBuilder->AddPrimitivePair(&lWheelCylinder, &lCarBox" not in wheel:
        failures.append("G22-D1 AddRaceCarWheelPair must append the wheel-cylinder vs car-box pair")
    gate = body(bridges, "void DeformationManager::ReadPotentialVehicleWorldContact(")
    if "IsValidVec3Lanes(lrPotentialContact.mNormal)" not in gate:
        failures.append("G22-D2 ReadPotentialVehicleWorldContact must validate the contact normal")
    for f in failures:
        print("FAIL:", f)

    with tempfile.TemporaryDirectory(prefix="brn_wheelcar_") as directory:
        out = Path(directory)
        tu = out / "CgsPrimitivePairListBuilder.cpp"
        tu.write_text(read(BUILDER, rev), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public "
               + includes + f' "{Path(__file__).with_name("WheelCarPair.cpp")}" "{tu}"'
               + " /Fe:regression.exe /link /OPT:REF /FORCE:UNRESOLVED")
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + cmd + ' >build.log 2>&1\n'
                          'if not exist regression.exe (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    print(f"structural: 2 checks, {len(failures)} failures")
    sys.exit(1 if (rc or failures) else 0)


if __name__ == "__main__":
    main()
