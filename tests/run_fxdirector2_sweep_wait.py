"""FX-DIRECTOR2 (crash parity 2026-09-25): the crash sweep's opt-in BRN_SWEEP_WAIT_ROAMING gate (harness, NOT X360).

The sweep arms on the junkyard exit, where ArbStateCarSelect plays its 3 s OUTRO take and owns the camera until it
ends (ArbStateCarSelect::Update @0x8226F5D0 case 6 leaves only on HasFinishedOrFailed). A shot fired at the arm
crashes inside that outro, so the crash camera and the hard stop's time scale only start when it ends (measured:
scratch/flow_run/fxd2trace_h225_s80_r1 -- crash f604, `container current state -> 2 (ArbStateCrashing)` f687).
The gate holds shot 0 until the director's arbitrator is back in ArbStateRoaming, read through a harness-only flag the
container's single state writer publishes. Structural checks on comment-stripped source (there is no console body);
each fails on the pre-gate tree.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector2_sweep_wait.py [--rev <b5 rev>]
"""
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, report

SWEEP_CPP = "src/GameSource/World/BrnPlaceOnTrackManager.cpp"
CONTAINER_CPP = "src/GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.cpp"
HARNESS_H = "src/GameSource/Director/BrnDirectorHarness.h"


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def body(source, signature):
    try:
        return squash(definition(source, signature))
    except ValueError:
        return ""


def wiring(tree):
    harness = squash(tree.read(HARNESS_H))
    yield ("BrnDirectorHarness.h declares BrnDirector::Harness::gbArbitratorInRoaming",
           "namespaceBrnDirector{namespaceHarness{externboolgbArbitratorInRoaming;" in harness)

    container = tree.read(CONTAINER_CPP)
    setter = body(container, "void ArbitratorStateContainer::SetCurrentState(")
    yield ("ArbitratorStateContainer::SetCurrentState -- the single writer of the current state -- publishes it "
           "(true exactly for E_STATE_ROAMING), after the console's own store",
           setter.endswith("mpCurrentState=GetState(leState);"
                           "Harness::gbArbitratorInRoaming=(leState==E_STATE_ROAMING);}"))
    yield ("the flag is defined once, false at start", "boolgbArbitratorInRoaming=false;" in squash(container))

    sweep = body(tree.read(SWEEP_CPP), "void PlaceOnTrackManager::ArmCrashSweepBringUp()")
    at_arm = sweep.find("return;}")   # the arm-distance early-out ...
    gate = sweep.find('std::getenv("BRN_SWEEP_WAIT_ROAMING")')
    running = sweep.find("seStage=E_SWEEP_RUNNING;")
    yield ("the sweep reads BRN_SWEEP_WAIT_ROAMING once, default OFF (unset / empty / '0' keep the old recipe)",
           gate >= 0 and "(lpcWait!=0&&lpcWait[0]!='\\0'&&lpcWait[0]!='0')?1:0" in sweep)
    yield ("... and holds shot 0 while the director is not in ArbStateRoaming, AFTER the arm distance and BEFORE "
           "the stage goes RUNNING",
           0 <= at_arm < gate < running
           and "if(siWaitRoaming==1&&!BrnDirector::Harness::gbArbitratorInRoaming)" in sweep)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector2_sweep_wait", list(wiring(tree)), (0, 0), 0)


if __name__ == "__main__":
    sys.exit(main())
