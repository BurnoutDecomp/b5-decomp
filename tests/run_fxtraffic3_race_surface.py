"""FX-TRAFFIC3 item 3: VehicleManager::CrashFatalRaceCars @0x826361C0 indexes KAB_SURFACE_IS_WATER with the
raw surface id, as the console does (0x826363C4 `lbzx r31, r31, r14`, r14 = byte_82FB7DF4 from 0x82636298;
the KI_NUM_USED_SURFACES assert at 0x826363A0 falls through, no bound test) -- the host
`liSurfaceId < KI_MAX_NUM_SURFACES &&` guard is gone -- and the facts that make an out-of-table id
impossible for a RACE CAR on the shipped data stay true. (FX-FPUMAX proved the same for traffic's AGTR tag,
50055bb9; a race car reads its WHEEL road-contact tag instead, so the chain is re-derived here.)

  R1  the read is `KAB_SURFACE_IS_WATER[liSurfaceId]` with no host bound;
  I1  the id is (the wheel mRoadContact tag's low halfword & KU_COLLISION_MASK_SURFACE_ID) >> 4
      (0x8263638C `lhz 0xA6` of the 48-byte stack copy = +0x26, `srwi 4`, `clrlwi 26`);
  I2  KU_COLLISION_MASK_SURFACE_ID == 1008 (bits 4..9: a 6-bit field, so the table bound matters);
  I3  the only write of a physics wheel's mRoadContact.mCollisionTag is Wheel::SetRoadContact, which packs
      (lo << 16) | hi -- the low halfword read in I1 is its lu16TagHi argument; every other write targets an
      output WheelLite copy, and no whole-RoadContact assignment exists;
  I4  SetRoadContact's only caller is SimpleVehiclePhysics::AddTractionPoint, passing (tag >> 16, tag &
      0xFFFF); RaceCarPhysics forwards to it; the race-car callers pass the traction-line job's
      mauSurfaceTag[wheel] and the PPU re-test's muSurfaceTag, both run over the car's GetCache() window;
  I5  those are Triangle4 lane tags (LaneBits(mSurfaceTags, lane)); the only Triangle4 tag writer copies the
      polygon-soup polys' muSurfaceTag, and the triangle-cache fill job is ExtractTriangle4ListIntersectingSphere;
  D1  DATA: every poly of every PolygonSoupList in build/game/WORLDCOL.BIN gives ((tag >> 16) & 0x3F0) >> 4
      < KI_MAX_NUM_SURFACES (measured by FX-FPUMAX: exactly 0..19);
  D2  DATA: no other bundle under build/game carries a PolygonSoupList (type 0x43) resource.

usage: env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic3_race_surface.py [--rev R] [--no-data]
"""
import argparse
import re
import sys

sys.dont_write_bytecode = True
from run_fxfpumax_water_surface import Tree, code_only, squash, definition, worldcol_scan, bundles_with_polygon_soups

CRASH = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_CrashState.cpp"
CONSTANTS = "src/GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
TAG = "src/SharedClasses/World/BrnCollisionTag.h"
WHEEL = "src/GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.cpp"
SIMPLE = "src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.cpp"
RACECAR = "src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.cpp"
TRACTION = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_TractionLineTests.cpp"
STUCK = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_PlayerStuck.cpp"
TRAFFIC_TRACTION = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_TractionLineTests.cpp"
JOB = "src/GameShared/Jobs/ContactGenerator/ContactGeneratorJob.cpp"
SOUP = "src/GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.cpp"
FILL = "src/GameShared/Jobs/PolygonSoupTester/PolygonSoupTesterJob.cpp"


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

    body = code_only(definition(tree.read(CRASH), "void VehicleManager::CrashFatalRaceCars("))
    flat = re.sub(r"\s+", "", body)
    check("R1 the water read is KAB_SURFACE_IS_WATER[liSurfaceId] with no host bound (0x826363C4 lbzx, none on the console)",
          "constboollbIsWater=KAB_SURFACE_IS_WATER[liSurfaceId];" in flat
          and "KI_MAX_NUM_SURFACES" not in body and "<32" not in flat)
    check("I1 the id is (the wheel mRoadContact tag's low halfword & KU_COLLISION_MASK_SURFACE_ID) >> 4",
          ".mRoadContact;" in flat
          and "constu16lu16Tag=static_cast<u16>(lRoadContact.mCollisionTag.muValue&0xFFFFu);" in flat
          and "consts32liSurfaceId=static_cast<s32>((lu16Tag&BrnWorld::KU_COLLISION_MASK_SURFACE_ID)>>4);" in flat)
    check("I2 KU_COLLISION_MASK_SURFACE_ID == 1008 (a 6-bit field at bits 4..9)",
          re.search(r"KU_COLLISION_MASK_SURFACE_ID\s*=\s*1008\s*;", code_only(tree.read(TAG))))

    # I3: every assignment INTO a road contact's tag, or of a whole road contact.
    writes, whole = [], []
    for path, text in tree.grep("mRoadContact").items():
        code = code_only(text)
        for m in re.finditer(r"([\w\.\[\]]*)mRoadContact\.mCollisionTag(?:\.muValue)?\s*=(?!=)", code):
            writes.append((path, m.group(1)))
        for m in re.finditer(r"([\w\.\[\]]*)mRoadContact\s*=(?!=)", code):
            whole.append((path, m.group(1)))
    wheel_writes = [w for w in writes if not w[1].startswith("lrWheelLite.")]
    setter = squash(definition(tree.read(WHEEL), "void Wheel::SetRoadContact("))
    check("I3 the only write of a physics wheel's mRoadContact tag is Wheel::SetRoadContact, packing (lo << 16) | hi",
          wheel_writes == [(WHEEL, "")] and not whole
          and "mRoadContact.mCollisionTag.muValue=(static_cast<u32>(lu16TagLo)<<16)|static_cast<u32>(lu16TagHi);" in setter,
          f"writes={writes} whole={whole}")

    set_callers = sorted({path for path, text in tree.grep("SetRoadContact").items()
                          for line in code_only(text).splitlines()
                          if "SetRoadContact(" in line and "void " not in line})
    simple = squash(definition(tree.read(SIMPLE), "void SimpleVehiclePhysics::AddTractionPoint("))
    racecar = squash(tree.read(RACECAR))
    add_callers = sorted({path for path, text in tree.grep("AddTractionPoint").items()
                          for line in code_only(text).splitlines()
                          if "AddTractionPoint(" in line and "void " not in line})
    traction = squash(tree.read(TRACTION))
    stuck = squash(tree.read(STUCK))
    check("I4 SetRoadContact <- SimpleVehiclePhysics::AddTractionPoint (tag >> 16, tag & 0xFFFF) <- the race-car traction "
          "job's mauSurfaceTag and the PPU re-test's muSurfaceTag, both over the car's GetCache() window",
          set_callers == [SIMPLE]
          and "constu16lu16TagHi=static_cast<u16>(lu32CollisionTag>>16);" in simple
          and "lrWheel.SetRoadContact(lbIsOnGround,lbIsCloseToGround,lvPosition,lvNormal,lu16TagHi,lu16TagLo,lfLineDist);" in simple
          and "SimpleVehiclePhysics::AddTractionPoint(leWheel,lvPosition,lvNormal,lu32CollisionTag);" in racecar
          and add_callers == sorted([RACECAR, TRACTION, STUCK, TRAFFIC_TRACTION])
          and "lrCar.AddTractionPoint(leWheel,lvPosition,lvNormal,lpResult->mauSurfaceTag[liWheel]);" in traction
          and "lpCacheInterface->GetCache(liCar);" in traction
          and "lrCar.AddTractionPoint(static_cast<EVehicleDrivenWheel>(liWheel),lvPosition,lvNormal,lrHit.muSurfaceTag);" in stuck
          and "lpCacheInterface->GetCache(static_cast<s32>(luPlayer));" in stuck,
          f"SetRoadContact callers={set_callers} AddTractionPoint callers={add_callers}")

    job = squash(tree.read(JOB))
    soup = squash(tree.read(SOUP))
    fill = squash(tree.read(FILL))
    tag_writers = sorted({path for path, text in tree.grep("mSurfaceTags").items()
                          for m in re.finditer(r"(?:\bmSurfaceTags\s*=(?!=)|memcpy\(\s*&\s*\w+\.mSurfaceTags)", code_only(text))})
    check("I5 both tags are Triangle4 lane tags; the only Triangle4 tag writer copies polygon-soup poly tags; "
          "the cache fill is ExtractTriangle4ListIntersectingSphere",
          "lpResult->mauSurfaceTag[liLine]=lauBestTag[liLine];" in job
          and "lauBestTag[liLine]=LaneBits(lrBlock.mSurfaceTags,liLane);" in job
          and "lpaOut[liLine].muSurfaceTag=LaneBits(lrBlock.mSurfaceTags,liLane);" in stuck
          and tag_writers == [SOUP]
          and "std::memcpy(&lrDest.mSurfaceTags,lpBuffer->mauSurfaceTags,4*sizeof(u32));" in soup
          and "lpBuffer->mauSurfaceTags[liLane]=luSurfaceTag;" in soup
          and "constu32luTag=lpPoly->muSurfaceTag;" in soup
          and "lpPoly->muSurfaceTag);" in soup
          and "CgsGeometric::ExtractTriangle4ListIntersectingSphere(" in fill,
          f"Triangle4 tag writers={tag_writers}")

    if args.no_data:
        print("SKIPPED  D1/D2 (--no-data)")
    else:
        limit = int(re.search(r"KI_MAX_NUM_SURFACES\s*=\s*(\d+)\s*;", code_only(tree.read(CONSTANTS))).group(1))
        try:
            ids, polys, lists = worldcol_scan(limit)
            check(f"D1 every WORLDCOL.BIN poly's race-car surface id ((tag >> 16) & 0x3F0) >> 4 < KI_MAX_NUM_SURFACES ({limit}) "
                  f"[{lists} lists, {polys} polys, ids {min(ids)}..{max(ids)}, {len(ids)} distinct]",
                  max(ids) < limit)
        except FileNotFoundError as missing:
            check(f"D1 WORLDCOL.BIN scan (needs {missing})", False)
        hits = bundles_with_polygon_soups()
        check(f"D2 WORLDCOL.BIN is the only bundle with PolygonSoupList resources {hits}",
              list(hits) == ["WORLDCOL.BIN"])

    failures = sum(1 for _, ok in checks if not ok)
    print(f"run_fxtraffic3_race_surface: {len(checks)} checks, {failures} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
