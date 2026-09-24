"""FX-CRASHSND2 (crash parity 2026-09-24, item 2): the collision-sound scrape legs.

ARTIST CollisionStateManager::UpdateScrapes @0x826D3F50 (DWARF cpp:2678), called by UpdateResolver
after the contact imports and before the culls (0x826F9720), with UpdateScrapeHistory @0x826BEB98,
FindOldestScrapeInHistory sub_82688A58, FindEntity @0x826A0398 and UniqueScrape @0x82688B20, and
UpdateResolver's history reset (0x826F92D0..0x826F9328: the impact time changed, or the fatality
turned to E_FATAL_START). The PC had none of them: the 16-entry scrape history was never written, so
FindInScrapeHistory never matched, no collision state ever took the SCRAPE lifetime (the only thing
that starts ScrapeEffect's AEMS_ScrapeGranulator voice), and every frame of a grind along a wall was
a fresh impact instead of one impact and a scrape.

Numeric: tests/FxCrashSnd2Scrapes.cpp compiles the PRODUCTION scrape region of
BrnCollisionStateManager.cpp (the banner through UpdateScrapes), FindInScrapeHistory, the entity-id
helpers, GenericEntity, ScrapeInfo::operator== / UpdateHistory, CollisionState::Attach, the base
StateManager::GetFreeState and GetTrafficEntityIndex against a fixture manager / state list / input.
--rev reads a b5 revision (the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashsnd2_scrapes.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report

MANAGER_CPP = "src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp"
MANAGER_H = "src/GameSource/Sound/Collision/BrnCollisionStateManager.h"
DATA_CPP = "src/GameSource/Sound/Collision/BrnCollisionDataStructures.cpp"
STATE_CPP = "src/GameSource/Sound/Collision/BrnCollisionState.cpp"
BASE_MANAGER_CPP = "src/GameShared/GameClasses/Sound/Logic/CgsStateManager.cpp"
TRAFFIC_IFACE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.cpp"

UPDATE_RESOLVER = "void CollisionStateManager::UpdateResolver("
UPDATE_SCRAPES = "void CollisionStateManager::UpdateScrapes(const BrnSound::Logic::FrameInformation& lrFrame)"
BANNER = "THE SCRAPE LEGS (FX-CRASHSND2 item 2)"
NUMERIC_CHECKS = 54


def scrapes_region(manager):
    """The production text from the scrape banner's rule line through the end of UpdateScrapes."""
    try:
        banner = manager.index(BANNER)
    except ValueError:
        raise ValueError("the scrape-legs banner")
    start = manager.rindex("\n// ====", 0, banner) + 1
    body = definition(manager, UPDATE_SCRAPES)
    end = manager.index(body) + len(body)
    return manager[start:end]


def wiring(tree):
    manager = tree.read(MANAGER_CPP)
    resolver = body_or_empty(manager, UPDATE_RESOLVER)
    scrapes = body_or_empty(manager, UPDATE_SCRAPES)
    header = code_only(tree.read(MANAGER_H))

    last_import = resolver.rfind("ImportContactSpies(")
    call = re.search(r"\bUpdateScrapes\(\s*lrFrame\s*\)\s*;", resolver)
    cull = re.search(r"\bCullInputCollisions\(\)\s*;", resolver)
    reset = re.search(r"if\s*\(\s*mFrameInformation\s*\.\s*meImpactTime\s*\.\s*HasChanged\(\)\s*\|\|\s*"
                      r"mFrameInformation\s*\.\s*meFatality\s*\.\s*HasChangedTo\(\s*E_FATAL_START\s*\)\s*\)\s*"
                      r"\{\s*for\s*\([^)]*E_MAX_SCRAPE_HISTORY[^)]*\)\s*"
                      r"maScrapeHistory\s*\[\s*\w+\s*\]\s*\.\s*mbValid\s*=\s*false\s*;\s*\}", resolver)
    count_reset = resolver.find("mu32InputCollisionCount = 0;")
    generic = re.search(r"struct\s+GenericEntity\s*\{(.*?)\};", header, re.S)
    generic_members = generic is not None and all(
        re.search(pattern, generic.group(1)) is not None
        for pattern in (r"Vector3\s+mPosition\s*;", r"Vector3\s+mVelocity\s*;", r"bool\s+mbCrashing\s*;",
                        r"bool\s+mbPlayer\s*;", r"bool\s+mbWorld\s*;"))
    declarations = all(re.search(pattern, header) is not None for pattern in (
        r"void\s+UpdateScrapes\(\s*const\s+BrnSound::Logic::FrameInformation&",
        r"ScrapeInfo\*\s+FindOldestScrapeInHistory\(\s*\)\s*;",
        r"bool\s+FindEntity\(\s*const\s+EntityId&\s*\w*\s*,\s*GenericEntity&\s*\w*\s*\)\s*const\s*;",
        r"bool\s+UniqueScrape\(\s*const\s+BrnSound::Logic::Collision::ScrapeInfo&",
        r"void\s+UpdateScrapeHistory\(\s*const\s+BrnSound::Logic::FrameInformation&"))
    return [
        ("UpdateResolver calls UpdateScrapes(lrFrame) after the contact imports and before CullInputCollisions "
         "(0x826F9718 mr r4,r26 ; bl 0x826D3F50 ; bl CullInputCollisions)",
         call is not None and cull is not None and last_import != -1 and last_import < call.start() < cull.start()),
        ("UpdateResolver forgets the scrape history when the impact time changes or the fatality turns to "
         "E_FATAL_START, before the input count is reset (0x826F92D0..0x826F9328)",
         reset is not None and count_reset != -1 and reset.start() < count_reset),
        ("a continuing scrape takes a free state through the BASE StateManager::GetFreeState (0x826D4310: a "
         "direct bl 0x8268D7D0, not the collision manager's priority-stealing override)",
         re.search(r"CgsSound::Logic::StateManager::GetFreeState\(\s*nullptr\s*\)", scrapes) is not None),
        ("GenericEntity (DWARF h:102) carries mPosition / mVelocity / mbCrashing / mbPlayer / mbWorld",
         generic_members),
        ("CollisionStateManager declares UpdateScrapes / FindOldestScrapeInHistory / FindEntity / UniqueScrape / "
         "UpdateScrapeHistory (DWARF h:827..h:907)", declarations),
    ]


def numeric(tree):
    try:
        manager = tree.read(MANAGER_CPP)
        region = scrapes_region(manager)
        find_in_history = definition(
            manager, "BrnSound::Logic::Collision::ScrapeInfo*\nCollisionStateManager::FindInScrapeHistory(")
        helpers = re.search(r"u32 GetEntityOwner\(EntityId lEntityId\).*?const u32 KU_ENTITY_OWNER_PROP\s*=\s*3;",
                            manager, re.S)
        if helpers is None:
            raise ValueError("the entity-id helpers")
        generic = definition(tree.read(MANAGER_H), "struct GenericEntity\n{") + ";"
        data_cpp = tree.read(DATA_CPP)
        scrape_bodies = (definition(data_cpp, "bool ScrapeInfo::operator==( const ScrapeInfo& lInfo ) const")
                         + "\n\n" + definition(data_cpp, "void ScrapeInfo::UpdateHistory( const ScrapeInfo& lInfo )"))
        attach = definition(tree.read(STATE_CPP), "void CollisionState::Attach(void* apvAttachment)")
        base_free = definition(tree.read(BASE_MANAGER_CPP), "State* StateManager::GetFreeState(void* /*apvAttachment*/)")
        get_traffic = definition(tree.read(TRAFFIC_IFACE_CPP),
                                 "const TrafficSoundEntity* TrafficSoundOutputInterface::GetTrafficEntityIndex(")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxCrashSnd2Scrapes.cpp"), "fxcrashsnd2_scrapes_region.inc",
                           region, "FxCrashSnd2Scrapes",
                           extra_files={"fxcrashsnd2_find_in_scrape_history.inc": find_in_history,
                                        "fxcrashsnd2_entity_helpers.inc": helpers.group(0),
                                        "fxcrashsnd2_generic_entity.inc": generic,
                                        "fxcrashsnd2_scrape_info_bodies.inc": scrape_bodies,
                                        "fxcrashsnd2_collision_state_attach.inc": attach,
                                        "fxcrashsnd2_base_get_free_state.inc": base_free,
                                        "fxcrashsnd2_get_traffic_entity_index.inc": get_traffic})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxcrashsnd2_scrapes", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
