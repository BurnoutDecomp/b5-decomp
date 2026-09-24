"""FX-FPUMAX item 3: PhysicalTrafficManager::CheckForTrafficHittingWater @0x8261DDF0 indexes
KAB_SURFACE_IS_WATER with the raw surface id, as the console does (0x8261E00C `lbzx byte_82FB7DF4[id]`,
no bound test) -- the host `lu8SurfaceId < KI_MAX_NUM_SURFACES` guard 8ffb4270 added is gone -- and the
facts that make an out-of-table id impossible on the shipped data stay true:

  R1  the read is `KAB_SURFACE_IS_WATER[lu8SurfaceId]` with no host bound;
  I1  the id is (the AGTR tag's low halfword & KU_COLLISION_MASK_SURFACE_ID) >> 4, read only under mbValid;
  I2  KU_COLLISION_MASK_SURFACE_ID == 1008 (bits 4..9: a 6-bit field, so the table bound matters);
  I3  the ONLY `mAboveGroundTestResult.mbValid = true` in the tree is SimpleVehiclePhysics::
      SetAboveGroundTestResult (@0x82602880), which assembles the tag as (lo << 16) | hi;
  I4  a traffic body's AGTR comes only from the traction-line consumer, as (tag >> 16, tag & 0xFFFF) of
      the job's ground-line mauSurfaceTag; the scene-manager path asserts for traffic (request type 3)
      and its other call targets maRaceCarVehicles;
  I5  that tag is the nearest accepted triangle's Triangle4 lane tag, and Triangle4 tags are the
      polygon-soup polys' muSurfaceTag;
  D1  DATA: every poly of every PolygonSoupList in build/game/WORLDCOL.BIN gives
      ((tag >> 16) & 0x3F0) >> 4 < KI_MAX_NUM_SURFACES (measured: exactly 0..19, the surfacelist's 20);
  D2  DATA: no other bundle under build/game carries a PolygonSoupList (type 0x43) resource.

usage: run_fxfpumax_water_surface.py [--rev <b5-decomp revision>] [--no-data]
    --rev R     read every source from revision R (the RED side: the fix commit's parent)
    --no-data   skip D1/D2 (reported as SKIPPED -- they need build/game and build/tools/yap/YAP.exe)
"""
from pathlib import Path
import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, REPO, WORKFLOW

MAINT = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_MaintenanceEvents.cpp"
CONSTANTS = "src/GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
TAG = "src/SharedClasses/World/BrnCollisionTag.h"
SIMPLE = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.cpp"
TRACTION = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_TractionLineTests.cpp"
UPDATE = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_UpdateVehiclePhysics.cpp"
JOB = "src/GameShared/Jobs/ContactGenerator/ContactGeneratorJob.cpp"
SOUP = "src/GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.cpp"


class Tree:
    def __init__(self, rev):
        self.rev = rev

    def read(self, relative):
        if self.rev is None:
            return (REPO / relative).read_text(encoding="utf-8-sig")
        return subprocess.run(["git", "-C", str(REPO), "show", f"{self.rev}:{relative}"], check=True,
                              capture_output=True, text=True, encoding="utf-8").stdout

    def grep(self, word):
        """{path: text} of every src/ .cpp/.h that mentions `word` (tracked + untracked, or at the revision)."""
        command = ["git", "-C", str(REPO), "grep", "-l"]
        command += ["--untracked", "-F", word] if self.rev is None else ["-F", word, self.rev]
        listing = subprocess.run(command + ["--", "src/*.cpp", "src/*.h"], capture_output=True, text=True,
                                 encoding="utf-8").stdout
        paths = [line.split(":", 1)[1] if self.rev else line for line in listing.splitlines() if line]
        return {path: self.read(path) for path in paths}


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def worldcol_scan(limit):
    """D1: every PolygonSoupList poly in WORLDCOL.BIN -> the set of site surface ids."""
    sys.path.insert(0, str(WORKFLOW / "tools" / "assets" / "bundles"))
    import world_support_transcode as wst
    bundle = WORKFLOW / "build" / "game" / "WORLDCOL.BIN"
    if not bundle.is_file() or not os.path.isfile(wst.YAP):
        raise FileNotFoundError(f"{bundle} / {wst.YAP}")
    tmp = tempfile.mkdtemp(prefix="fxfpumax_wc_")
    try:
        subprocess.run([wst.YAP, "e", str(bundle), tmp], check=True, capture_output=True)
        folder = Path(tmp) / "PolygonSoupList"
        ids, polys, lists = set(), 0, 0
        for dat in sorted(folder.glob("*.dat")):
            lists += 1
            for soup in wst.parse_polygonsouplist_le(dat.read_bytes())["soups"]:
                for tag, _, _ in soup["polys"]:
                    polys += 1
                    ids.add(((tag >> 16) & 0x3F0) >> 4)
        return ids, polys, lists
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def bundles_with_polygon_soups():
    """D2: {bundle: count} of PolygonSoupList (type 0x43) resources under build/game (bnd2 entries, 0x40 each)."""
    hits = {}
    for path in (WORKFLOW / "build" / "game").rglob("*"):
        if not path.is_file():
            continue
        with open(path, "rb") as handle:
            head = handle.read(0x30)
            if head[:4] != b"bnd2":
                continue
            order = "<" if struct.unpack("<I", head[4:8])[0] == 2 else ">"
            count, entries = struct.unpack(order + "II", head[0x10:0x18])
            handle.seek(entries)
            table = handle.read(0x40 * count)
        found = sum(1 for i in range(count)
                    if struct.unpack(order + "I", table[0x40 * i + 56:0x40 * i + 60])[0] == 0x43)
        if found:
            hits[path.name] = found
    return hits


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    parser.add_argument("--no-data", action="store_true")
    args = parser.parse_args()
    tree = Tree(args.rev)
    checks = []

    def check(name, ok, detail=""):
        checks.append((name, bool(ok)))
        print(f"{'PASS' if ok else 'FAIL'}  {name}" + (f"  [{detail}]" if detail and not ok else ""))

    maint = tree.read(MAINT)
    body = code_only(definition(maint, "void PhysicalTrafficManager::CheckForTrafficHittingWater("))
    check("R1 the water read is KAB_SURFACE_IS_WATER[lu8SurfaceId] with no host bound (0x8261E00C lbzx, none on the console)",
          re.search(r"if\s*\(\s*KAB_SURFACE_IS_WATER\[lu8SurfaceId\]\s*\)", body)
          and "KI_MAX_NUM_SURFACES" not in body and "< 32" not in body)
    flat = re.sub(r"\s+", "", body)
    valid_at = flat.find("if(lpAboveGroundTest->mbValid)")
    check("I1 the id is (the AGTR tag's low halfword & KU_COLLISION_MASK_SURFACE_ID) >> 4, read only under mbValid",
          valid_at >= 0
          and flat.find("lpAboveGroundTest->mCollisionTag", valid_at) > valid_at
          and "static_cast<u16>(lCollisionTag.muValue&0xFFFFu)" in flat
          and "static_cast<u8>((lu16Tag&BrnWorld::KU_COLLISION_MASK_SURFACE_ID)>>4)" in flat)
    check("I2 KU_COLLISION_MASK_SURFACE_ID == 1008 (a 6-bit field at bits 4..9)",
          re.search(r"KU_COLLISION_MASK_SURFACE_ID\s*=\s*1008\s*;", code_only(tree.read(TAG))))

    raisers = [(path, m.start()) for path, text in tree.grep("mAboveGroundTestResult").items()
               for m in re.finditer(r"mAboveGroundTestResult\.mbValid\s*=\s*true", code_only(text))]
    simple = tree.read(SIMPLE)
    setter = squash(definition(simple, "void SimpleVehiclePhysics::SetAboveGroundTestResult("))
    check("I3 the only AGTR `mbValid = true` is SetAboveGroundTestResult, tag = (lo << 16) | hi",
          len(raisers) == 1 and raisers[0][0] == SIMPLE
          and "mAboveGroundTestResult.mbValid=true" in setter
          and "(static_cast<u32>(lu16TagLo)<<16)|static_cast<u32>(lu16TagHi)" in setter,
          f"raisers={raisers}")

    callers = [(path, line) for path, text in tree.grep("SetAboveGroundTestResult").items()
               for line in code_only(text).splitlines()
               if "SetAboveGroundTestResult(" in line and "void " not in line]
    traction = squash(tree.read(TRACTION))
    update = squash(tree.read(UPDATE))
    check("I4 traffic AGTRs come only from the traction-line consumer as (tag >> 16, tag & 0xFFFF) of mauSurfaceTag[4]; "
          "the scene-manager path asserts for traffic",
          sorted(path for path, _ in callers) == sorted([TRACTION, UPDATE])
          and "constu32luTag=lpResult->mauSurfaceTag[liGroundLine];" in traction
          and "lpBody->SetAboveGroundTestResult(lvPosition,lvNormal,static_cast<u16>(luTag>>16),static_cast<u16>(luTag&0xFFFFu));" in traction
          and "maRaceCarVehicles[luRaceCarIndex].SetAboveGroundTestResult(" in update
          and "Aphysicaltrafficvehicletriedsubmittedan'aboveground'linetesttotheSceneManager" in update,
          f"callers={callers}")
    job = squash(tree.read(JOB))
    soup = squash(tree.read(SOUP))
    check("I5 the job's tag is the nearest accepted Triangle4 lane tag; Triangle4 tags are polygon-soup poly tags",
          "lpResult->mauSurfaceTag[liLine]=lauBestTag[liLine];" in job
          and "lauBestTag[liLine]=LaneBits(lrBlock.mSurfaceTags,liLane);" in job
          and "lpBuffer->mauSurfaceTags[liLane]=luSurfaceTag;" in soup
          and "constu32luTag=lpPoly->muSurfaceTag;" in soup
          and "lpPoly->muSurfaceTag);" in soup)

    if args.no_data:
        print("SKIPPED  D1/D2 (--no-data)")
    else:
        limit = int(re.search(r"KI_MAX_NUM_SURFACES\s*=\s*(\d+)\s*;", code_only(tree.read(CONSTANTS))).group(1))
        try:
            ids, polys, lists = worldcol_scan(limit)
            check(f"D1 every WORLDCOL.BIN poly's site surface id < KI_MAX_NUM_SURFACES ({limit}) "
                  f"[{lists} lists, {polys} polys, ids {min(ids)}..{max(ids)}, {len(ids)} distinct]",
                  max(ids) < limit)
        except FileNotFoundError as missing:
            check(f"D1 WORLDCOL.BIN scan (needs {missing})", False)
        hits = bundles_with_polygon_soups()
        check(f"D2 WORLDCOL.BIN is the only bundle with PolygonSoupList resources {hits}",
              list(hits) == ["WORLDCOL.BIN"])

    failures = sum(1 for _, ok in checks if not ok)
    print(f"run_fxfpumax_water_surface: {len(checks)} checks, {failures} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
