"""FX-CRASHVFX (crash parity 2026-09-25): BrnEffects::Utils::DebrisColourRandomiser::Randomise @0x8227E698 -- the
colour of every debris particle ParticleModule::SpawnDebris spawns.

Each of the console's two draws is ONE number (word 0 of the ring's vector slot, splatted through lvsl(0) / vspltw /
vperm) and each combine is ONE vmaddfp. The tree drew the slot's four words as four per-channel fractions (a hue
jitter the console never makes) and rounded the product before the add.

NUMERIC -- tests/FxCrashVfxDebrisColour.cpp compiles the revision's whole BrnEffectsDebrisColourRandomiser.cpp
(against the revision's header and the real CgsNumeric::Random) and compares 32 colours with the console's own
outputs (tests/FxCrashVfxDebrisColourData.h, the function's real instruction words run on emu64 by
scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_colour_data.py): every lane and the ring / seed / cursor.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_debris_colour.py [--rev <b5 rev>]
                                                                               [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, compile_and_run, report

SOURCE = "src/GameSource/Effects/BrnEffectsDebrisColourRandomiser.cpp"
HEADER = "src/GameSource/Effects/BrnEffectsDebrisColourRandomiser.h"
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
NUMERIC_CHECKS = 2


class RootTree(Tree):
    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = Path(root) if root else None

    def read(self, relative):
        if self.root is not None and (self.root / relative).exists():
            return (self.root / relative).read_text(encoding="utf-8-sig")
        try:
            return super().read(relative)
        except FileNotFoundError:
            return ""


def wiring(tree):
    source = tree.read(SOURCE).replace("\r\n", "\n")
    yield ("the draws are the vector-slot SPLAT (one number per draw) and the combines are fused (std::fma), "
           "both cited at the console's addresses",
           "DrawNextRingSplat" in source and "std::fma(mVecRange" in source
           and all(token in source for token in ("0x8227E6E4", "0x8227E714", "0x8227E788", "0x8227E78C")))


def numeric(tree):
    source = tree.read(SOURCE)
    header = tree.read(HEADER)
    if not source or not header:
        print("NUMERIC: cannot build -- BrnEffectsDebrisColourRandomiser.cpp / .h is absent")
        return None
    return compile_and_run(Path(__file__).with_name("FxCrashVfxDebrisColour.cpp"), "fxcrashvfx_debris_colour.inc",
                           source.replace("\r\n", "\n") + "\n", "FxCrashVfxDebrisColour", shadow={HEADER: header},
                           extra_sources=(RANDOM_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    return report("run_fxcrashvfx_debris_colour", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
