"""FX-FLOW (crash parity 2026-09-24, G13-X5 remainder): the GUI side of the free-burn rival shutdown.

  TakedownManager posts action 120 (ShutdownAction) when the player takes a free-burn rival down;
  TranslateGameActionsToGuiEvents case 120 (0x823ED88C) turns it into GUI 373 (GuiShutdownEvent, the
  8-byte victim car id); GuiCache::RecEvent stores it as mShutdownCarID (0x8250FFA8) and raises
  mbCarUnlockPending on 374 (0x8250FFC4); InGame's case 373 then enters OfflineRivalShutdown, which
  reads the id back. On PC the two cache arms, their GuiModule forward and the 120 arm were missing
  (the 120 arm was held back until the cache writer existed).

Numeric: tests/FxFlowRivalShutdownGui.cpp runs the extracted GuiCache arms 373 / 374 and the
extracted translator arm 120 (+ its TU-local wire record) -- empty stand-ins where a revision lacks
them.
Wiring: GuiModule::DispatchInboundGuiEvents forwards 373 and 374 to GuiCache::RecEvent.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_rival_shutdown_gui.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

CACHE_CPP = "src/GameSource/Gui/BrnGuiCache.cpp"
MODULE_CPP = "src/GameSource/Gui/BrnGuiModule.cpp"
BRIDGE_CPP = "src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp"
NUMERIC_CHECKS = 6


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
    body = mask(definition(tree.read(MODULE_CPP), "void GuiModule::DispatchInboundGuiEvents("))
    ok = True
    for event_id in ("373", "374"):
        label = re.search(r"\bcase\s+" + event_id + r"\s*:", body)
        if label is None:
            ok = False
            continue
        rest = body[label.end():]
        stop = rest.find("break;")
        ok = ok and stop >= 0 and "mGuiCache.RecEvent(lpEvent, liId);" in rest[:stop]
    yield ("GuiModule::DispatchInboundGuiEvents forwards 373 and 374 to GuiCache::RecEvent (the console "
           "cache sees every module-input event before the flow does)", ok)


def numeric(tree):
    stood_in = []
    cache_body = definition(tree.read(CACHE_CPP), "void GuiCache::RecEvent(")
    cache_arms = []
    for event_id in ("373", "374"):
        text = arm(cache_body, event_id)
        if text is None:
            stood_in.append("GuiCache case " + event_id)
        else:
            cache_arms.append(text)
    bridge = tree.read(BRIDGE_CPP)
    translator = definition(bridge, "void BrnGameModule::TranslateGameActionsToGuiEvents(")
    translator_arm = arm(translator, r"BrnGameState::GameStateModuleIO::E_ACTION_SHUTDOWN")
    wire = re.search(r"struct ShutdownEventWire373\s*\{[\s\S]*?\};", code_only(bridge))
    if translator_arm is None or wire is None:
        stood_in.append("translator case 120")
        translator_arm = ""
        wire_text = ""
    else:
        wire_text = wire.group(0)
    if stood_in:
        print("NUMERIC: bodies absent in this revision (empty stand-ins): " + ", ".join(stood_in))
    return compile_and_run(Path(__file__).with_name("FxFlowRivalShutdownGui.cpp"), "cache_arms.inc",
                           "\n".join(cache_arms) + "\n", "FxFlowRivalShutdownGui",
                           extra_sources=[STRSTREAM_CPP],
                           extra_files={"translator_arm.inc": translator_arm + "\n",
                                        "wire373.inc": wire_text + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxflow_rival_shutdown_gui", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
