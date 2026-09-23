"""Regression for two VehicleManager defects (crash-parity 2026-09-22, G43-D1 and G40-D1/D3).

Run from the workflow checkout:
    python b5-decomp/tests/run_vehicle_cache_and_stuck.py [--pre-fix <b5 rev>]

G43-D1  VehicleManager::UpdateTriangleCache @0x82615C38 / PhysicalTrafficManager::UpdateTriangleCache
        @0x825EE640: the triangle-cache inner radius is |mHalfExtent + kvfVehicleTriangleCachePadding|
        (0x82615D70 / 0x825EE7E8 vaddfp; the padding is splat(1.0f) written by CRT thunk 0x82C5A470).
G40-D1  VehicleManager::DoPlayerStuckLineTests @0x825C3A70: occluded = hit && !((tag >> 16) & 0x2000)
        (0x825C4934 srwi 16) -- the MATERIAL halfword's DRIVEABLE bit, not the group halfword's.
G40-D3  KF_STUCK_LINETEST_ANGULARCUTOFF_SQ is the image word flt_82093CE4 == 0x3CECBFB2.

The radius and occlusion statements are EXTRACTED from the production sources (so the test tests the
shipped text) and compiled into a tiny harness with the canonical flags. --pre-fix <rev> extracts
them from that git revision instead, which is how the RED side is shown.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_rival_impacts import REPO, WORKFLOW, settings

VM = "src/GameSource/Physics/VehicleManager/"


def read(rel, rev):
    if rev:
        return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True,
                              text=True, encoding="utf-8", check=True).stdout
    return (REPO / rel).read_text(encoding="utf-8-sig")


def between(src, start, end):
    a = src.index(start)
    b = src.index(end, a)
    return src[a:b]


def main():
    rev = None
    if "--pre-fix" in sys.argv:
        rev = sys.argv[sys.argv.index("--pre-fix") + 1]
    veh = read(VM + "BrnVehicleManager_Prepare.cpp", rev)
    traf = read(VM + "BrnPhysicalTrafficManager.cpp", rev)
    stuck = read(VM + "BrnVehicleManager_PlayerStuck.cpp", rev)
    consts = read(VM + "BrnVehicleConstants.h", rev)

    veh_block = between(veh[veh.index("void VehicleManager::UpdateTriangleCache("):],
                        "const Vector3 lvHalfExtent = lrCar.GetHalfExtent();", "const Vector3& lrPosition")
    traf_block = between(traf[traf.index("void PhysicalTrafficManager::UpdateTriangleCache("):],
                         "const Vector3 lvHalfExtent = lpBody->GetHalfExtent();", "const Vector3& lrPosition")
    occl = between(stuck, "const bool lbOccluded =", ";") + ";"
    cutoff = re.search(r"const f32 KF_STUCK_LINETEST_ANGULARCUTOFF_SQ = ([^;]+);", stuck).group(1)
    pad = re.search(r"const f32 KVF_VEHICLE_TRIANGLE_CACHE_PADDING = ([^;]+);", consts)

    harness = []
    harness.append("namespace BrnPhysics { namespace Vehicle {")
    harness.append("const f32 KVF_VEHICLE_TRIANGLE_CACHE_PADDING = %s;" % (pad.group(1) if pad else "0.0f /* absent pre-fix */"))
    harness.append("struct Car { Vector3 he; Vector3 GetHalfExtent() const { return he; } };")
    harness.append("f32 VehicleRadius(const Car& lrCar) {\n" + veh_block + "\n return lfRadius; }")
    harness.append("f32 TrafficRadius(const Car* lpBody) {\n" + traf_block + "\n return lfRadius; }")
    harness.append("struct LineTestHit { bool mbHit; u32 muSurfaceTag; };")
    harness.append("bool Occluded(const LineTestHit& lHit) {\n" + occl + "\n return lbOccluded; }")
    harness.append("const f32 KF_CUTOFF = %s;" % cutoff)
    harness.append("} }")

    with tempfile.TemporaryDirectory(prefix="brn_vehcache_") as directory:
        out = Path(directory)
        (out / "extracted.inc").write_text("\n".join(harness), encoding="utf-8")
        includes = " ".join(f'/I"{WORKFLOW / p}"' for p in settings("msvc_includes.txt"))
        cmd = ("cl " + " ".join(settings("msvc_flags.txt")) + " " + includes + f' /I"{out}" "'
               + str(Path(__file__).with_name("VehicleCacheAndStuck.cpp")) + '" /Fe:regression.exe /link /OPT:REF')
        script = out / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat") + '" >nul 2>&1\n'
                          'if errorlevel 1 exit /b 1\n' + cmd + '\nif errorlevel 1 exit /b 1\n'
                          '.\\regression.exe\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        rc = subprocess.run(["cmd", "/c", str(script)], cwd=out).returncode
    sys.exit(rc)


if __name__ == "__main__":
    main()
