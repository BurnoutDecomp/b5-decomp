"""FX-TAILS-B (crash parity 2026-09-24, item 7): CgsSound::Logic::State::Detach @0x826C4C20.

ARTIST: `lwz 0x38 ; cmpwi 5 ; beq` -- only an E_UPDATE_ATTACHED state detaches (anything else returns
false and stores nothing); then mpvAttachment = 0, mbIsAttached = false and meUpdateState.Set(DETATCHING)
(the old state goes to the history word +0x3C). VehicleState::Detach @0x826C9FF8 inlines the same body.
CollisionState's vtable off_820B25D0 holds 0x826C4C20 at +0x18: its override is identical-code-folded
with the base and must not touch meLifetime.

The PC detached from any update state, kept the attachment, never wrote the history word, and its
CollisionState::Detach wrote E_NONE into meLifetime on success.

Numeric: tests/FxTailsBStateDetach.cpp compiles the PRODUCTION State::Detach against the real CgsState.h
State and the PRODUCTION CollisionState::Detach against a fixture state with the real DataPoint. --rev
reads a b5 revision (the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsb_state_detach.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report

STATE_CPP = "src/GameShared/GameClasses/Sound/Logic/CgsState.cpp"
COLLISION_CPP = "src/GameSource/Sound/Collision/BrnCollisionState.cpp"
NUMERIC_CHECKS = 13


def normalise(text):
    return text.replace("\r\n", "\n")


def wiring(tree):
    state = body_or_empty(normalise(tree.read(STATE_CPP)), "bool State::Detach()")
    collision = body_or_empty(normalise(tree.read(COLLISION_CPP)), "bool CollisionState::Detach()")
    return [
        ("State::Detach refuses anything but E_UPDATE_ATTACHED (0x826C4C24..0x826C4C34)",
         re.search(r"if\s*\(\s*mauUpdateState\[0\]\s*!=\s*E_UPDATE_ATTACHED\s*\)\s*return\s+false\s*;", state) is not None),
        ("CollisionState::Detach adds nothing to the base (identical-code-folded at 0x826C4C20)",
         "meLifetime" not in collision and "State::Detach()" in collision),
    ]


def numeric(tree):
    try:
        state = definition(normalise(tree.read(STATE_CPP)), "bool State::Detach()")
        collision = definition(normalise(tree.read(COLLISION_CPP)), "bool CollisionState::Detach()")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTailsBStateDetach.cpp"), "fxtailsb_state_detach_body.inc",
                           state, "FxTailsBStateDetach",
                           extra_files={"fxtailsb_collision_detach_body.inc": collision})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtailsb_state_detach", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
