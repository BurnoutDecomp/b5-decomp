"""FX-TRAFFIC2 (crash parity 2026-09-24, reviewer C on 0e0a5781 item 3): TrafficEntityModule::ManageTriggers.

  @0x82747518 had no body and PreSceneUpdate never called it; the console calls it unconditionally
  at 0x8274AB20, after the four output producers and before the state switch. It removes the
  light-trigger boxes of every hull in mHullsToRemoveTriggersFor, adds those of every hull in
  mHullsToAddTriggersFor (owner-57 ids (hull << 8) | 0x39000000 | index, type-2
  InAddBoxTriggerEvents with |dimensions| and GetTransform()), posts nothing while the +0x729F0
  "Running Worst Case" debug byte is set, and clears both lists.

Wiring: PreSceneUpdate calls ManageTriggers(lpOutput) after the producers and before the switch.
Numeric: tests/FxTraffic2ManageTriggers.cpp compiled against the PRODUCTION ManageTriggers and
GetHull (working tree, or --rev <b5 rev>), writing into a real TriggerManagementInputInterface.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtraffic2_manage_triggers.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, body_or_empty, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
FIXTURE = "TriggerFixture"
NUMERIC_CHECKS = 9


def wiring(tree):
    pre_scene = body_or_empty(tree.read(MODULE_CPP), "void TrafficEntityModule::PreSceneUpdate(")
    stomp = pre_scene.find("GeneratePotentialLeapedAndStompedCarsOutput(lpInput, lpOutput);")
    manage = pre_scene.find("ManageTriggers(lpOutput);")
    switch = pre_scene.find("switch (meState)")
    return [("PreSceneUpdate calls ManageTriggers(lpOutput) after the output producers and before the state "
             "switch (0x8274AB20, unconditional)", 0 <= stomp < manage < switch)]


def numeric(tree):
    try:
        module = tree.read(MODULE_CPP)
        get_hull = definition(module, "const Hull* TrafficEntityModule::GetHull(u32 luIndex) const")
        manage = definition(module, "void TrafficEntityModule::ManageTriggers(")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    parts = ["namespace BrnTraffic {",
             get_hull.replace("TrafficEntityModule::", FIXTURE + "::", 1),
             manage.replace("TrafficEntityModule::", FIXTURE + "::", 1),
             "}"]
    return compile_and_run(Path(__file__).with_name("FxTraffic2ManageTriggers.cpp"), "manage_triggers.inc",
                           "\n".join(parts), "FxTraffic2ManageTriggers", extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtraffic2_manage_triggers", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
