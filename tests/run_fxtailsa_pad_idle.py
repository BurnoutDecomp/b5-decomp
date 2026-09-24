"""FX-TAILS-A item 1 (crash parity 2026-09-24): the PC pad fill computes the pad record's idle byte (+0x3A0).

The console's writer is CgsInput::InputPads::Update @0x828F8690 (export hole; tools/re/ppcdis.py): per pad
r7 = 1 (`li r26,1` @0x828F8780 / `mr r7,r26` @0x828F89B8), cleared by every action that is held -- `lfs f0,
0x78E4(r17)` = 0x820F78E4 = 0.1f, `fcmpu value, f0 ; blt` = not held, so held means NOT below 0.1 -- or
released this frame (the not-held arm with the previous frame's down byte set), then `stb r7, 0x3A0(r25)`
@0x828F8CB0 (DWARF PadOutputInformation::mbPadIdle). BridgeControllerToDirector @0x823C0F70 turns it into the
director's "some input" byte (cntlzw, 0x823C0FC8..0x823C0FFC), which drives GameState +0x148 mfPadInactiveTime /
+0x14C mfPlayerInactiveTime (Picture Paradise idle entry, the junkyard idle orbit). The PC leaf stored 0 on
every fill, so both clocks were zeroed every frame.

  1. WIRING -- UpdatePlayer0's status pass uses the console's held test `!(lfValue < KF_ACTION_DOWN_THRESHOLD)`
     (0.1f), accumulates the idle byte from held-or-released actions, and stores it at the tail.
  2. NUMERIC -- tests/FxTailsAPadIdle.cpp runs the revision's whole CgsInputPadsPC.cpp (UpdatePlayer0 exists in
     every revision, so the old body builds and shows what it wrote) against a scripted XInput pad.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_pad_idle.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import STRSTREAM_CPP, Tree, body_or_empty, code_only, compile_and_run, report

PADSPC_CPP = "src/GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.cpp"
SHADOWED = (
    "src/GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h",
    "src/GameShared/GameClasses/System/Input/CgsInputModuleIO.h",
    "src/GameShared/GameClasses/System/Input/CgsInputPads.h",
    "src/GameShared/GameClasses/System/Input/CgsInputTypes.h",
    "src/GameShared/GameClasses/System/Input/Devices/X360/CgsInputDeviceX360Pad.h",
)
UPDATE = "    void InputPadsPC::UpdatePlayer0(InputIO::OutputBuffer* lpOutput)"
NUMERIC_CHECKS = 32


def wiring(tree):
    source = tree.read(PADSPC_CPP).replace("\r\n", "\n")
    update = body_or_empty(source, UPDATE)
    yield ("UpdatePlayer0 exists (the production fill under test)", bool(update))
    yield ("the action test is the console's `fcmpu value, 0.1 ; blt` = held unless below 0.1 (0x828F89CC)",
           re.search(r"lbDown\s*=\s*!\s*\(\s*lfValue\s*<\s*KF_ACTION_DOWN_THRESHOLD\s*\)", update) is not None)
    yield ("KF_ACTION_DOWN_THRESHOLD is 0.1f (0x820F78E4 = 0x3DCCCCCD)",
           re.search(r"const f32 KF_ACTION_DOWN_THRESHOLD\s*=\s*0\.1f;", code_only(source)) is not None)
    yield ("r7: idle starts at 1 and a held or released-this-frame action clears it (0x828F89EC / 0x828F8A6C)",
           re.search(r"bool lbPadIdle\s*=\s*true;", update) is not None
           and re.search(r"if\s*\(\s*lbDown\s*\|\|\s*sabActionWasDown\[luAction\]\s*\)\s*lbPadIdle\s*=\s*false;",
                         update) is not None)
    stores = re.findall(r"lrPad\.mbDisconnected\s*=\s*([^;]+);", update)
    yield ("the tail stores the computed byte at +0x3A0 (`stb r7, 0x3A0(r25)` @0x828F8CB0), not a constant 0",
           stores == ["lbPadIdle ? 1 : 0"])


def numeric(tree):
    source = tree.read(PADSPC_CPP)
    if UPDATE not in source.replace("\r\n", "\n"):
        print("NUMERIC: cannot build -- InputPadsPC::UpdatePlayer0 absent")
        return None
    shadow = {path: tree.read(path) for path in SHADOWED}
    return compile_and_run(Path(__file__).with_name("FxTailsAPadIdle.cpp"), "fxtailsa_pads_pc.inc", source,
                           "FxTailsAPadIdle", shadow=shadow, extra_sources=(STRSTREAM_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtailsa_pad_idle", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
