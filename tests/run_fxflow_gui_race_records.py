"""FX-FLOW (crash parity 2026-09-24, FOLLOWUPS item 14): the race-finish GUI records and their enum.

  BrnGui::EFinishType carried one invented enumerator (E_FINISH_TYPE_NONE = 0) instead of the DWARF
  set (BrnGuiEventTypeDefs.h:893: 1ST..8TH = 0..7, TIMED_OUT 8, WON 9, LOST 10, COUNT 11 -- the
  values X360 ModeManager::FinishCurrentMode @0x8234B978 hands AddFinishedRaceEvent), and the GUI
  records 371 GuiOvertakeEvent / 372 GuiFinishRaceEvent were opaque u8[8] placeholders in
  BrnGuiDemangledEventTypes.h, so only the translator's TU-local wire records knew the layout the
  console posts (TranslateGuiInterfaceToGuiEvents @0x823E1D90: slot @+0, position byte / finish
  type @+4; AddGuiEvent<T> @0x823D9DB0 / @0x823D9E68, 8 bytes).

Numeric: tests/FxFlowGuiRaceRecords.cpp compiles the revision's enum and the two BrnGui records
(wherever that revision defines them) with the translator's wire records from BrnGameModule.cpp and
checks the enumerator values, the DWARF field offsets / sizes / ids and that a posted wire record
reads back through the BrnGui type.
Wiring: one definition per record across the GUI headers (no ODR fork); the translator's wire
records are pinned byte-for-byte to the BrnGui homes.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_gui_race_records.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, REPO, definition, code_only, compile_and_run, report

EVENTS_H = "src/GameSource/GameState/SharedIO/BrnGameStateToGuiEvents.h"
GAME_CPP = "src/GameSource/Game/BrnGameModule.cpp"
GUI_HEADERS = [
    "src/GameSource/Gui/BrnGuiEventTypeDefs.h",
    "src/GameSource/Gui/BrnGuiDemangledEventTypes.h",
]
RECORDS = ["GuiOvertakeEvent", "GuiFinishRaceEvent"]
WIRE_RECORDS = ["struct GuiOvertakeEventWire371", "struct GuiFinishRaceEventWire372"]
ENUMERATORS = [
    ("E_FINISH_TYPE_1ST", 0), ("E_FINISH_TYPE_2ND", 1), ("E_FINISH_TYPE_3RD", 2), ("E_FINISH_TYPE_4TH", 3),
    ("E_FINISH_TYPE_5TH", 4), ("E_FINISH_TYPE_6TH", 5), ("E_FINISH_TYPE_7TH", 6), ("E_FINISH_TYPE_8TH", 7),
    ("E_FINISH_TYPE_TIMED_OUT", 8), ("E_FINISH_TYPE_WON", 9), ("E_FINISH_TYPE_LOST", 10),
    ("E_FINISH_TYPE_COUNT", 11),
]
NUMERIC_CHECKS = len(ENUMERATORS) + 10


def record_definitions(text, name):
    """Every definition (not a forward declaration) of `struct <name>` in the code of `text`."""
    code = code_only(text)
    found = []
    for match in re.finditer(r"\bstruct\s+" + name + r"\s*(?::[^{;]*)?\{", code):
        found.append(definition(code, code[match.start():match.end()]))
    return found


def gui_headers(tree):
    """The revision's GUI event headers (the two named ones + everything under Gui/Events)."""
    paths = list(GUI_HEADERS)
    if tree.rev:
        listing = subprocess.run(["git", "-C", str(REPO), "ls-tree", "-r", "--name-only", tree.rev,
                                  "src/GameSource/Gui/Events/"], capture_output=True, text=True).stdout
        paths += [line for line in listing.splitlines() if line.endswith(".h")]
    else:
        paths += [str(p.relative_to(REPO)).replace("\\", "/")
                  for p in sorted((REPO / "src/GameSource/Gui/Events").glob("*.h"))]
    return {path: tree.read(path) for path in paths}


def wiring(tree):
    headers = gui_headers(tree)
    counts = {name: sum(len(record_definitions(text, name)) for text in headers.values()) for name in RECORDS}
    print("  definitions: " + ", ".join("%s x%d" % item for item in counts.items()))
    yield ("GuiOvertakeEvent and GuiFinishRaceEvent are each defined exactly once across the GUI event "
           "headers (the placeholder is gone, no ODR fork)", all(count == 1 for count in counts.values()))
    game = re.sub(r"\s+", "", code_only(tree.read(GAME_CPP)))
    pinned = ("sizeof(GuiOvertakeEventWire371)==sizeof(BrnGui::GuiOvertakeEvent)" in game
              and "sizeof(GuiFinishRaceEventWire372)==sizeof(BrnGui::GuiFinishRaceEvent)" in game
              and "offsetof(BrnGui::GuiOvertakeEvent,muNewPosition)" in game
              and "offsetof(BrnGui::GuiFinishRaceEvent,meFinishType)" in game)
    yield ("the translator's 371 / 372 wire records are asserted byte-for-byte against the BrnGui homes",
           pinned)


def numeric(tree):
    events = code_only(tree.read(EVENTS_H))
    match = re.search(r"enum\s+EFinishType\b[^{;]*\{[^}]*\}\s*;", events)
    if match is None:
        print("NUMERIC: cannot build -- BrnGui::EFinishType not found in " + EVENTS_H)
        return None
    enum_text = match.group(0)
    checks = []
    for name, value in ENUMERATORS:
        label = "BrnGui::%s == %d (DWARF BrnGuiEventTypeDefs.h:893)" % (name, value)
        if re.search(r"\b" + name + r"\b", enum_text):
            checks.append('Check(static_cast<int>(BrnGui::%s) == %d, "%s");' % (name, value, label))
        else:
            checks.append('Check(false, "%s -- enumerator missing");' % label)
    headers = gui_headers(tree)
    records = []
    for name in RECORDS:
        found = [text for header in headers.values() for text in record_definitions(header, name)]
        if not found:
            print("NUMERIC: cannot build -- no definition of BrnGui::" + name)
            return None
        records.append(found[0] + ";")
    game = tree.read(GAME_CPP)
    try:
        wires = [definition(game, signature) + ";" for signature in WIRE_RECORDS]
    except ValueError:
        print("NUMERIC: cannot build -- the translator's wire records are absent")
        return None
    return compile_and_run(Path(__file__).with_name("FxFlowGuiRaceRecords.cpp"), "finish_type.inc",
                           enum_text + "\n", "FxFlowGuiRaceRecords",
                           extra_files={"gui_records.inc": "\n".join(records) + "\n",
                                        "wire_records.inc": "\n".join(wires) + "\n",
                                        "enum_checks.inc": "\n".join(checks) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxflow_gui_race_records", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
