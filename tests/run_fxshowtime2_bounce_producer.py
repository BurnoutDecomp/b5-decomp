"""FX-SHOWTIME2 (crash parity 2026-09-24): VehicleManager::ProcessAftertouchEvents @0x82633DE8 posts the
game-state header's own event records -- the RecentBounceEventPayload fork is folded.

  The producer of world events 52 / 53 queued a function-local `RecentBounceEventPayload` (and a bare u8 for
  53) whose member names contradicted the record the consumer reads (GameStateModuleIO::JustBouncedEvent,
  DWARF BrnGameEvents.h:2782-2790), so nothing tied the two ends together. The fold posts JustBouncedEvent /
  JustAppliedExtraSpinEvent with GetRecentBounce writing straight into the record, exactly as the console
  (r4..r10 = record+0x00 / +0x04..+0x07 / +0x08 / +0x10, @0x82633E80..0x82633E98), plus static_asserts tying
  each register to its offset.

Numeric: tests/FxShowtime2BounceProducer.cpp runs the extracted production body against a stand-in car
and checks the posted 76 / 52 / 53 bytes and gates -- IT PASSES ON THE PRE-FOLD BODY TOO (the fold keeps
the bytes); that equality is the point. Wiring: the folded producer (the header records, the event ids, the
static_asserts, no local fork) -- this half fails on the pre-fold revision.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxshowtime2_bounce_producer.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, extract, compile_and_run, report, STRSTREAM_CPP

PRODUCER_CPP = "src/GameSource/Physics/VehicleManager/BrnVehicleManager_UpdateVehiclePhysics.cpp"
NUMERIC_CHECKS = 18

METHODS = ["    void VehicleManager::ProcessAftertouchEvents("]


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    source = tree.read(PRODUCER_CPP)
    try:
        body = squash(definition(source, "    void VehicleManager::ProcessAftertouchEvents("))
    except ValueError:
        body = ""
    yield ("the bounce record is the header's BrnGameState::GameStateModuleIO::JustBouncedEvent, filled by "
           "GetRecentBounce in place and posted as E_EVENT_JUST_BOUNCED",
           "BrnGameState::GameStateModuleIO::JustBouncedEventlBounce={};" in body
           and "lrCar.GetRecentBounce(&lBounce.miBounceChain,&lBounce.mbFromStationary,&lBounce.mbOnCar,"
               "&lBounce.mbBoostedBounce,&lBounce.mbGoodImpact," in body
           and "&lBounce.mContactPoint))" in body
           and "BrnGameState::GameStateModuleIO::E_EVENT_JUST_BOUNCED," in body)
    yield ("the extra-spin record is the header's JustAppliedExtraSpinEvent, posted as "
           "E_EVENT_JUST_APPLIED_EXTRA_SPIN",
           "BrnGameState::GameStateModuleIO::JustAppliedExtraSpinEventlSpin={};" in body
           and "BrnGameState::GameStateModuleIO::E_EVENT_JUST_APPLIED_EXTRA_SPIN," in body)
    yield ("no function-local event payload fork is left (RecentBounceEventPayload / a bare u8 53)",
           body != "" and "RecentBounceEventPayload" not in body and "lu8Payload" not in body)
    code = squash(source)
    needed = ["E_EVENT_JUST_BOUNCED==0x34", "sizeof(BrnGameState::GameStateModuleIO::JustBouncedEvent)==0x20",
              "JustBouncedEvent,miBounceChain)==0x00", "JustBouncedEvent,mbFromStationary)==0x04",
              "JustBouncedEvent,mbOnCar)==0x05", "JustBouncedEvent,mbBoostedBounce)==0x06",
              "JustBouncedEvent,mbGoodImpact)==0x07", "JustBouncedEvent,midImpactEntityId)==0x08",
              "JustBouncedEvent,mContactPoint)==0x10", "E_EVENT_JUST_APPLIED_EXTRA_SPIN==0x35",
              "sizeof(BrnGameState::GameStateModuleIO::JustAppliedExtraSpinEvent)==1"]
    yield ("static_asserts tie each GetRecentBounce register (r4..r10) and both li r5 / li r6 pairs to the "
           "header records",
           all("static_assert(" in code and item in code for item in needed))


def numeric(tree):
    methods, missing = extract(tree, PRODUCER_CPP, METHODS)
    if missing:
        print("NUMERIC: cannot build -- production bodies absent: " + ", ".join(missing))
        return None
    return compile_and_run(Path(__file__).with_name("FxShowtime2BounceProducer.cpp"), "producer_methods.inc",
                           "\n".join(methods) + "\n", "FxShowtime2BounceProducer",
                           extra_sources=[STRSTREAM_CPP])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxshowtime2_bounce_producer", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
