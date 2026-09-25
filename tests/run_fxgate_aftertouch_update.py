"""FX-GATE (crash parity 2026-09-25): BehaviourAftertouchCrash::Update @0x82228158 -- the WHOLE production body against
the console's words run WHOLE on emu64.

The production text (Update with its file-local helpers and KF statics, CheckForPlayerCarBouncing,
CameraImpactEffect::RegisterImpact / ::Update) is extracted by run_fxcamrig_rig.generate, compiled with the real
CgsNumeric::Random (CgsRandom.cpp) against tests/FxGateAftertouchUpdate.cpp, and run frame by frame from the rows of
tests/FxGateAftertouchUpdateData.h: edge and random cases, three frames each, every frame the console's function run
from its first word to its return (scratch/CRASHPARITY_0922/fixes/FX-GATE.update/gen_update_data.py). Four checks per
frame: the rig's fields, the shared random, the camera (transform, FOV, E_FLAG_VALID) and the stubs' call records
(PositionLag, the four CreateLookAt eyes / targets, the two SLerp amounts, the two CameraShake calls, SineLerp, the
assert count). The SLerp is stubbed on both sides (it is FX-GATE item B).

  WIRING -- the console has no fsqrts / fres in Update: every root is vrsqrtefp + two Newton steps (ROUNDING_RULE 5),
  every |v|^2 a vmsum3fp128 (rule 1), and CameraImpactEffect::Update's decay one fmadds (0x8222927C, rule 3).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_aftertouch_update.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report
from run_fxcamrig_rig import IMPACT, LAG, RIG, generate

UPDATE = "bool BehaviourAftertouchCrash::Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo)"
IMPACT_UPDATE = "void CameraImpactEffect::Update("
DATA = Path(__file__).with_name("FxGateAftertouchUpdateData.h")


def numeric_total():
    text = DATA.read_text(encoding="utf-8")
    cases = int(re.search(r"kuUpdateCases = (\d+);", text).group(1))
    frames = int(re.search(r"kuUpdateFrames = (\d+);", text).group(1))
    return cases * frames * 4


def body(tree, path, signature):
    try:
        return code_only(definition(tree.read(path).replace("\r\n", "\n"), signature))
    except ValueError:
        return ""


def wiring(tree):
    update = body(tree, RIG, UPDATE)
    decay = body(tree, IMPACT, IMPACT_UPDATE)
    yield ("Update has no std::sqrt (every root is vrsqrtefp + two Newton steps: 0x82228220, 0x82228878, 0x82228910, "
           "0x82228AEC, 0x82228C7C)", bool(update) and "std::sqrt" not in update)
    yield ("Update has no vendor MagnitudeSquared / Magnitude / Normalize / NormalizeReturnMagnitude (vmsum3fp128 is "
           "ROUNDING_RULE 1)", bool(update) and re.search(
               r"rw::math::vpu::(MagnitudeSquared|Magnitude|Normalize|NormalizeReturnMagnitude)\s*\(", update) is None)
    yield ("CameraImpactEffect::Update's decay is ONE fmadds (0x8222927C): std::fmaf(-mfImpactFactor, decay, "
           "mfImpactFactor)", re.search(r"mfImpactFactor\s*=\s*std::fmaf\(\s*-mfImpactFactor\s*,\s*lrParameters\."
                                        r"mfShakeDecayFactor\s*,\s*mfImpactFactor\s*\)", decay) is not None)


def numeric(tree):
    text = generate(tree.read(RIG).replace("\r\n", "\n"), tree.read(IMPACT).replace("\r\n", "\n"),
                    tree.read(LAG).replace("\r\n", "\n"))
    return compile_and_run(Path(__file__).with_name("FxGateAftertouchUpdate.cpp"), "fxcamrig_rig.inl", text,
                           "FxGateAftertouchUpdate",
                           extra_sources=[REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_aftertouch_update", list(wiring(tree)), numeric(tree), numeric_total())


if __name__ == "__main__":
    sys.exit(main())
