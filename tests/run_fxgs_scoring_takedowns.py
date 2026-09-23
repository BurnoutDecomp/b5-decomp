"""FX-GS (crash parity 2026-09-23, G10-D8): the online-stunt takedown arm.

  ScoringSystem::UpdateTakedowns @0x8232AC88 -- the four-argument form and its trailing
      StuntModeScoringOnline::DealWithTakedown arm (0x8232AE40..0x8232AE7C);
  StuntModeScoringOnline::DealWithTakedown @0x82321890 -- the mbStuntModeActive (+0x28) gate;
  the ss+0x2620 member typed StuntModeScoringOnline (ctor vtable store 0x827E0A18), whose
      Construct / Prepare legs ScoringSystem::Construct / Prepare make (0x823380E0 / 0x8232A460),
      with the online Construct / Prepare / Destruct bodies read store for store, and whose
      vtable +0x10 reset (StuntModeScoringOnline::ClearData 0x82321968) the stunt-challenge
      Setup / End bodies reach.

Wiring: structural checks on the production sources. Numeric: tests/FxGsScoringTakedowns.cpp
compiled against the extracted production bodies (UpdateTakedowns re-homed onto a fixture; a
revision with the one-argument UpdateTakedowns is driven through the arguments it has).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgs_scoring_takedowns.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, code_only, compile_and_run, report

SS_H = "src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
UPDATE_A = "src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_UpdateA.cpp"
LIFECYCLE = "src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_Lifecycle.cpp"
WORLDTICK = "src/GameSource/GameState/ModeManager/BrnModeManager_WorldTick.cpp"
ONLINE_CPP = "src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoringOnline.cpp"
ONLINE_H = "src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoringOnline.h"
STUNT_H = "src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"
CHAIN_H = "src/GameSource/GameState/SharedIO/BrnChainableMultiplierInfo.h"
LEAF_H = "src/GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"
MODE_MANAGER = "src/GameSource/GameState/ModeManager/BrnModeManager.cpp"
NUMERIC_CHECKS = 26


def wiring(tree):
    header = code_only(tree.read(SS_H))
    yield ("D8 ss+0x2620 is `StuntModeScoringOnline mOnlineStuntModeScoring` (ctor vtable store 0x827E0A18)",
           re.search(r"StuntModeScoringOnline\s+mOnlineStuntModeScoring\s*;", header) is not None)
    yield ("D8 UpdateTakedowns takes (queue, player active race-car index, stunt-challenge flag)",
           re.search(r"void\s+UpdateTakedowns\s*\(\s*const\s+InputBuffer::TakedownEventQueue\s*\*\s*\w+\s*,"
                     r"\s*::EActiveRaceCarIndex\s+\w+\s*,\s*bool\s+\w+\s*\)", header) is not None)
    tick = code_only(tree.read(WORLDTICK))
    yield ("D8 PostWorldUpdateTakedownScoringBringUp passes mePlayerActiveRaceCarIndex (0x8234AC4C) and "
           "mbStuntChallengeActive (0x8234AC44)",
           re.search(r"UpdateTakedowns\s*\([^;]*,\s*mePlayerActiveRaceCarIndex\s*,\s*mbStuntChallengeActive\s*\)", tick, re.S)
           is not None)
    life = tree.read(LIFECYCLE)
    construct = body_or_empty(life, "void ScoringSystem::Construct(")
    offline = construct.find("mStuntModeScoring.Construct(lpAchievementManager)")
    online = construct.find("mOnlineStuntModeScoring.Construct(lpAchievementManager)")
    race = construct.find("mOnlineRaceModeScoring.Construct()")
    yield ("D8 ScoringSystem::Construct constructs the online scorer with the manager, between the offline "
           "scorer and the online race scorer (0x823380D8 < 0x823380F0 < 0x82338104)",
           0 <= offline < online < race)
    prepare = body_or_empty(life, "bool ScoringSystem::Prepare(")
    offline = prepare.find("mStuntModeScoring.Prepare()")
    online = prepare.find("mOnlineStuntModeScoring.Prepare()")
    yield ("D8 ScoringSystem::Prepare prepares the online scorer after the offline one (0x8232A45C < 0x8232A470)",
           0 <= offline < online)
    # The stunt-challenge resets dispatch vtable +0x10 of the ss+0x2620 object (0x8231EB4C / 0x82312134);
    # the online vtable 0x820CF9EC holds StuntModeScoringOnline::ClearData 0x82321968 there, so the
    # pointer the PC body calls through must be the online class (a base pointer binds the base ClearData).
    mode_manager = tree.read(MODE_MANAGER)
    for name, address in (("SetupStuntChallenge", "0x8231EB4C"), ("EndStuntChallenge", "0x82312134")):
        body = body_or_empty(mode_manager, "void ModeManager::" + name + "()")
        yield (f"D8 ModeManager::{name} resets through StuntModeScoringOnline::ClearData "
               f"(vtable +0x10 @{address} -> 0x82321968)",
               re.search(r"StuntModeScoringOnline\s*\*\s*lpStuntModeScoring\s*=", body) is not None
               and "lpStuntModeScoring->ClearData()" in body)


def numeric(tree):
    update_a = tree.read(UPDATE_A)
    online = tree.read(ONLINE_CPP)
    try:
        update = definition(update_a, "    void ScoringSystem::UpdateTakedowns(")
        deal = definition(online, "void StuntModeScoringOnline::DealWithTakedown()")
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    four_args = re.search(r"UpdateTakedowns\s*\([^)]*,\s*::EActiveRaceCarIndex\s+\w+\s*,\s*bool\s+\w+\s*\)", update)
    if four_args:
        declaration = ("void UpdateTakedowns(const InputBuffer::TakedownEventQueue*, ::EActiveRaceCarIndex, bool)")
        call = "(lrScoring).UpdateTakedowns((lpQueue), (lePlayer), (lbChallenge))"
    else:
        print("NUMERIC: this revision's UpdateTakedowns takes the queue only (the dropped arguments are the defect)")
        declaration = "void UpdateTakedowns(const InputBuffer::TakedownEventQueue*)"
        call = "((void)(lePlayer), (void)(lbChallenge), (lrScoring).UpdateTakedowns((lpQueue)))"
    update = update.replace("ScoringSystem::UpdateTakedowns", "ScoringFixture::UpdateTakedowns")
    constants = "\n".join(re.findall(r"^    const (?:f32|EStuntType)\s+K[EF]_[A-Z_]+\s*=[^;]+;", online, re.M))
    lifecycle = []
    for signature in ("void StuntModeScoringOnline::Construct(", "void StuntModeScoringOnline::Destruct()",
                      "bool StuntModeScoringOnline::Prepare()"):
        try:
            lifecycle.append(definition(online, signature))
        except ValueError:
            print("NUMERIC: absent in this revision: " + signature)
            return None
    if "StuntModeScoringOnline::Construct()" in lifecycle[0]:
        # The pre-fix Construct took no argument; drive it through the one it has.
        lifecycle[0] = lifecycle[0].replace("StuntModeScoringOnline::Construct()",
                                            "StuntModeScoringOnline::Construct(AchievementManager*)")
    inc = ("namespace BrnGameState {\nnamespace {\n" + constants + "\n}\n" + update + "\n" + deal + "\n"
           + "\n".join(lifecycle) + "\n}\n")
    config = ("#define FXGS_UPDATE_TAKEDOWNS_DECLARATION " + declaration + "\n"
              "#define FXGS_CALL_UPDATE_TAKEDOWNS(lrScoring, lpQueue, lePlayer, lbChallenge) " + call + "\n")
    shadow = {ONLINE_H: tree.read(ONLINE_H), STUNT_H: tree.read(STUNT_H), LEAF_H: tree.read(LEAF_H)}
    chain = tree.read(CHAIN_H)
    if chain:
        shadow[CHAIN_H] = chain
    if "Construct(AchievementManager* lpAchievementManager)" not in shadow[ONLINE_H]:
        shadow[ONLINE_H] = shadow[ONLINE_H].replace("void Construct();", "void Construct(AchievementManager*);", 1)
    return compile_and_run(Path(__file__).with_name("FxGsScoringTakedowns.cpp"), "scoring_takedowns_methods.inc",
                           inc, "FxGsScoringTakedowns", shadow=shadow,
                           extra_files={"scoring_takedowns_config.inc": config})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgs_scoring_takedowns", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
