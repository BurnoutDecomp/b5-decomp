"""Regression for VehicleManager::PredictCarCarIntersection @0x825C57B0 (crash parity G41-D2).

Run from the workflow checkout:
    python b5-decomp/tests/run_predict_car_car.py [--pre-fix <b5 rev>]

Until 2026-09-23 the body was a GATE returning true, so every race-car/traffic potential contact
became a slam/check/crash and the near-miss arm (TestForNearMissFreakOut) never ran. The shipped
helper block and the two member bodies are EXTRACTED from BrnVehicleManager_RaceCarTrafficContact.cpp
and linked against the real rw::collision narrow phase (every vendor collision TU the exe mounts).
--pre-fix <rev> extracts the body from that revision instead (the RED side).
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings

TU = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_RaceCarTrafficContact.cpp"
MOUNT_PREFIX = 'echo "%SRC%\\vendor\\renderware\\collision\\'


def read(rel, rev):
    if rev:
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout
    return (REPO / rel).read_text(encoding="utf-8-sig")


def collision_sources():
    """Every vendor collision TU the exe build mounts (tools/build/build_game_exe.bat)."""
    bat = (WORKFLOW / "tools/build/build_game_exe.bat").read_text(encoding="utf-8", errors="replace")
    out = []
    for line in bat.splitlines():
        line = line.strip()
        if line.startswith(MOUNT_PREFIX) and line.endswith('.cpp"'):
            rel = line[len('echo "%SRC%\\'):-1].replace("\\", "/")
            out.append(REPO / "src" / rel)
    return out


def main():
    rev = sys.argv[sys.argv.index("--pre-fix") + 1] if "--pre-fix" in sys.argv else None
    src = read(TU, rev).replace("\r\n", "\n")
    fn = src.index("bool VehicleManager::PredictCarCarIntersection(")
    if "u32 VehicleManager::CachedCarIdentity(" in src:
        start = src.index("namespace\n{\n    // unk_82FB7FD0 = splat(flt_82013A78")
    else:
        start = fn
    end = src.index("\n}\n", fn) + 3
    extracted = src[start:end]
    if "s_bPredictGateLogged" in extracted:
        extracted = "namespace { bool s_bPredictGateLogged = true; }\n" + extracted

    with tempfile.TemporaryDirectory(prefix="brn_predict_") as directory:
        out = Path(directory)
        (out / "extracted.inc").write_text(extracted, encoding="utf-8")
        # The descriptor table the narrow phase dispatches through is filled at boot by the SDK's
        # Volume::InitializeVTable, which lives in the other half of the rwcollision header fork;
        # a one-line shim TU calls it for the harness.
        (out / "vtable_shim.cpp").write_text(
            '#include "SDKs/EATech/rwcollision/volume_debug_access.h"\n'
            'int TestInitializeVolumeVTable() { return rw::collision::Volume::InitializeVTable(); }\n',
            encoding="utf-8")
        # The mounted collision TUs log through the REAL CgsDev::Log::gpDebugPrint (VolumeQuery.cpp's
        # [vvq] witness, b5 935db3cd), a different symbol from the harness's own NullPrint sink; a
        # null one keeps them silent.
        (out / "log_shim.cpp").write_text(
            '#include "GameShared/GameClasses/Development/Log/CgsLog.h"\n'
            'namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; } }\n',
            encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        sources = ([Path(__file__).with_name("PredictCarCarIntersection.cpp"), out / "vtable_shim.cpp",
                    out / "log_shim.cpp", REPO / "src/SDKs/EATech/rwcollision/volume.cpp"]
                   + collision_sources())
        cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + f' /I"{out}" '
               + " ".join(f'"{s}"' for s in sources) + " /Fe:regression.exe /link /OPT:REF")
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + cmd + ' >build.log 2>&1\n'
                          'if errorlevel 1 (findstr /i /c:"error" build.log & exit /b 1)\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    sys.exit(rc)


if __name__ == "__main__":
    main()
