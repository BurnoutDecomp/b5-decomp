"""FX-TAILS-A item 4 (crash parity 2026-09-24): CrashPlayManager::OnBounce / OnVehicleHitConfirmed do the
console's float arithmetic -- the three fmadds (OnBounce's bounce cost @0x822A7F6C; OnVehicleHitConfirmed's
per-vehicle award @0x822C33AC and every-N award @0x822C3448) round ONCE, and the every-N award is
(boost + (HI - LO) * d) + LO (0x822C3448 / 0x822C344C), not boost + (LO + (HI - LO) * d).
(FX-SHOWTIME2's follow-ups. Its other note -- "the stationary-bounce min takes the wrong side on a NaN" -- does
not hold at this tree: fpu::Min is the rwmath fsel form since b27e1448, fsel(sum - max, max, sum), which keeps a
NaN sum exactly as the console's fsel @0x822A8018 does; the numeric test pins it.)

  1. WIRING -- the three sites spell std::fmaf with the console's operands, and the every-N award adds LO last.
  2. NUMERIC -- tests/FxTailsACrashPlay.cpp runs the extracted production bodies (with the file's constants and
     witness) on the REAL CrashPlayManager struct and compares bit for bit with a model of the asm, on inputs
     where one rounding and two differ. The old bodies build too, so the numeric side shows what they got wrong.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsa_crash_play.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import STRSTREAM_CPP, Tree, body_or_empty, code_only, compile_and_run, definition, report

CRASHPLAY_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayManager.cpp"
BODIES = ["void CrashPlayManager::ClampBoostLevel()",
          "void CrashPlayManager::OnBounce(",
          "void CrashPlayManager::OnVehicleHitConfirmed("]
NUMERIC_CHECKS = 21


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    source = tree.read(CRASHPLAY_CPP).replace("\r\n", "\n")
    bounce = squash(body_or_empty(source, BODIES[1]))
    yield ("OnBounce: the cost is one fmadds(HARD - EASY, difficulty, EASY) (0x822A7F6C)",
           "std::fmaf(KF_COST_FOR_BOUNCE_BOOST_HARD-KF_COST_FOR_BOUNCE_BOOST_EASY,mfDifficultyLevel,"
           "KF_COST_FOR_BOUNCE_BOOST_EASY)" in bounce)
    yield ("OnBounce: the stationary top-up is fpu::Min(sum, max) -- fsel(sum - max, max, sum) (0x822A8018)",
           "rw::math::fpu::Min(mfAftertouchPower+KF_AFTERTOUCH_FOR_STATIONARY_BOUNCE_BOOST,GetMaxAftertouchPower())"
           in bounce)
    hit = squash(body_or_empty(source, BODIES[2]))
    yield ("OnVehicleHitConfirmed: the per-vehicle award is one fmadds(scaled, HIGH - LOW, LOW) (0x822C33AC)",
           "std::fmaf(lfScaledScore,KF_BOOST_FOR_VEHICLE_IMPACT_HIGH-KF_BOOST_FOR_VEHICLE_IMPACT_LOW,"
           "KF_BOOST_FOR_VEHICLE_IMPACT_LOW)" in hit)
    yield ("OnVehicleHitConfirmed: every-N award = fmadds(HI - LO, d, boost) + LO (0x822C3448 / 0x822C344C)",
           "mfBoostPercentage=std::fmaf(KF_BOOST_FOR_EVERY_10_CARS_HIT_HI-KF_BOOST_FOR_EVERY_10_CARS_HIT_LO,"
           "mfDifficultyLevel,mfBoostPercentage)+KF_BOOST_FOR_EVERY_10_CARS_HIT_LO;" in hit)


def numeric(tree):
    source = tree.read(CRASHPLAY_CPP).replace("\r\n", "\n")
    parts = re.findall(r"^static\s+(?:const\s+)?(?:f32|s32|u32)\s+K[FI]_[A-Z0-9_]+\s*=[^;]+;", source, re.M)
    witness = re.search(r"struct CrashPlayWitness\s*\{[^}]*\};\s*CrashPlayWitness gCrashPlayWitness[^;]*;", source)
    if witness is None:
        print("NUMERIC: cannot build -- the [crashplay] witness block is absent")
        return None
    parts.append(witness.group(0))
    try:
        parts += [definition(source, signature) for signature in BODIES]
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxTailsACrashPlay.cpp"), "fxtailsa_crash_play.inc",
                           "\n".join(parts) + "\n", "FxTailsACrashPlay", extra_sources=(STRSTREAM_CPP,))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtailsa_crash_play", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
