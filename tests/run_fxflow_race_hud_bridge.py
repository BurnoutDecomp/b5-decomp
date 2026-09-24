"""FX-FLOW (crash parity 2026-09-24, G11-D1 remainder, consumer half): the race-mode HUD message arms of
BrnGameModule::TranslateGameActionsToGuiEvents.

  HUDMessageLogic's race arm posts actions 242 (took lead), 243 (took last), 244 (distance to finish),
  245 (leader split), 247 (a rival finished), 248 (a rival reached a checkpoint), 249 (the player's
  checkpoint) and 250 (a car crashed). The console translator @0x823E9CE0 turns 242 -> GUI 484,
  245 -> 420, 246 -> 421, 247 -> 423, 248 -> 422 and 250 -> 482, and drops 243 / 244 / 249 (its
  default list). The PC translator had none of these arms, although DetectCrashes / DetectOnlineCrashes
  already post 250 live; the ids 242..248 and their records had no PC home.

Numeric: tests/FxFlowRaceHudBridge.cpp runs the extracted production arms (+ the TU-local 421 wire
record) against raw action records laid out at the X360 producers' offsets, with a recording
PushGuiEvent -- empty stand-ins where a revision lacks an arm.
Wiring: EGameActionType keeps one enumerator per value; GuiInEventRivalProgress has one definition.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_race_hud_bridge.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, REPO, definition, code_only, compile_and_run, report, STRSTREAM_CPP

BRIDGE_CPP = "src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp"
ACTIONS_H = "src/GameSource/GameState/BrnGameActions.h"
GUI_HEADERS = ["src/GameSource/Gui/BrnGuiEventTypeDefs.h", "src/GameSource/Gui/BrnGuiDemangledEventTypes.h"]
ARMS = [
    "E_ACTION_HUD_MESSAGE_TOOK_LEAD",
    "E_ACTION_HUD_MESSAGE_LEADING",
    "E_ACTION_HUD_MESSAGE_NECK_AND_NECK",
    "E_ACTION_HUD_MESSAGE_X_FINISHES",
    "E_ACTION_HUD_MESSAGE_X_REACHES_CHECKPOINT",
    "E_ACTION_HUD_MESSAGE_X_CRASHES",
]
NUMERIC_CHECKS = 11


def mask(text):
    """Blank comments and string literals (same length) so brace / label scans see only code."""
    def blank(match):
        return re.sub(r"[^\n]", " ", match.group(0))
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', blank, text)


def arm(body, label_pattern):
    """The verbatim text of `case <label>:` up to the next label of the same switch (or None)."""
    masked = mask(body)
    label = re.search(r"\bcase\s+" + label_pattern + r"\s*:", masked)
    if not label:
        return None
    depth = 0
    position = label.end()
    token = re.compile(r"[{}]|\bcase\b|\bdefault\s*:")
    while True:
        match = token.search(masked, position)
        if match is None:
            return None
        text = match.group(0)
        if text == "{":
            depth += 1
        elif text == "}":
            depth -= 1
            if depth < 0:
                return body[label.start():match.start()]
        elif depth == 0:
            return body[label.start():match.start()]
        position = match.end()


def wiring(tree):
    code = code_only(tree.read(ACTIONS_H))
    start = code.find("enum EGameActionType")
    values = {}
    if start >= 0:
        for match in re.finditer(r"^\s*(E_ACTION_\w+)\s*=\s*(\d+)", code[start:code.index("};", start)], re.M):
            values.setdefault(int(match.group(2)), []).append(match.group(1))
    duplicates = {value: names for value, names in values.items() if len(names) > 1}
    if duplicates:
        print("  duplicate EGameActionType values: " + ", ".join(
            "%d = %s" % (value, " / ".join(names)) for value, names in sorted(duplicates.items())))
    yield ("EGameActionType keeps one enumerator per value (242..248 land on free X360 ids)",
           start >= 0 and not duplicates)
    count = 0
    for path in GUI_HEADERS:
        count += len(re.findall(r"\bstruct\s+GuiInEventRivalProgress\s*(?::[^{;]*)?\{", code_only(tree.read(path))))
    yield ("GuiInEventRivalProgress (GUI 422) is defined exactly once across the GUI event headers", count == 1)


def numeric(tree):
    bridge = tree.read(BRIDGE_CPP)
    translator = definition(bridge, "void BrnGameModule::TranslateGameActionsToGuiEvents(")
    texts = []
    missing = []
    for name in ARMS:
        text = arm(translator, r"BrnGameState::GameStateModuleIO::" + name)
        if text is None:
            missing.append(name)
        else:
            texts.append(text)
    wire = re.search(r"struct NeckAndNeckEventWire421\s*\{[\s\S]*?\};", code_only(bridge))
    if missing:
        print("NUMERIC: arms absent in this revision (empty stand-ins): " + ", ".join(missing))
    return compile_and_run(Path(__file__).with_name("FxFlowRaceHudBridge.cpp"), "race_arms.inc",
                           "\n".join(texts) + "\n", "FxFlowRaceHudBridge",
                           extra_sources=[STRSTREAM_CPP],
                           extra_files={"wire421.inc": (wire.group(0) if wire else "") + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxflow_race_hud_bridge", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
