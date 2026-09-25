"""crash parity FX-NETCRASH (2026-09-25), item 3b: UpdateTimers' fused accumulate.

TrafficEntityModule::UpdateTimers @0x82715858 accumulates the time since the last traffic decision frame with
`fmadds f0, f0, f13, f12` at 0x827159E0 -- the sim timer block's mfTimeStepMultiplier (+0x20) * mfBaseTimeStep
(+0x1C) + mfSimTimeSinceLastDecision, ONE rounding. The PC added the already-rounded GetCurrentTimeStep(), which
is 1 ulp off whenever the product is inexact (any multiplier that is not a power of two).

NUMERIC: the production UpdateTimers, IsDecisionFrame and the file-local constants from BrnTrafficEntityModule.cpp
plus TimerStatusInterface::IsSimTimerFrequency50Hz from CgsTimerStatusInterface.cpp, compiled into
FxNetcrashUpdateTimers.cpp.
WIRING: the accumulate is one std::fma of the timer's multiplier and base step.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnetcrash_update_timers.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

MODULE_CPP = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
TIMER_CPP = "src/GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.cpp"
TIMER_H = "src/GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
UPDATE_TIMERS = "void TrafficEntityModule::UpdateTimers("
CONSTANTS = ("KF_SIM_TIMESTEP_SQ_SCALE", "KF_ONLINE_SIM_TIME_SINCE_DECISION", "KU_FRAMES_PER_DECISION_50HZ",
             "KU_FRAMES_PER_DECISION_60HZ")
NUMERIC_CHECKS = 11


def numeric(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    timers = tree.read(TIMER_CPP).replace("\r\n", "\n")
    try:
        parts = []
        for name in CONSTANTS:
            match = re.search(r"const (?:f32|u32) " + name + r"\s*=[^;]+;", module)
            if match is None:
                raise ValueError("constant " + name)
            parts.append(match[0])
        parts.append(definition(module, "bool TrafficEntityModule::IsDecisionFrame()"))
        parts.append(definition(module, UPDATE_TIMERS))
        rate = definition(timers, "bool\nCgsSystem::TimerStatusInterface::IsSimTimerFrequency50Hz() const")
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    block = "namespace\n{\n" + "\n".join(parts[:len(CONSTANTS)]) + "\n}\n" + "\n".join(parts[len(CONSTANTS):])
    return compile_and_run(Path(__file__).with_name("FxNetcrashUpdateTimers.cpp"), "update_timers.inc", block,
                           "FxNetcrashUpdateTimers", shadow={TIMER_H: tree.read(TIMER_H)},
                           extra_sources=[STRSTREAM_CPP], extra_files={"timer_status.inc": rate + "\n"})


def wiring(tree):
    module = tree.read(MODULE_CPP).replace("\r\n", "\n")
    try:
        body = code_only(definition(module, UPDATE_TIMERS))
    except ValueError:
        body = ""
    fused = re.search(r"mfSimTimeSinceLastDecision\s*=\s*std::fma\(\s*lpSimTimer->GetTimeStepMultiplier\(\),\s*"
                      r"lpSimTimer->GetBaseTimeStep\(\),\s*mfSimTimeSinceLastDecision\s*\);", body)
    return [
        ("UpdateTimers accumulates with ONE std::fma(multiplier, base, time) -- fmadds 0x827159E0",
         fused is not None and "+= lpSimTimer->GetCurrentTimeStep()" not in body),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxnetcrash_update_timers", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
