"""FX-EMITTER (crash parity 2026-09-24): SoundWorldScene -- the streamed static-sound-map table the world
emitters are queried from -- against ARTIST. Structural (the changes are the console's dev spew, its
data asserts and its missing null fallbacks; nothing numeric moves):

  * KB_SPEW_STATIC_MAP_INFO (DWARF BrnSoundWorldScene.cpp:38, .bss 0x82FFB8CA, default false) gates the
    four "[Static map]" spews together with gxMessageFilterFlags bit 0: "1: Load unit" (HandleWorldZoneLoad
    0x8269BF60), "2: Aqcuire unit" (Update 0x826E659C, the console's spelling), "3: Resource for unit"
    (ResourcesAreReady 0x826BA9E0), "4: Unload unit" (HandleWorldZoneUnload 0x826BAB28);
  * Query @0x826E6788 prints "[Static map] Stalled." under gxMessageFilterFlags bit 0 alone (0x826E6AA8,
    `clrldi 63`), asserts "mpLogicModule" (l.478), and per prepared zone asserts the map's miNumEntities
    < 10000 (l.493, `cmplwi 0x2710`) and meRootType < 2 (l.495) before the bounds test -- no `lpMap &&`;
  * Update @0x826E6438 asserts "lpMap" (l.273) and uses it as it stands; GetZoneMap @0x8269C028 asserts
    (l.426) and returns **(handle) with no null fallback.
  * HandleWorldZoneLoad @0x8269BF48 appends with no duplicate-zone check (the PC-only early-out is gone:
    measured over the junction-480886 pursuit, the world streamer never double-posts a unit's load).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxemitter_scene.py [--rev <rev>]
"""
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, report

SCENE_CPP = "src/SharedClasses/Sound/World/BrnSoundWorldScene.cpp"


def body(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def spews(text, literal):
    """The body prints `literal` under the spew switch (SpewStaticMapInfo() or the switch itself)."""
    return re.search(r"if\s*\(\s*(SpewStaticMapInfo\s*\(\s*\)|KB_SPEW_STATIC_MAP_INFO[^)]*)\)\s*"
                     r"\*?\s*CgsDev::Log::gpDebugPrint\s*<<\s*\"" + re.escape(literal), text) is not None


def wiring(tree):
    source = tree.read(SCENE_CPP)
    code = code_only(source)
    load = body(source, "void SoundWorldScene::HandleWorldZoneLoad(")
    unload = body(source, "void SoundWorldScene::HandleWorldZoneUnload(")
    update = body(source, "void SoundWorldScene::Update()")
    ready = body(source, "void SoundWorldScene::ResourcesAreReady()")
    query = body(source, "s32 SoundWorldScene::Query(")
    zone_map = body(source, "const BrnSound::World::StaticSoundMap* SoundWorldScene::GetZoneMap(")
    helper = body(source, "bool SpewStaticMapInfo()")
    return [
        ("KB_SPEW_STATIC_MAP_INFO is defined off by default (the BRN_EMITTER_DIAG stand-in only) and the spew "
         "gate also needs gxMessageFilterFlags bit 0",
         re.search(r"^\s*bool\s+KB_SPEW_STATIC_MAP_INFO\s*=\s*(false|std::getenv\s*\(\s*\"BRN_EMITTER_DIAG\"\s*\)"
                   r"\s*!=\s*nullptr)\s*;", code, re.M) is not None and
         re.search(r"KB_SPEW_STATIC_MAP_INFO\s*&&\s*\(\s*CgsDev::Message::gxMessageFilterFlags\s*&\s*1\s*\)", helper)
         is not None),
        ("the four console spews: 1: Load unit / 2: Aqcuire unit / 3: Resource for unit / 4: Unload unit",
         spews(load, "[Static map] 1: Load unit\\t") and spews(update, "[Static map] 2: Aqcuire unit\\t") and
         spews(ready, "[Static map] 3: Resource for unit\\t") and spews(unload, "[Static map] 4: Unload unit\\t")),
        ("Query prints \"[Static map] Stalled.\" under gxMessageFilterFlags bit 0 alone and asserts mpLogicModule",
         re.search(r"gxMessageFilterFlags\s*&\s*1\s*\)\s*!=\s*0[^;]*\)\s*\*CgsDev::Log::gpDebugPrint\s*<<\s*"
                   r"\"\[Static map\] Stalled\.\\n\"", query) is not None and
         re.search(r"CGS_ASSERT\s*\(\s*mpLogicModule\s*!=\s*0", query) is not None),
        ("Query asserts each prepared map's miNumEntities < 10000 and meRootType < 2 (l.493 / l.495) and has no "
         "`lpMap &&` fallback",
         re.search(r"GetNumEntities\s*\(\s*\)\s*\)\s*<\s*10000u", query) is not None and
         re.search(r"GetRootType\s*\(\s*\)\s*\)\s*<[^;]*E_ROOT_TYPE_COUNT", query) is not None and
         re.search(r"lpMap\s*&&", query) is None),
        ("Update uses the asserted map as it stands and GetZoneMap has no null fallback (l.273 / l.426)",
         update != "" and re.search(r"if\s*\(\s*lpMap\s*\)", update) is None and
         zone_map != "" and re.search(r"\?\s*\*", zone_map) is None and ": 0" not in zone_map),
        ("HandleWorldZoneLoad appends with no duplicate-zone check (0x8269BF48: assert l.346, append) -- "
         "no early return on a zone already in the table",
         load != "" and re.search(r"\breturn\b", load) is None and
         re.search(r"GetZone\s*\(\s*\)\s*==\s*lu16Zone", load) is None),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxemitter_scene", wiring(tree), (0, 0), 0)


if __name__ == "__main__":
    sys.exit(main())
