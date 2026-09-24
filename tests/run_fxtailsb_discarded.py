"""FX-TAILS-B (crash parity 2026-09-24, item 1): the discarded-contact leg of the collision sound.

ARTIST CollisionStateManager::UpdateResolver runs, between the traffic contacts and the glass leg
(0x826F93C4..0x826F9408), ImportContactSpies<EventQueue<DiscardedContact,20>> @0x826DD1C0 on the queue
the inlined ContactSpyInterface::GetDiscardedContacts returns (the "mpData != NULL" tripwire at
BrnContactSpyInterface.h:219 = 0xDB, then mpData + 0x193C0 = ContactSpyData::mDiscardedContactQueue),
building each record with InputCollision(const CameraInfo&, CollisionStateManager&, const DiscardedContact&,
const LogicInputBuffer&, f32, f32) = sub_826BDAE8 (DWARF h:424). The PC had no accessor at either level
(ContactSpyData::GetDiscardedContacts was declared with no body), no builder and no leg.

Console fact the witness relies on: the queue is empty on every frame on the console too -- the only
writer of its source (VehicleManager::mDiscardedContacts) is PhysicsModule::BridgeSimulationToOutput's
drain; nothing appends to that queue (Construct's bind, two Clears and the drain are the only
instructions on its seats). The last wiring check guards against an invented producer.

Numeric: tests/FxTailsBDiscarded.cpp compiles the PRODUCTION builders region of BrnCollisionStateManager.cpp,
the discarded builder, the ImportContactSpies member template, TrafficClassToSize, GetRaceCar and
GetTrafficEntityIndex against a fixture manager / sound input, with the real ContactSpyData /
ContactSpyInterface / InputCollision headers (the revision's own on --rev). --rev reads a b5 revision
(the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsb_discarded.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, REPO, definition, code_only, body_or_empty, compile_and_run, report
from run_fxcrashsnd2_builders import builders_region, balanced_end

MANAGER_CPP = "src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp"
STRUCTURES_H = "src/GameSource/Sound/Collision/BrnCollisionDataStructures.h"
INTERFACE_H = "src/GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"
DATA_H = "src/GameSource/Physics/ContactSpies/BrnContactSpyData.h"
RACE_CAR_CACHE_CPP = "src/GameSource/Sound/Collision/BrnRaceCarCache.cpp"
TRAFFIC_STATE_H = "src/GameSource/Sound/Vehicles/Traffic/BrnTrafficStateManager.h"
TRAFFIC_IFACE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.cpp"
UPDATE_RESOLVER = "void CollisionStateManager::UpdateResolver("
IMPORT = "template <typename SpyQueue>\nvoid CollisionStateManager::ImportContactSpies("
NUMERIC_CHECKS = 26


def discarded_builder(manager):
    """The discarded InputCollision constructor, balanced from its OWN opening brace (its member
    initialiser list carries braces of its own)."""
    start = re.search(r"InputCollision::InputCollision\(const CameraInfo& lCamera, CollisionStateManager& lMgr,"
                      r"\s*const BrnPhysics::ContactSpy::DiscardedContact& lSpy", manager)
    if start is None:
        raise ValueError("the discarded InputCollision constructor")
    body_open = re.compile(r"\n\{\r?\n").search(manager, start.end())
    if body_open is None:
        raise ValueError("the discarded InputCollision constructor body")
    return manager[start.start():balanced_end(manager, body_open.start() + 1)]


def if_block(code, condition):
    """The brace-balanced block of the first `if (<condition>)` in code, or ''."""
    match = re.search(r"if\s*\(\s*" + condition + r"\s*\)\s*\{", code)
    if not match:
        return ""
    depth = 0
    for index in range(match.end() - 1, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return code[match.start():index + 1]
    return ""


def producer_appends(tree):
    """Any statement appending to VehicleManager::mDiscardedContacts anywhere under src/ (the files
    `git grep` names at the revision, or in the working tree with untracked files)."""
    command = ["git", "-C", str(REPO), "grep", "-l", "-e", "mDiscardedContacts"]
    command += [tree.rev] if tree.rev else ["--untracked"]
    listing = subprocess.run(command + ["--", "src"], capture_output=True, text=True, encoding="utf-8",
                             errors="replace")
    hits = []
    for line in listing.stdout.splitlines():
        relative = line.split(":", 1)[1] if tree.rev and line.startswith(tree.rev + ":") else line
        code = code_only(tree.read(relative))
        for match in re.finditer(r"mDiscardedContacts\s*\.\s*(AddEvent|AddEventSafe|AllocateEvent|"
                                 r"AllocateEventSafe|Append|AppendSafe)\s*\(", code):
            hits.append(relative + ": " + match.group(0))
    return hits


def wiring(tree):
    resolver = body_or_empty(tree.read(MANAGER_CPP), UPDATE_RESOLVER)
    order = [re.search(pattern, resolver) for pattern in (
        r"ImportContactSpies\(\s*\*\s*lrContacts\s*\.\s*GetTrafficContacts\(\)",
        r"ImportContactSpies\(\s*\*\s*lrContacts\s*\.\s*GetDiscardedContacts\(\)\s*,\s*lrInput\s*,\s*mfCurrentTime\s*,"
        r"\s*afDeltaTime\s*\)\s*;",
        r"UpdateGlass\(\s*lrDeformation\s*\)\s*;")]
    valid_block = if_block(resolver, r"lrContacts\s*\.\s*IsValid\(\)")
    interface = code_only(tree.read(INTERFACE_H))
    data = code_only(tree.read(DATA_H))
    structures = code_only(tree.read(STRUCTURES_H))
    appends = producer_appends(tree)
    return [
        ("UpdateResolver imports the discarded contacts after the traffic contacts and before the glass leg, "
         "stamped with mfCurrentTime and the step (0x826F93C0 < 0x826F9408 < 0x826F9420)",
         all(match is not None for match in order) and order[0].start() < order[1].start() < order[2].start()),
        ("the discarded leg sits inside the contact-data test (0x826F9344 `lwz r4,0(r17) ; cmplwi ; beq` skips "
         "every import)",
         re.search(r"GetDiscardedContacts\(\)", valid_block) is not None),
        ("ContactSpyInterface::GetDiscardedContacts: the \"mpData != NULL\" tripwire (h:219), then "
         "mpData->GetDiscardedContacts() (0x826F93C4..0x826F9404)",
         re.search(r"const\s+ContactSpyData::DiscardedContactQueue\s*\*\s*GetDiscardedContacts\(\)\s*const\s*\{\s*"
                   r"CGS_ASSERT\(\s*mpData\s*!=\s*nullptr\s*,\s*\"mpData != NULL\"\s*\)\s*;\s*"
                   r"return\s+mpData\s*->\s*GetDiscardedContacts\(\)\s*;\s*\}", interface) is not None),
        ("ContactSpyData::GetDiscardedContacts returns &mDiscardedContactQueue (mpData + 0x193C0, DWARF h:138)",
         re.search(r"const\s+DiscardedContactQueue\s*\*\s*GetDiscardedContacts\(\)\s*const\s*\{\s*return\s*&\s*"
                   r"mDiscardedContactQueue\s*;\s*\}", data) is not None),
        ("InputCollision declares the DiscardedContact builder (DWARF h:424, sub_826BDAE8)",
         re.search(r"InputCollision\(\s*const\s+CameraInfo\s*&\s*lCamera\s*,\s*CollisionStateManager\s*&\s*lMgr\s*,"
                   r"\s*const\s+BrnPhysics::ContactSpy::DiscardedContact\s*&\s*lSpy\s*,\s*const\s+LogicInputBuffer"
                   r"\s*&\s*lInput\s*,\s*f32\s+lfTimeStamp\s*,\s*f32\s+lfTimeStep\s*\)\s*;", structures) is not None),
        ("no invented producer: nothing appends to VehicleManager::mDiscardedContacts (console: Construct's bind "
         "0x8263BFA0, Clears 0x82633710 / 0x82646198, the bridge's drain 0x825B055C only)"
         + ("" if not appends else " -- found: " + "; ".join(appends)),
         not appends),
    ]


def numeric(tree):
    try:
        manager = tree.read(MANAGER_CPP)
        builders = builders_region(manager)
        discarded = discarded_builder(manager)
        imports = definition(manager, IMPORT)
        class_to_size = definition(tree.read(TRAFFIC_STATE_H), "static ETrafficSize TrafficClassToSize(")
        get_race_car = definition(tree.read(RACE_CAR_CACHE_CPP),
                                  "const RaceCarCache::RaceCarCacheNode* RaceCarCache::GetRaceCar(")
        get_traffic = definition(tree.read(TRAFFIC_IFACE_CPP),
                                 "const TrafficSoundEntity* TrafficSoundOutputInterface::GetTrafficEntityIndex(")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    shadow = None
    if tree.rev:
        shadow = {path: tree.read(path) for path in (STRUCTURES_H, INTERFACE_H, DATA_H)}
    return compile_and_run(Path(__file__).with_name("FxTailsBDiscarded.cpp"), "fxtailsb_discarded_builder.inc",
                           discarded, "FxTailsBDiscarded", shadow=shadow,
                           extra_files={"fxtailsb_builders_region.inc": builders,
                                        "fxtailsb_import_contact_spies.inc": imports,
                                        "fxtailsb_traffic_class_to_size.inc": class_to_size,
                                        "fxtailsb_get_race_car.inc": get_race_car,
                                        "fxtailsb_get_traffic_entity_index.inc": get_traffic})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtailsb_discarded", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
