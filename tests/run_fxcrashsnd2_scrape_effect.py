"""FX-CRASHSND2 (crash parity 2026-09-24, item 2): the scrape voice -- ScrapeEffect.

A collision state that UpdateScrapes gives the SCRAPE lifetime runs ScrapeEffect, whose
AEMS_ScrapeGranulator voice IS the scrape sound. Against ARTIST:
  * GetIntensity @0x826BD620 is the scrape's RELATIVE VELOCITY (|v| * KF_AEMS_FRICTION_SCALE_UP 250,
    * KF_AI_SCALING_UP 10 against a car / traffic, else * KF_WORLD_SCALING_UP 2.5 outside a fatality),
    ramped to 0 over KF_RAMP_DOWN_TIME 0.25 s -- the PC returned the contact impulse;
  * UpdateParams @0x826F8578 starts the voice only KF_TIME_DELAY_BEFORE_SCRAPES (0.15 s) after the
    state attached and, playing, sets friction_stress = the intensity clamped to [0, 32767] -- the
    PC started at once, sent 32767 - intensity (inverted), and overwrote normal_stress / material_a;
  * ProcessUpdate @0x826BD6F8 feeds friction_stress to the mixer input every frame -- the PC skipped
    it whenever the voice was not live.
Constants from the image: 0x82F2CED8 0.15, 0x82F2CEDC 250, 0x82F2CEE0 0.25, 0x82F2CEE4 10,
0x82F2CEE8 2.5 (DWARF BrnScrapeEffect.cpp:32..43).

Numeric: tests/FxCrashSnd2ScrapeEffect.cpp compiles the PRODUCTION GetIntensity / UpdateParams /
ProcessUpdate and the file's constant block as members of a recording fixture. --rev reads a b5
revision (the RED side: <fix>~1) -- the old bodies compile and fail.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashsnd2_scrape_effect.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

SCRAPE_CPP = "src/GameSource/Sound/Collision/BrnScrapeEffect.cpp"
NUMERIC_CHECKS = 32


def constants_block(source):
    match = re.search(r"namespace\s*\{\s*const u32 KU_SCRAPE_PARAMETERS.*?\n\}\n", source, re.S)
    if match is None:
        raise ValueError("the constant block")
    return match.group(0)


def wiring(tree):
    source = code_only(tree.read(SCRAPE_CPP))
    return [
        ("KF_TIME_DELAY_BEFORE_SCRAPES = 0.15 (0x82F2CED8) gates the voice start",
         re.search(r"KF_TIME_DELAY_BEFORE_SCRAPES\s*=\s*0\.15f", source) is not None and
         re.search(r"GetTimeWeAttached\(\)\s*\+\s*KF_TIME_DELAY_BEFORE_SCRAPES\s*<\s*lpState\s*->\s*GetCurrentTime\(\)",
                   source) is not None),
        ("the intensity constants are the image's: 250 / 0.25 / 10 / 2.5 (0x82F2CEDC..0x82F2CEE8)",
         all(re.search(pattern, source) is not None for pattern in (
             r"KF_AEMS_FRICTION_SCALE_UP\s*=\s*250\.0f", r"KF_RAMP_DOWN_TIME\s*=\s*0\.25f",
             r"KF_AI_SCALING_UP\s*=\s*10\.0f", r"KF_WORLD_SCALING_UP\s*=\s*2\.5f"))),
    ]


def numeric(tree):
    try:
        source = tree.read(SCRAPE_CPP)
        constants = constants_block(source)
        bodies = "\n\n".join(definition(source, signature) for signature in (
            "f32 ScrapeEffect::GetIntensity(",
            "void ScrapeEffect::UpdateParams(",
            "void ScrapeEffect::ProcessUpdate()"))
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    return compile_and_run(Path(__file__).with_name("FxCrashSnd2ScrapeEffect.cpp"),
                           "fxcrashsnd2_scrape_effect_bodies.inc", bodies, "FxCrashSnd2ScrapeEffect",
                           extra_files={"fxcrashsnd2_scrape_effect_constants.inc": constants})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxcrashsnd2_scrape_effect", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
