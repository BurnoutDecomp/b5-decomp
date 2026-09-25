"""FX-GATE item 7 (crash parity 2026-09-25): CgsNumeric::Random::RandomUInt(min, max) is INCLUSIVE of max.

The console has no standalone RandomUInt(min, max); every expansion computes luMod = max - min + 1, asserts
"luMod > 0" (CgsRandom.h:303, `li r5, 0x12F`) and returns min + hi32(OLD seed) % luMod. The witness with both
bounds constant is EffectsModule::HandleShowtimeTrafficBounce's RandomUInt(150, 300): 0x82292DD8 mulhwu
0x36406C81 ; srwi 5 ; mulli 0x97 ; subf ; addi 0x96 = 150 + draw % 151. The PC body reduced by max - min.

  1. WIRING -- the body computes luMod = luMax - luMin + 1 and asserts it; the three callers pass the console's
     bounds: Randomize RandomUInt(0, KU_SIZE - 1) (& 0x1FF, 0x826C5B2C), FindRandomOldest
     RandomUInt(0, n >> 1) (0x8270294C / 0x82702950), GenerateFreeRoamingDestination RandomInt(0, (u16)n - 1)
     (0x8276960C / 0x82769610, RandomInt's asserts at :320 / :323).
  2. NUMERIC -- tests/FxGateRandomUIntRange.cpp runs the extracted RandomUInt() / RandomUInt(min, max) /
     RandomInt bodies and the three extracted caller draw expressions against the console arithmetic:
     values, the maximum reachable, one seed step per draw, no assert.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_random_uint_range.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, body_or_empty, code_only, compile_and_run, definition, report

RANDOM_CPP = "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
RANDOM_H = "src/GameShared/GameClasses/Numeric/CgsRandom.h"
SOUND_UTILS = "src/GameShared/GameClasses/Sound/CgsSoundUtils.cpp"
ROUTES = "src/GameSource/World/AI/BrnRouteRequestManager.cpp"

BODIES = ["u32 Random::RandomUInt()", "u32 Random::RandomUInt(u32 luMin, u32 luMax)",
          "s32 Random::RandomInt(s32 liMin, s32 liMax)"]
RANDOMIZE = "void SelectionHistory<512u, u16, u16, 65536ull>::Randomize(unsigned int luSeed)"
FIND_OLDEST = "StoredType SelectionHistory<tuSize, StoredType, TimeStampType, tuModulo>::FindRandomOldest("
FREE_ROAM = "void RouteRequestManager::GenerateFreeRoamingDestination("
NUMERIC_CHECKS = 2491


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def draw_call(body):
    """The first `mRandom.Random...(...)` call in a body, parentheses balanced."""
    start = body.find("mRandom.Random")
    if start < 0:
        return None
    depth = 0
    for index in range(body.index("(", start), len(body)):
        if body[index] == "(":
            depth += 1
        elif body[index] == ")":
            depth -= 1
            if depth == 0:
                return body[start:index + 1]
    return None


def callers(tree):
    sound = tree.read(SOUND_UTILS).replace("\r\n", "\n")
    routes = tree.read(ROUTES).replace("\r\n", "\n")
    return (draw_call(body_or_empty(sound, RANDOMIZE)), draw_call(body_or_empty(sound, FIND_OLDEST)),
            draw_call(body_or_empty(routes, FREE_ROAM)))


def wiring(tree):
    source = tree.read(RANDOM_CPP).replace("\r\n", "\n")
    body = squash(body_or_empty(source, BODIES[1]))
    yield ("RandomUInt(min, max): luMod = luMax - luMin + 1, CGS_ASSERT(luMod > 0), min + RandomUInt() % luMod "
           "(CgsRandom.h:303; 150 + draw % 151 at 0x82292DD8..0x82292E10)",
           "constu32luMod=luMax-luMin+1u;" in body and 'CGS_ASSERT(luMod>0u,"luMod>0");' in body
           and body.endswith("returnluMin+(RandomUInt()%luMod);}"))
    randomize, oldest, roam = (re.sub(r"\s+", "", call or "") for call in callers(tree))
    yield ("Randomize draws RandomUInt(0, KU_SIZE - 1): & 0x1FF (0x826C5B2C clrlwi 23)",
           randomize == "mRandom.RandomUInt(0,KU_SIZE-1)")
    yield ("FindRandomOldest draws RandomUInt(0, n >> 1): luMod = (n >> 1) + 1 (0x8270294C srwi / 0x82702950 addi 1)",
           oldest == "mRandom.RandomUInt(0,static_cast<u32>(luNumOfItems)>>1)")
    yield ("GenerateFreeRoamingDestination draws RandomInt(0, (u16)n - 1) (0x8276960C clrlwi 16 / 0x82769610 addi -1, "
           "asserts :320 / :323)",
           roam == "mRandom.RandomInt(0,static_cast<s32>(static_cast<u16>(luNumSections))-1)")


def numeric(tree):
    source = tree.read(RANDOM_CPP).replace("\r\n", "\n")
    try:
        bodies = [definition(source, signature) for signature in BODIES]
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    randomize, oldest, roam = callers(tree)
    if not (randomize and oldest and roam):
        print("NUMERIC: cannot build -- a caller's draw expression is missing")
        return None
    caller_text = (
        "static u32 RandomizeDraw(CgsNumeric::Random& mRandom)\n{\n"
        "    const u32 KU_SIZE = 512u;\n"
        f"    return static_cast<u16>({randomize});\n}}\n"
        "static u32 FindRandomOldestDraw(CgsNumeric::Random& mRandom, u16 luNumOfItems)\n{\n"
        "    const u32 luMod = (static_cast<u32>(luNumOfItems) >> 1) + 1u; (void)luMod;\n"
        f"    return {oldest};\n}}\n"
        "static u32 FreeRoamDraw(CgsNumeric::Random& mRandom, u32 luCount)\n{\n"
        "    struct SectionData { u32 muNumSections; } lData = { luCount };\n"
        "    const SectionData* lpAISectionData = &lData; (void)lpAISectionData;\n"
        "    const u32 luNumSections = lpAISectionData->muNumSections; (void)luNumSections;\n"
        f"    return static_cast<u16>({roam});\n}}\n")
    return compile_and_run(Path(__file__).with_name("FxGateRandomUIntRange.cpp"), "fxgate_random_uint_bodies.inc",
                           "\n".join(bodies) + "\n", "FxGateRandomUIntRange",
                           shadow={RANDOM_H: tree.read(RANDOM_H)},
                           extra_files={"fxgate_random_uint_callers.inc": caller_text})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_random_uint_range", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
