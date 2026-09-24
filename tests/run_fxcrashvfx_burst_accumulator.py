"""FX-CRASHVFX (crash parity 2026-09-24, item 1 callee wall): BrnEffects::BurstAccumulator::Update @0x8227EC90.

The spark-burst accumulator ProcessRaceCarContacts and HandleVehicleVehicleSparks drive before every spark shower.
The old body had the DWARF's shape wrong (two phantom int parameters: the r4/r5 slots the two floats eat) and three
arithmetic defects: a NaN time did not reset the size (fcmpu unordered falls through `blt`), the threshold's
final multiply-add was rounded twice (the console fuses it), and a NaN size returned 0x80000000 bursts instead of
fctidz's low word, 0.

  1. WIRING -- the DWARF signature `uint32_t Update(float32_t, float32_t, Random &)` and member names.
  2. NUMERIC -- tests/FxCrashVfxBurstAccumulator.cpp compiles the PRODUCTION body against the revision's header and
     the real CgsNumeric::Random and compares it, bit for bit, with the function's own instruction words run by
     scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_burst_data.py (tests/FxCrashVfxBurstAccumulatorData.h).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_burst_accumulator.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

HEADER = "src/GameSource/Effects/ActiveRaceCarData.h"
SOURCE = "src/GameSource/Effects/ActiveRaceCarData.cpp"
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
NEW_SIGNATURE = "u32 BurstAccumulator::Update(f32 lfBurstSize, f32 lfTime, CgsNumeric::Random& lrRandom)"
OLD_SIGNATURE = "s32 BurstAccumulator::Update(f32 lfDelta, f32 lfTime, s32 liArg3, s32 liArg4, CgsNumeric::Random* lpRandom)"

NUMERIC_CHECKS = 7 * 4


def wiring(tree):
    header = code_only(tree.read(HEADER))
    yield ("BurstAccumulator::Update has the DWARF signature `uint32_t Update(float32_t, float32_t, Random &)` "
           "(no phantom r4/r5 ints)",
           re.search(r"u32\s+Update\(\s*f32\s+\w+,\s*f32\s+\w+,\s*CgsNumeric::Random&\s*\w+\)", header) is not None)
    yield ("the members carry the DWARF names (mfBurstSizeThreshold / mfBurstTimeThreshold / mfCurrentBurstSize)",
           all(name in header for name in ("mfBurstSizeThreshold", "mfBurstTimeThreshold", "mfCurrentBurstSize")))


def numeric(tree):
    source = tree.read(SOURCE).replace("\r\n", "\n")
    header = tree.read(HEADER)
    try:
        body = definition(source, NEW_SIGNATURE)
        call = "#define FXCRASHVFX_BURST_UPDATE(acc, size, time, rnd) ((acc).Update((size), (time), (rnd)))\n"
    except ValueError:
        try:
            body = definition(source, OLD_SIGNATURE)
            call = ("#define FXCRASHVFX_BURST_UPDATE(acc, size, time, rnd) "
                    "(static_cast<u32>((acc).Update((size), (time), 0, 0, &(rnd))))\n")
        except ValueError:
            print("NUMERIC: cannot build -- BurstAccumulator::Update is absent")
            return None
    inc = call + body + "\n"
    # the macro must be visible before main(); put it at file scope ahead of the namespace block
    return compile_and_run(Path(__file__).with_name("FxCrashVfxBurstAccumulator.cpp"), "fxcrashvfx_burst_update.inc",
                           inc, "FxCrashVfxBurstAccumulator", shadow={HEADER: header},
                           extra_sources=(RANDOM_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_burst_accumulator", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
