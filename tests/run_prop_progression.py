"""L4 (owner's list 2026-09-27/28, "The already broken smashes/billboards are not visually broken or fallen to the
ground"): the profile's hit-prop bits never reached the prop world.

The console hands them over in a handshake inside GameStateModule::ProcessGameEvents @0x823A0A18. OnProfileLoaded
@0x82397310 -- case 8 at the boot (the MemoryCard exit), case 109 on an in-game load (E_EVENT_PROGRESSION_PROFILE_LOADED,
bridged from GUI event 352 by BridgeGuiToGameState @0x823DDB78) -- posts action 194 (E_ACTION_LOAD_PROFILE, size 1);
the world drops its props and asks back with game event 112 (E_EVENT_REQUEST_PROP_PROGRESSION); case 112 sets
mbPropSystemNeedsProgression (`*(this+292288) = 1`), and the dispatcher's tail (LABEL_648) posts action 199
(E_ACTION_PROP_SMASH_PROGRESSION, size 4) carrying gsm+107224 == &profile.mabHitPropBitArray and clears the flag. The
world copies those 300000 bits into PropZoneManager::maPreviouslyHitProps, and LoadProp leaves a hit don't-respawn
prop out and swaps a hit respawn-changed prop for its broken alternative. On the PC nothing posted 194 or 199 and no
arm consumed 109 or 112, so after every boot the smash gates and billboards the save records as broken streamed in
intact.

  1. WIRING -- the four ids are the console's (109 / 112 / 194 / 199) and the world bridge reads the same two action
     ids; the dispatcher calls the arm LAST (the tail runs after the console's whole walk); Profile::GetHitProps is
     the profile's own mabHitPropBitArray.
  2. NUMERIC -- tests/PropProgression.cpp compiles the PRODUCTION arm and action records onto a fixture and checks
     what the arm does for 109 (OnProfileLoaded), for 112, for a flag raised earlier, and for neither.
     OnProfileLoaded itself (its 194 among its 18 statements) is run_profile_delivery.py's.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_prop_progression.py [--rev <b5 rev>]
                                                                                             [--root <shadow root>]
(--root reads any file present under <root>/... in place of the working tree's.)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

EVENTS_H = "src/GameSource/GameState/BrnGameEvents.h"
ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
PROFILE_H = "src/GameSource/GameState/Progression/BrnProfile.h"
DISPATCH_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
BRIDGE_CPP = "src/GameSource/World/Bridges/WorldBridgeInputToEntityModules.cpp"
ARM_SIGNATURE = "void GameStateModule::ProcessGameEventsPropProgressionBringUp("
DISPATCHER_SIGNATURE = "void GameStateModule::PreWorldUpdateStuntBringUp("
RECORDS = ("struct LoadProfileAction ", "struct PropSmashReportAction ")
EVENT_IDS = {"E_EVENT_PROGRESSION_PROFILE_LOADED": 109, "E_EVENT_REQUEST_PROP_PROGRESSION": 112}
ACTION_IDS = {"E_ACTION_LOAD_PROFILE": 194, "E_ACTION_PROP_SMASH_PROGRESSION": 199}

NUMERIC_CHECKS = 9   # see PropProgression.cpp


class RootTree(Tree):
    """The working tree (or --rev) with an optional shadow root whose files take precedence."""

    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = Path(root) if root else None

    def read(self, relative):
        if self.root is not None and (self.root / relative).exists():
            return (self.root / relative).read_text(encoding="utf-8-sig")
        try:
            return super().read(relative)
        except FileNotFoundError:
            return ""


def normalised(tree, relative):
    return tree.read(relative).replace("\r\n", "\n")


def enumerators(text, names):
    """{name: (value, line)} for each `NAME = value,` enumerator found in `text` (comments stripped)."""
    found = {}
    for name in names:
        match = re.search(r"^\s*" + re.escape(name) + r"\s*=\s*(\d+)\s*,", code_only(text), re.M)
        if match:
            found[name] = (int(match.group(1)), f"{name} = {match.group(1)},")
    return found


def wiring(tree):
    events = enumerators(normalised(tree, EVENTS_H), EVENT_IDS)
    actions = enumerators(normalised(tree, ACTIONS_H), ACTION_IDS)
    yield ("the ids are the console's: game events 109 (case 109 -> OnProfileLoaded) and 112 (case 112), actions "
           "194 (OnProfileLoaded's AddEvent(a3, v14, 194, 1)) and 199 (the tail's AddEvent(v30, &v341, 199, 4))",
           all(events.get(n, (None,))[0] == v for n, v in EVENT_IDS.items())
           and all(actions.get(n, (None,))[0] == v for n, v in ACTION_IDS.items()))
    bridge = code_only(normalised(tree, BRIDGE_CPP))
    yield ("the world bridge consumes the same two action ids (BridgeInputToEntityModules @0x827ADF88: 194 -> "
           "SendingPropProgression, 199 -> SetHitPropsBitArray)",
           re.search(r"KI_GAME_ACTION_SEND_PROP_PROGRESSION\s*=\s*194\b", bridge) is not None
           and re.search(r"KI_GAME_ACTION_PROP_SMASH_REPORT\s*=\s*199\b", bridge) is not None)
    dispatcher_source = normalised(tree, DISPATCH_CPP)
    try:
        dispatcher = code_only(definition(dispatcher_source, DISPATCHER_SIGNATURE))
    except ValueError:
        dispatcher = ""
    calls = re.findall(r"\b(ProcessGameEvents\w*BringUp)\s*\(\s*&lGameEventQueue", dispatcher)
    yield ("the dispatcher calls the arm over the merged queue, LAST of its ProcessGameEvents arms (the console's tail "
           "runs after the whole walk)",
           len(calls) > 1 and calls[-1] == "ProcessGameEventsPropProgressionBringUp"
           and calls.count("ProcessGameEventsPropProgressionBringUp") == 1)
    profile = code_only(normalised(tree, PROFILE_H))
    yield ("Profile::GetHitProps (DWARF BrnProfile.h:1030) is the profile's own mabHitPropBitArray (gsm+107224)",
           re.search(r"GetHitProps\s*\(\s*\)\s*const\s*\{\s*return\s+mabHitPropBitArray\s*;\s*\}", profile) is not None)


def numeric(tree):
    events = enumerators(normalised(tree, EVENTS_H), EVENT_IDS)
    actions = enumerators(normalised(tree, ACTIONS_H), ACTION_IDS)
    missing = [n for n in EVENT_IDS if n not in events] + [n for n in ACTION_IDS if n not in actions]
    actions_source = normalised(tree, ACTIONS_H)
    records = []
    for signature in RECORDS:
        try:
            records.append(definition(actions_source, signature) + ";")
        except ValueError:
            missing.append(signature.strip())
    try:
        arm = definition(normalised(tree, DISPATCH_CPP), ARM_SIGNATURE)
    except ValueError:
        missing.append(ARM_SIGNATURE)
        arm = ""
    if missing:
        print("NUMERIC: cannot build -- no " + ", ".join(missing))
        return None
    return compile_and_run(Path(__file__).with_name("PropProgression.cpp"), "propprog_body.inc", arm + "\n",
                           "PropProgression",
                           extra_files={"propprog_event_ids.inc": "\n".join(l for _, l in events.values()) + "\n",
                                        "propprog_action_ids.inc": "\n".join(l for _, l in actions.values()) + "\n",
                                        "propprog_records.inc": "\n".join(records) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_prop_progression", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
