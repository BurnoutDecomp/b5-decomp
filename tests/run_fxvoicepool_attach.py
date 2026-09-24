"""FX-VOICEPOOL (crash parity 2026-09-24), second piece: CollisionEffect::Attach -- how a crash voice
starts and how hard it ducks the mix.

Against ARTIST:
  * InitWork<crashbin> @0x826EB240 / <propscrashbin> @0x826EB368 (DWARF BrnCollisionEffect.h:126):
    attach the manager's splicer bank, SetGain(0, 0.0, "Send01") BEFORE Play (0x826EB2D4..0x826EB2F4)
    -- the voice starts silent until ProcessUpdate sets its gain -- pitch slider 1, volume slider =
    the bin's MixerSlider, then GetSizeSpecificSettings<T> (0x826AABC8 / 0x826AAC88: Large z,
    Medium y, Small x, else "Bad Size"). The PC played at the voice's previous gain.
  * CalculateIntensity @0x82688240 (DWARF cpp:434) had NO BODY: only a Collision action ducks; a prop
    hit gives 12 * x (flt_82F2CEC8); a race car against the world / a race car / traffic, or traffic
    against anything, gives (100 - 25) * x + 25 (flt_82F2CED0 / CECC / CED4, flt_82F2CEC4); else 0.
  * Attach @0x826F8218: mixer input 1 = min(max(CalculateIntensity * 327.67 (flt_820B78E0), 0), 32767
    (flt_820AD310)) by fneg/fsel/fsubs/fsel/fctiwz (0x826F82DC..0x826F8330) -- the PC sent
    32767 - min(32767, max(0, x) * 327.67): inverted, every hit near full scale. And the azimuth is
    dropped when meFatality (+0x3C) == E_FATAL_START (0x826F8334..0x826F8340), not on a Detach action.

Numeric: tests/FxVoicepoolAttach.cpp compiles the PRODUCTION Attach (plus InitWork / GetSizeSpecific-
Settings / CalculateIntensity where the revision has them) against a recording fixture. --rev reads a
b5 revision (the RED side: <fix>~1); a revision without CalculateIntensity fails its direct checks.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvoicepool_attach.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

EFFECT_CPP = "src/GameSource/Sound/Collision/BrnCollisionEffect.cpp"
NUMERIC_CHECKS = 27
TEMPLATE_SIGNATURES = ("void CollisionEffect::GetSizeSpecificSettings(",
                       "void CollisionEffect::InitWork(")


def optional(source, signature):
    try:
        return definition(source, signature)
    except ValueError:
        return ""


def wiring(tree):
    source = code_only(tree.read(EFFECT_CPP))
    init_work = code_only(optional(tree.read(EFFECT_CPP), "void CollisionEffect::InitWork("))
    attach = code_only(optional(tree.read(EFFECT_CPP), "bool CollisionEffect::Attach()"))
    gain = init_work.find("SetGain(0, 0.0f")
    play = init_work.find(".Play(")
    return [
        ("InitWork<T> silences Send01 before it plays (SetGain(0, 0.0) 0x826EB2D4..0x826EB2F4)",
         0 <= gain < play),
        ("the ducking constants are the image's: 25 / 12 / 100 / 327.67 / 32767 "
         "(flt_82F2CEC4 / CEC8 / CECC..CED4, flt_820B78E0, flt_820AD310)",
         all(re.search(pattern, source) is not None for pattern in (
             r"KF_MIN_INTENSITY\s*=\s*25\.0f", r"KF_PROP_INTENSITY_SCALE\s*=\s*12\.0f",
             r"KF_RACECAR_VS_RACECAR\s*=\s*100\.0f", r"KF_INTENSITY_TO_MIXER\s*=\s*327\.67001f",
             r"KF_MIXER_INPUT_MAX\s*=\s*32767\.0f"))),
        ("Attach drops the azimuth on meFatality == E_FATAL_START, not on a Detach action",
         re.search(r"meFatality\s*==\s*E_FATAL_START", attach) is not None and "eAction::Detach" not in attach),
    ]


def numeric(tree):
    source = tree.read(EFFECT_CPP)
    try:
        bodies = [definition(source, "bool CollisionEffect::Attach()")]
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    flags = ""
    calculate = optional(source, "f32 CollisionEffect::CalculateIntensity(")
    if calculate:
        bodies.insert(0, calculate)
        flags = "/DFXVP_HAS_CALCULATE_INTENSITY"
    for signature in reversed(TEMPLATE_SIGNATURES):
        text = optional(source, signature)
        if text:
            bodies.insert(0, "template <typename T>\n" + text)
    return compile_and_run(Path(__file__).with_name("FxVoicepoolAttach.cpp"),
                           "fxvoicepool_attach_bodies.inc", "\n\n".join(bodies), "FxVoicepoolAttach",
                           extra_flags=flags)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxvoicepool_attach", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
