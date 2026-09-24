"""FX-DIRECTOR (crash parity 2026-09-24): MomentPlayerStunt's ICE take id is the name's CRC ZERO-extended.

  CgsResource::ID::HashString @0x828D84A8 returns through `nor r11, r9, r9 ; clrldi r3, r11, 0x20`, and
  MomentPlayerStunt::Update @0x82272750 hands that register to DirectorResourceManager::GetKeyAnim as the
  64-bit id (0x82272B04). The PC's MomentPlayerStunt_StageTakeReference widened HashString's s32 return
  straight to u64 -- a SIGN extension whenever the CRC's top bit is set. "World_Signature_305" (the take
  the live jump asked for) is 0x9CA2368A, so the dictionary missed, "lpIceTake != NULL" fired and the
  guid read faulted at address 8: the first live run with the moment tick on died there
  (scratch/bugtest/runs/fxdirector_moment_tick/20260924_180957).

Numeric: tests/FxDirectorStuntTake.cpp compiles the revision's MomentPlayerStunt_StageTakeReference (and
the detail:: declarations it calls) with the production HashString and SPrintf, against a one-take
dictionary keyed like the console's bundle entries.
Wiring: the widening goes through u32.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_stunt_take.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, compile_and_run, definition, report

SOURCE = "src/GameSource/Director/MomentController/Moments/BrnMomentPlayerStunt.cpp"
HASH_CPP = REPO / "src/GameShared/GameClasses/System/Resource/CgsResourceID.cpp"
SPRINTF_CPP = REPO / "src/GameShared/GameClasses/Core/CgsStringUtils.cpp"
STRSTREAM_CPP = REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"
HELPER = "static void MomentPlayerStunt_StageTakeReference("
NUMERIC_CHECKS = 6


def squash(text):
    return re.sub(r"\s+", "", text)


def wiring(tree):
    source = tree.read(SOURCE)
    try:
        helper = definition(source, HELPER)
    except ValueError:
        helper = ""
    code = squash("\n".join(line.split("//", 1)[0] for line in helper.splitlines()))
    yield ("the take id widens HashString's return through u32 (zero-extension, HashString's clrldi r3, r11, 0x20)",
           "SetHash(static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(" in code)


def numeric(tree):
    source = tree.read(SOURCE)
    try:
        inc = "\n\n".join((definition(source, "namespace detail"), "using namespace detail;",
                           definition(source, HELPER)))
    except ValueError as error:
        print(f"NUMERIC: the revision lacks {error}")
        return None
    return compile_and_run(Path(__file__).with_name("FxDirectorStuntTake.cpp"), "stunt_take_reference.inc", inc,
                           "FxDirectorStuntTake", extra_sources=(HASH_CPP, SPRINTF_CPP, STRSTREAM_CPP))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector_stunt_take", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
