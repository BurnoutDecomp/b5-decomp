"""FX-CRASHSND item 3 (crash parity 2026-09-24): FxEffect::Notify's E_WINDOW_SMASH note.

FxEffect::Notify @0x826F7248 case 0 reads `lwz r11, 0xC8(r27)` (0x826F73E0) and picks tag 2 for 1,
tag 0 for 2, nothing otherwise. The old FLAG called that read "far past the 20-byte
Message<FxMessage>" -- an out-of-record read. It is not: the DWARF locals of the arm are
`const Message<FxMessage_WindowSmash>* lpWindowSmashMessage` (BrnFxEffect.cpp:296) and
`const GlassSmashOrCrackEvent& lEvent` (:299), so +0xC8 is the embedded glass event's meNewState
(event at message +0x20, meNewState at +0xA8 -- the X360 layout sub_826BE398 reads: +0xA0 vehicle
id, +0xA4 part, +0xA8 new state asserted < NUM_GLASS_STATES at BrnCollisionStateManager.cpp:1525).
No producer posts type 0 in ARTIST (the sound unity TU's AddEvent<Message<T>> block holds no
record large enough; no export names FxMessage_WindowSmash; Notify has no direct caller): glass
reaches the sound through CollisionStateManager::UpdateGlass @0x826D4850 as input collisions.
The arm is dead on the console, so its behaviour is left as the console's "neither" outcome.

A structural runner for a documentation correction: the false premise is gone, the corrected
note names the real field and the missing producer, and the arm's behaviour is unchanged.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashsnd_window_smash.py [--rev <b5 rev>]
"""
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, body_or_empty, report

FX_CPP = "src/GameSource/Sound/Global/BrnFxEffect.cpp"
NOTIFY = "void FxEffect::Notify(const CgsSound::Io::MessageHeader* apMessageHeader)"


def window_smash_arm(notify_code):
    match = re.search(r"case\s+FxMessage::E_WINDOW_SMASH\s*:(.*?)(?=case\s+FxMessage::)", notify_code, re.S)
    return match.group(1) if match else ""


def wiring(tree):
    source = tree.read(FX_CPP)
    notify_banner = source[source.find("// FxEffect::Notify(const MessageHeader*)"):source.find(NOTIFY)] \
        if NOTIFY in source else ""
    arm = window_smash_arm(body_or_empty(source, NOTIFY))
    return [
        ("the false premise is gone: the +0xC8 read is no longer called out-of-record / past a 20-byte record",
         notify_banner != "" and "far past the 20-byte" not in notify_banner and
         "reproduce an out-of-record read" not in notify_banner),
        ("the note names the real field: Message<FxMessage_WindowSmash> -> GlassSmashOrCrackEvent::meNewState "
         "at +0xC8 (DWARF BrnFxEffect.cpp:296/:299, sub_826BE398)",
         all(token in notify_banner for token in
             ("Message<FxMessage_WindowSmash>", "GlassSmashOrCrackEvent", "meNewState", "+0xC8", "sub_826BE398"))),
        ("the note documents the missing producer and the console's real glass path "
         "(CollisionStateManager::UpdateGlass @0x826D4850)",
         "PRODUCER IS MISSING" in notify_banner and "CollisionStateManager::UpdateGlass @0x826D4850" in notify_banner),
        ("the dead arm keeps the console's 'neither 1 nor 2' outcome: no GetSampleTag, FX bank, mixer output 3",
         arm != "" and "GetSampleTag" not in arm and "GetFxSpliceBank" in arm and
         re.search(r"mau8MixerOutputs\s*\[\s*liSlot\s*\]\s*=\s*3\s*;", arm) is not None),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxcrashsnd_window_smash", wiring(tree), (0, 0), 0)


if __name__ == "__main__":
    sys.exit(main())
