"""FX-LADDER (crash parity 2026-09-25): CrashPlayManager::UpdateMomentum @0x823020D0 -- the deferred bounce
penalty's grace-period gate takes the console's arm on a NaN.

  0x82302294  lfs f0, 0x144(r31) ; fsubs f0, f0, f30(dt) ; stfs  -- mfLoseBoostGracePeriod -= dt
  0x823022A0  fcmpu cr6, f0, f31                                  -- f31 = flt_82001CC0 = 0.0 (0x82302188)
  0x823022A4  bgt cr6, 0x823022D0                                 -- skip: taken only for an ORDERED grace > 0
  0x823022A8  the lose-boost arm: mbAboutToLoseBoost (+0x151) = r25 (0), mfBoostPercentage (+0x134) -=
              miConsecutiveBouncesOnGround (+0x148) == 1 ? flt_82FAD300 : flt_82FAD304
`bgt` is not taken on an unordered compare, so a NaN grace period LOSES the boost; the tree's
`grace <= 0.0f` skipped it. Faithful: !(grace > 0.0f).

Extracts the production UpdateMomentum, ClampBoostLevel, the file's tunables, its [crashplay] witness and
its IsZeroVmx helper from BrnCrashPlayManager.cpp and runs FxLadderGraceNan.cpp on the REAL CrashPlayManager /
ActiveRaceCar structs.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxladder_grace_nan.py [--rev <b5 rev>]

`--rev <b5 rev>` reads BrnCrashPlayManager.cpp from that revision (the RED side of the fix).
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, compile_and_run, definition, report

CRASHPLAY_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayManager.cpp"
BODIES = ["void CrashPlayManager::ClampBoostLevel()", "void CrashPlayManager::UpdateMomentum("]
HELPER = "static inline bool IsZeroVmx("
NUMERIC_CHECKS = 15


def numeric(tree):
    source = tree.read(CRASHPLAY_CPP).replace("\r\n", "\n")
    parts = re.findall(r"^static\s+(?:const\s+)?(?:f32|s32|u32)\s+K[FI]_[A-Z0-9_]+\s*=[^;]+;", source, re.M)
    witness = re.search(r"struct CrashPlayWitness\s*\{[^}]*\};\s*CrashPlayWitness gCrashPlayWitness[^;]*;", source)
    if witness is None:
        print("NUMERIC: cannot build -- the [crashplay] witness block is absent")
        return None
    parts.append(witness.group(0))
    try:
        parts.append(definition(source, HELPER))
        parts += [definition(source, signature) for signature in BODIES]
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxLadderGraceNan.cpp"), "fxladder_grace_nan.inc",
                           "\n".join(parts) + "\n", "FxLadderGraceNan")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    return report("run_fxladder_grace_nan", [], numeric(Tree(args.rev)), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
