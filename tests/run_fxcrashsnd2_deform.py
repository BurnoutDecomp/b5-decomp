"""FX-CRASHSND2 (crash parity 2026-09-24, item 3): the deformation legs of the collision sound.

ARTIST UpdateResolver 0x826F940C..0x826F94B0 turns what the deformation system and the car-part contact
spies report into collisions: UpdateGlass @0x826D4850 (glass builder sub_826BE398), UpdateHingingBodyParts
@0x826D44D0 (hinge builder sub_826BDF60, through HingeStateCache Update @0x826830D8 / Insert @0x826831A8),
ImportContactSpies on the broken-joint and detached-part queues (builders sub_826BE108 / sub_826BE250)
and on ContactSpyData's physical car-part contacts (builder sub_826BDCB8), with MapBodyPartEnumToMateral
@0x82688CF8. The PC had none of them -- the sound input carried an opaque copy of the deformation
output until item 1 typed it -- so a smashed windscreen, a door swinging open, a wheel coming off
never reached the collision sound.

Numeric: tests/FxCrashSnd2Deform.cpp compiles the PRODUCTION builders region (item 1's mappers), the
deformation region, the HingeStateCache bodies, TrafficClassToSize, GetRaceCar and GetTrafficEntityIndex
against a fixture manager / sound input. --rev reads a b5 revision (the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashsnd2_deform.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, WORKFLOW, definition, code_only, body_or_empty, compile_and_run, report
from run_fxcrashsnd2_builders import builders_region, parent_build_script

MANAGER_CPP = "src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp"
MANAGER_H = "src/GameSource/Sound/Collision/BrnCollisionStateManager.h"
HINGE_CPP = "src/GameSource/Sound/Collision/BrnHingeStateCache.cpp"
HINGE_H = "src/GameSource/Sound/Collision/BrnHingeStateCache.h"
RACE_CAR_CACHE_CPP = "src/GameSource/Sound/Collision/BrnRaceCarCache.cpp"
TRAFFIC_STATE_H = "src/GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.h"
TRAFFIC_IFACE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.cpp"
UPDATE_RESOLVER = "void CollisionStateManager::UpdateResolver("
BANNER = "THE DEFORMATION LEGS (FX-CRASHSND2 item 3)"
LAST = "void CollisionStateManager::UpdateHingingBodyParts("
NUMERIC_CHECKS = 43


def deform_region(manager):
    try:
        banner = manager.index(BANNER)
    except ValueError:
        raise ValueError("the deformation-legs banner")
    start = manager.rindex("\n// ====", 0, banner) + 1
    body = definition(manager, LAST)
    return manager[start:manager.index(body) + len(body)]


def wiring(tree, rev):
    resolver = body_or_empty(tree.read(MANAGER_CPP), UPDATE_RESOLVER)
    header = code_only(tree.read(MANAGER_H))
    hinge_h = code_only(tree.read(HINGE_H))
    order = [re.search(pattern, resolver) for pattern in (
        r"ImportContactSpies\(\s*\*\s*lrContacts\s*\.\s*GetTrafficContacts\(\)",
        r"UpdateGlass\(\s*lrDeformation\s*\)\s*;",
        r"UpdateHingingBodyParts\(\s*&\s*lrDeformation\s*\.\s*mJointedPartStateQueue\s*\)\s*;",
        r"ImportContactSpies\(\s*lrDeformation\s*\.\s*mBrokenJointNotificationQueue\s*,",
        r"ImportContactSpies\(\s*lrDeformation\s*\.\s*mDetachedPartNotificationQueue\s*,",
        r"ImportContactSpies\(\s*\*\s*lrContacts\s*\.\s*GetPhysicalCarPartContacts\(\)\s*,",
        r"ImportContactSpies\(\s*\*\s*lrContacts\s*\.\s*GetPropContacts\(\)")]
    mount = re.search(r'echo\s+"%SRC%\\GameSource\\Sound\\Collision\\BrnHingeStateCache\.cpp"', parent_build_script(rev))
    return [
        ("UpdateResolver runs the deformation legs in the console's order: traffic, glass, hinged parts, "
         "broken joints, detached parts, car-part contacts, props (0x826F93C0..0x826F9504)",
         all(match is not None for match in order) and
         all(order[i].start() < order[i + 1].start() for i in range(len(order) - 1))),
        ("the deformation legs read the sound input's DeformationOutputInterface (the third input getter, r16)",
         re.search(r"lrDeformation\s*=\s*lrInput\s*\.\s*GetDeformationInterface\(\)\s*;", resolver) is not None),
        ("CollisionStateManager holds HingeStateCache mHingeCache (DWARF h:791, X360 +0x12F0)",
         re.search(r"HingeStateCache\s+mHingeCache\s*;", header) is not None),
        ("HingeStateCache::CacheNode's event is the real JointedPartStateEvent (no opaque placeholder)",
         re.search(r"typedef\s+BrnPhysics::Deformation::JointedPartStateEvent\s+JointedPartStateEvent\s*;", hinge_h)
         is not None and "mOpaque" not in hinge_h),
        ("the parent mounts BrnHingeStateCache.cpp (tools/build/build_game_exe.bat)", mount is not None),
    ]


def numeric(tree):
    try:
        manager = tree.read(MANAGER_CPP)
        builders = builders_region(manager)
        deform = deform_region(manager)
        hinge_cpp = tree.read(HINGE_CPP)
        hinge = "\n\n".join(definition(hinge_cpp, signature) for signature in (
            "void HingeStateCache::Update(",
            "HingeStateCache::CacheNode* HingeStateCache::FindInCache(",
            "HingeStateCache::CacheNode* HingeStateCache::Insert("))
        generic = definition(tree.read(MANAGER_H), "struct GenericEntity\n{") + ";"
        class_to_size = definition(tree.read(TRAFFIC_STATE_H), "static ETrafficSize TrafficClassToSize(")
        get_race_car = definition(tree.read(RACE_CAR_CACHE_CPP),
                                  "const RaceCarCache::RaceCarCacheNode* RaceCarCache::GetRaceCar(")
        get_traffic = definition(tree.read(TRAFFIC_IFACE_CPP),
                                 "const TrafficSoundEntity* TrafficSoundOutputInterface::GetTrafficEntityIndex(")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxCrashSnd2Deform.cpp"), "fxcrashsnd2_deform_region.inc",
                           deform, "FxCrashSnd2Deform",
                           extra_files={"fxcrashsnd2_builders_region.inc": builders,
                                        "fxcrashsnd2_hinge_cache_bodies.inc": hinge,
                                        "fxcrashsnd2_generic_entity.inc": generic,
                                        "fxcrashsnd2_traffic_class_to_size.inc": class_to_size,
                                        "fxcrashsnd2_get_race_car.inc": get_race_car,
                                        "fxcrashsnd2_get_traffic_entity_index.inc": get_traffic})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxcrashsnd2_deform", wiring(tree, args.rev), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
