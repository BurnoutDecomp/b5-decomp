"""FX-AIBUZZ item 3 (crash parity 2026-09-24): the CrashPlayManager fused sites FX-TAILS-A left as follow-ups.

The console computes these with ONE rounding (fmadds / fnmsubs) and the PC rounded twice:
  GetShowtimeTrafficDensityScale @0x822A8088  fmadds (MAX - MIN) * (1 - d) + MIN          @0x822A8110
  UpdateMomentum @0x823020D0                  fmadds distance award                        @0x82302174
                                              fnmsubs ground cost  (boost - K * dt)        @0x823021F8
                                              fmadds airtime award                         @0x82302260
                                              fnmsubs aftertouch bleed ; fsel >= 0         @0x82302338
                                              fnmsubs no-boost bleed ; fsel >= 0           @0x82302374
and UpdateMomentum's IsZero(mLastPlayerPos) is the console's vcmpgtfp./all-false form at FLT_EPSILON
(flt_82014460, 0x82302128..0x82302160): a NaN lane is zero and 5e-7 is not -- the shared vpu::IsZero it used
(|c| <= 1e-6) says the opposite on both.

  1. WIRING -- the six std::fmaf spellings with the console's operands, and the local IsZero helper.
  2. NUMERIC -- tests/FxAiBuzzCrashPlayFma.cpp runs the extracted production bodies on the real structs against
     the asm, on inputs where one rounding and two differ. The old bodies build too.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxaibuzz_crashplay_fma.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, body_or_empty, code_only, compile_and_run, definition, report

CRASHPLAY_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayManager.cpp"
BODIES = ["void CrashPlayManager::ClampBoostLevel()",
          "void CrashPlayManager::UpdateMomentum(",
          "f32 CrashPlayManager::GetShowtimeTrafficDensityScale()"]
HELPER = "static inline bool IsZeroVmx("
NUMERIC_CHECKS = 22


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    source = tree.read(CRASHPLAY_CPP).replace("\r\n", "\n")
    density = squash(body_or_empty(source, BODIES[2]))
    yield ("GetShowtimeTrafficDensityScale: fmadds(MAX - MIN, 1 - d, MIN) (0x822A8110)",
           "std::fmaf(KF_MAX_TRAFFIC_DENSITY-KF_MIN_TRAFFIC_DENSITY,1.0f-mfDifficultyLevel,KF_MIN_TRAFFIC_DENSITY)"
           in density)
    momentum = squash(body_or_empty(source, BODIES[1]))
    yield ("UpdateMomentum: distance award fmadds(K, d, boost) (0x82302174)",
           "mfBoostPercentage=std::fmaf(KF_BOOST_FOR_DISTANCE_TRAVELLED,lfDistanceTravelledLastFrame,"
           "mfBoostPercentage);" in momentum)
    yield ("UpdateMomentum: ground cost fnmsubs == fmaf(-K, dt, boost) (0x823021F8)",
           "mfBoostPercentage=std::fmaf(-KF_COST_FOR_BEING_ON_GROUND,lfSimTimerTimeStep,mfBoostPercentage);"
           in momentum)
    yield ("UpdateMomentum: airtime award fmadds(K, dt, boost) (0x82302260)",
           "mfBoostPercentage=std::fmaf(KF_BOOST_FOR_INITIAL_AIRTIME,lfSimTimerTimeStep,mfBoostPercentage);"
           in momentum)
    yield ("UpdateMomentum: aftertouch bleed fnmsubs(dt, rate, aftertouch) then Max 0 (0x82302338)",
           "rw::math::fpu::Max(std::fmaf(-lfSimTimerTimeStep,lfDecayRate,mfAftertouchPower),0.0f)" in momentum)
    yield ("UpdateMomentum: no-boost bleed fnmsubs(dt, 0.5, aftertouch) then Max 0 (0x82302374)",
           "std::fmaf(-lfSimTimerTimeStep,KF_AFTERTOUCH_NO_BOOST_DECAY_TIME,mfAftertouchPower)" in momentum)
    yield ("UpdateMomentum: the award's IsZero is the console form (vcmpgtfp./all-false at FLT_EPSILON)",
           "if(!IsZeroVmx(mLastPlayerPos))" in momentum and "rw::math::vpu::IsZero(" not in momentum
           and body_or_empty(source, HELPER) != "")


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
    except ValueError:
        pass   # the old tree has no helper; its UpdateMomentum does not call one
    try:
        parts += [definition(source, signature) for signature in BODIES]
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxAiBuzzCrashPlayFma.cpp"), "fxaibuzz_crashplay_fma.inc",
                           "\n".join(parts) + "\n", "FxAiBuzzCrashPlayFma")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxaibuzz_crashplay_fma", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
