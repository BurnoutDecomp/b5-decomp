"""FX-AINAN2: NaN branch polarity in the rival aggression state machine (BrnAIAggression.cpp).

Extracts the production bodies whose float decisions were re-spelt -- the twelve inlined
StateHasTimedOut() tests, AttackSlam's and OutOfRange's lead/aheadness/schedule tests,
AcrossSeparationTooBig, CheckForCarVeeringAwayFromPlayer, SetSlowOvertakingSpeed's ceiling fsel and
the StopAttacking / CurveToKeepLarge asserts -- plus the small helpers they call through, and
feeds them NaNs. The geometry queries (GetLeadingSeparation, GetAheadness, GetSeparation, ...)
are fixtures returning configured answers, so each check isolates one console branch. See
FxAinan2Aggression.cpp for the ARTIST addresses.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxainan2_aggression.py [--rev <rev>]

`--rev <b5 rev>` reads the production file from that revision (the RED side of the fix).
"""
import re
import sys
sys.dont_write_bytecode = True
from aidrv_common import Tree, compile_and_run, definition, parse_args  # noqa: E402

AGGRESSION = "src/GameSource/World/AI/BrnAIAggression.cpp"

SIGNATURES = [
    "f32 CurveToKeepLarge(f32 lfInput)",
    "bool AIAggression::AcrossSeparationTooBig(const AICar* lpThisCar, const AICar* lpOtherCar)",
    "void AIAggression::CheckForCarVeeringAwayFromPlayer(f32 lfTimeStep)",
    "f32 AIAggression::GetMaxOvertakeSpeed() const",
    "f32 AIAggression::GetMinFallBackSpeed()",
    "void AIAggression::SetSlowFallbackSpeed()",
    "void AIAggression::SetSlowOvertakingSpeed()",
    "void AIAggression::StopAttacking(EStopAttack leStopAttack)",
    "void BrnAI::AIAggression::UpdateAggressionPassive(",
    "void BrnAI::AIAggression::UpdateAggressionStateAttackSlam()",
    "void BrnAI::AIAggression::UpdateAggressionStateBeFodder()",
    "void BrnAI::AIAggression::UpdateAggressionStateClipOffBehind()",
    "void BrnAI::AIAggression::UpdateAggressionStateComeSlowFromBehind()",
    "void BrnAI::AIAggression::UpdateAggressionStateDropBackToSlam(",
    "void BrnAI::AIAggression::UpdateAggressionStateFallPast(",
    "void BrnAI::AIAggression::UpdateAggressionStateOutOfRange(",
    "void BrnAI::AIAggression::UpdateAggressionStateOvertakeFast()",
    "void BrnAI::AIAggression::UpdateAggressionStateOvertakeToSlam(",
    "void BrnAI::AIAggression::UpdateAggressionStateSpurtForward()",
    "void BrnAI::AIAggression::UpdateAggressionStateVeer()",
    "void BrnAI::AIAggression::UpdateAggressionStateVeerExtreme()",
    "void BrnAI::AIAggression::UpdateAggressionStateWait()",
]


def main():
    args = parse_args()
    source = Tree(args.rev).read(AGGRESSION)
    # The file-scope rodata constants the bodies name (column-0 `const f32 KF_...`).
    constants = re.findall(r"^const f32\s+KF_\w+\s*=[^;]+;", source, re.M)
    chunks = ["namespace BrnAI {", "namespace vpu = rw::math::vpu;"] + constants
    chunks += [definition(source, signature) for signature in SIGNATURES]
    chunks.append("}")
    sys.exit(compile_and_run("FxAinan2Aggression.cpp", chunks, prefix="brn_fxainan2_agg_"))


if __name__ == "__main__":
    main()
