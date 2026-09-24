"""FX-CRASHSND2 (crash parity 2026-09-24, item 1): the collision-sound InputCollision builders.

ARTIST sub_826D3850 (regular: race-car / traffic contacts, DWARF InputCollision h:405) and
sub_826E8B20 (prop contacts, h:414), with the mappers they call -- MapPositionToOrientation
@0x8269F418, MapPositionToOrientationUsingBox @0x8269ED18, MapEntityIdToMaterial @0x826A0CF8 --
against the PC's MakeBaseInputCollision / MakePropInputCollision, which:
  * never set meOrientation (every collision Front, though SelectBin gates on it);
  * ORed 0x2000000000 into the second material (a Hex-Rays phantom: 0x826D3BE8 is `extsw` only),
    so a race car's contact with a prop (material Nothing == 1) walked the bin cache and matched
    nothing instead of taking SelectBin's `cmpldi 1` exit -- the 15 "no material match" rejects of
    fxcrashsnd_bin_cache/20260924_112457;
  * mapped traffic by crash flags instead of the car's size class (KAE_TRAFFIC_CAR_SIZE_TO_MATERIAL_TABLE
    via the traffic sound output interface, which was an opaque 4-byte copy on the sound side);
  * took facing from the contact normal instead of the camera's At row;
  * guarded dt; had no race-car pair ordering, no prop-vs-prop split, no SloMoCrash culling.

Numeric: tests/FxCrashSnd2Builders.cpp compiles the PRODUCTION region of BrnCollisionStateManager.cpp
(the traffic material table through the prop builder) with TrafficClassToSize, GetRaceCar and
GetTrafficEntityIndex against a fixture manager and input buffer. --rev reads a b5 revision (the RED
side: <fix>~1), whose parent mount line is read from the parent's HEAD.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashsnd2_builders.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, WORKFLOW, definition, code_only, body_or_empty, compile_and_run, report

MANAGER_CPP = "src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp"
MANAGER_H = "src/GameSource/Sound/Collision/BrnCollisionStateManager.h"
RACE_CAR_CACHE_CPP = "src/GameSource/Sound/Collision/BrnRaceCarCache.cpp"
TRAFFIC_STATE_H = "src/GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.h"
TRAFFIC_IFACE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.cpp"
ROOT_IO_H = "src/GameSource/Sound/Module/BrnRootSoundModuleIo.h"
ROOT_IO_CPP = "src/GameSource/Sound/Module/BrnRootSoundModuleIO.cpp"
BUILD_BAT = "tools/build/build_game_exe.bat"
UPDATE_RESOLVER = "void CollisionStateManager::UpdateResolver("
SET_TRAFFIC = "void RootInputBuffer::SetTrafficOutputInterface("
SET_DEFORMATION = "void RootInputBuffer::SetDeformationInterface("
NUMERIC_CHECKS = 58


def parent_build_script(rev):
    path = WORKFLOW / BUILD_BAT
    if rev is None:
        return path.read_text(encoding="utf-8", errors="replace")
    result = subprocess.run(["git", "-C", str(WORKFLOW), "show", "HEAD:" + BUILD_BAT],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    return result.stdout if result.returncode == 0 else ""


def balanced_end(source, open_index):
    """Index just past the brace that closes the one at open_index (comments / literals skipped)."""
    depth = 0
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',
                             source[open_index:]):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return open_index + token.end()
    raise ValueError("unterminated body")


def builders_region(manager):
    """The production text from the traffic material table's namespace through the prop builder.
    The constructor's body is balanced from its OWN opening brace (a line holding only '{'), not
    from the signature -- its member-initialiser list carries braces of its own."""
    try:
        table = manager.index("const EeMaterialType KAE_TRAFFIC_CAR_SIZE_TO_MATERIAL_TABLE")
    except ValueError:
        raise ValueError("KAE_TRAFFIC_CAR_SIZE_TO_MATERIAL_TABLE")
    start = manager.rindex("namespace", 0, table)
    prop = re.search(r"InputCollision::InputCollision\(const CameraInfo& lCamera, CollisionStateManager& lMgr,"
                     r"\s*const InputPropSpy& lSpy", manager)
    if prop is None:
        raise ValueError("the prop InputCollision constructor")
    body_open = re.compile(r"\n\{\r?\n").search(manager, prop.end())
    if body_open is None:
        raise ValueError("the prop InputCollision constructor body")
    return manager[start:balanced_end(manager, body_open.start() + 1)]


def wiring(tree, rev):
    manager = tree.read(MANAGER_CPP)
    manager_code = code_only(manager)
    header_code = code_only(tree.read(MANAGER_H))
    resolver = body_or_empty(manager, UPDATE_RESOLVER)
    root_h = code_only(tree.read(ROOT_IO_H))
    set_traffic = body_or_empty(tree.read(ROOT_IO_CPP), SET_TRAFFIC)
    imports = [re.search(r"ImportContactSpies\(\s*\*\s*lrContacts\s*\.\s*Get" + queue + r"\(\)\s*,\s*lrInput\s*,"
                         r"\s*mfCurrentTime\s*,\s*afDeltaTime\s*\)", resolver)
               for queue in ("RaceCarContacts", "TrafficContacts", "PropContacts")]
    mount = re.search(r'echo\s+"%SRC%\\GameSource\\Sound\\Collision\\BrnRaceCarCache\.cpp"', parent_build_script(rev))
    return [
        ("UpdateResolver imports race-car, traffic and prop contacts through ImportContactSpies stamped "
         "with mfCurrentTime (0x826F9370 lfs f1,4(r31))",
         all(match is not None for match in imports) and
         imports[0].start() < imports[1].start() < imports[2].start()),
        ("no 0x2000000000 material bit anywhere in the collision manager (0x826D3BE8 / 0x826BDC0C are extsw only)",
         manager_code != "" and "0x2000000000" not in manager_code),
        ("the invented MakeBaseInputCollision / MakePropInputCollision are gone (DWARF: InputCollision ctors)",
         manager_code != "" and "MakeBaseInputCollision" not in manager_code + header_code and
         "MakePropInputCollision" not in manager_code + header_code),
        ("CollisionStateManager holds RaceCarCache mRaceCarCache (DWARF h:790, X360 +0xCF0)",
         re.search(r"RaceCarCache\s+mRaceCarCache\s*;", header_code) is not None),
        ("the sound input's TrafficSoundOutputInterface is the traffic module's own type, copied with its "
         "operator= (0x823B8710 -> 0x823A7F18)",
         re.search(r"typedef\s+BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface\s+TrafficSoundOutputInterface\s*;",
                   root_h) is not None and
         re.search(r"mTrafficOutputInterface\s*=\s*\*\s*lpInterface\s*;", set_traffic) is not None),
        ("UpdateResolver refreshes mRaceCarCache from the vehicle interface only while the deformation "
         "output carries a state (0x826F91EC lwz 0x70 ; beq -> bl 0x826BF478)",
         re.search(r"lpDeformationState\s*=\s*lrInput\s*\.\s*GetDeformationInterface\(\)\s*\.\s*mpDeformationState\s*;",
                   resolver) is not None and
         re.search(r"if\s*\(\s*lpDeformationState\s*\)\s*mRaceCarCache\s*\.\s*Update\(\s*\*\s*lrInput\s*\.\s*"
                   r"GetVehicleInterface\(\)\s*,\s*\*\s*lpDeformationState\s*\)\s*;", resolver) is not None),
        ("the sound input's DeformationInterface is the deformation manager's DeformationOutputInterface, "
         "copied with its operator= (0x823C91F0 -> 0x823C8900)",
         re.search(r"typedef\s+BrnPhysics::Deformation::DeformationOutputInterface\s+DeformationInterface\s*;",
                   root_h) is not None and
         re.search(r"mDeformationInterface\s*=\s*\*\s*lpInterface\s*;",
                   body_or_empty(tree.read(ROOT_IO_CPP), SET_DEFORMATION)) is not None),
        ("the parent mounts BrnRaceCarCache.cpp (tools/build/build_game_exe.bat)", mount is not None),
    ]


def numeric(tree):
    try:
        region = builders_region(tree.read(MANAGER_CPP))
        class_to_size = definition(tree.read(TRAFFIC_STATE_H), "static ETrafficSize TrafficClassToSize(")
        cache_cpp = tree.read(RACE_CAR_CACHE_CPP)
        get_race_car = (definition(cache_cpp, "const RaceCarCache::RaceCarCacheNode* RaceCarCache::GetRaceCar(")
                        + "\n\n" + definition(cache_cpp, "void RaceCarCache::Update("))
        get_traffic = definition(tree.read(TRAFFIC_IFACE_CPP),
                                 "const TrafficSoundEntity* TrafficSoundOutputInterface::GetTrafficEntityIndex(")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxCrashSnd2Builders.cpp"), "fxcrashsnd2_builders_region.inc",
                           region, "FxCrashSnd2Builders",
                           extra_files={"fxcrashsnd2_traffic_class_to_size.inc": class_to_size,
                                        "fxcrashsnd2_get_race_car.inc": get_race_car,
                                        "fxcrashsnd2_get_traffic_entity_index.inc": get_traffic})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxcrashsnd2_builders", wiring(tree, args.rev), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
