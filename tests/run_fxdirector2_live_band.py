"""FX-DIRECTOR2 (crash parity 2026-09-25): ArbStateCrashing::ApplySlomoAndShake reads the tracker's LIVE crash band.

ApplySlomoAndShake @0x8224F8D8 decides, every active frame of the crashing state, whether the crash slow motion
and the impact shake run. All four of its energy-band tests load the band fresh off the shared info's player
tracker -- `lwz r11, 0x3C(r30)` (ArbStateSharedInfo::mpPlayerTracker) then `lwz r11, 0x298(r11)`
(VehicleTracker::meCrashType) -- at 0x8224F914/18 (== NORMAL, lbDontSetRealTime), 0x8224F950/54 (!= NORMAL,
suppress slomo), 0x8224F98C/90 (== HIGH, suppress shake) and 0x8224F9BC/C0 (== LOW, no slomo). The PC body read the
state's own Prepare latch (meCrashType, +0x3AC -- stored by Prepare @0x8226563C/40) at all four; the console reads
that latch in exactly one place, ProcessPossibleStateChanges (`lwz 0x3AC(r31)` @0x8224F76C). The two differ once the
crash is over while the state is still live (the tracker drops to NOT_CRASHING) and on a second crash inside one
crashing state -- i.e. on the slow-motion decision, which reaches the SIM timestep through MainDirector::Update's
time-dilation publish (0x82275128..0x82275148).

The body is a four-read predicate over members of an un-mountable-in-isolation state, so the checks are structural,
on comment-stripped source (they fail on the old body).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector2_live_band.py [--rev <b5 rev>]
"""
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, report

CRASHING_CPP = "src/GameSource/Director/Arbitrator/States/BrnArbStateCrashing.cpp"
LIVE = "lrSharedInfo.mpPlayerTracker->GetCrashType()"


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def body(source, signature):
    try:
        return squash(definition(source, signature))
    except ValueError:
        return ""


def wiring(tree):
    source = tree.read(CRASHING_CPP)
    apply_ = body(source, "void ArbStateCrashing::ApplySlomoAndShake(")
    changes = body(source, "void ArbStateCrashing::ProcessPossibleStateChanges(")
    prepare = body(source, "bool ArbStateCrashing::Prepare(")

    yield ("ApplySlomoAndShake loads the tracker's live band at all four tests (0x8224F918 / 954 / 990 / 9C0)",
           apply_.count(LIVE) == 4)
    yield ("... and never the state's Prepare latch (meCrashType, +0x3AC)",
           apply_ != "" and "meCrashType" not in apply_)
    yield ("the four tests, in the console's order: == NORMAL (lbDontSetRealTime), != NORMAL (suppress slomo), "
           "== HIGH (suppress shake), == LOW (no slomo)",
           re.search(re.escape(LIVE) + r"==VehicleTracker::E_CRASH_NORMAL.*"
                     + re.escape(LIVE) + r"!=VehicleTracker::E_CRASH_NORMAL.*"
                     + re.escape(LIVE) + r"==VehicleTracker::E_CRASH_HIGH_ENERGY.*"
                     + re.escape(LIVE) + r"==VehicleTracker::E_CRASH_LOW_ENERGY", apply_) is not None)
    yield ("ProcessPossibleStateChanges still reads the latch (`lwz 0x3AC(r31)` @0x8224F76C)",
           "meCrashType==VehicleTracker::E_CRASH_LOW_ENERGY" in changes)
    yield ("Prepare still latches the tracker's band (`lwz 0x298` / `stw 0x3AC` @0x8226563C/40)",
           "meCrashType=" + LIVE + ";" in prepare)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector2_live_band", list(wiring(tree)), (0, 0), 0)


if __name__ == "__main__":
    sys.exit(main())
