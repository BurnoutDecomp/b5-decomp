"""FX-EMITTER (crash parity 2026-09-24): the world emitter's per-frame mix -- EmitterEffect::ProcessUpdate.

ARTIST @0x826E6BC8: "lpModule" is asserted (l.165) and the body runs on; the listener (module +0x29E0)
is measured against the entity through its radius (W & 0xFFFF, the reciprocal `fdivs f0,1.0,R`, the
vmsum3fp/vrsqrtefp length) and clamped to t in [0, 1] (two `fsel`s); the volume mixer output is scaled
by Curve::GetOutput(1 - t, E_ONE_MINUS_EQPWR), inlined at 0x826E6D70..0x826E6DE4 (the CgsSoundUtils.cpp:161
input assert, `fmsubs f0,f30,511,511 ; fctiwz ; slwi 2 ; subf` into the quarter-sine table at 0x82F2D920 --
sin(i*pi/1024) -- and `fsubs f0,1.0,f0`), i.e. 1 - sin(t*pi/2). `mi16PitchOutput <
E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH` is asserted (l.185, `extsh r9 ; cmpwi cr6,r9,3`) before the pitch
read. The PC used a linear 1 - t behind a radius > 0 guard of its own, returned early without a module,
and dropped the pitch-output assert.

Numeric: tests/FxEmitterProcessUpdate.cpp compiles the PRODUCTION ProcessUpdate body (plus the file's
anonymous namespace, which names the azimuth output) against a fixture effect whose mixer, voice and
Curve record what they are asked; StaticSoundEntity is the real BrnStaticSoundMap.h. --rev reads a b5
revision (the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxemitter_process_update.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

EFFECT_CPP = "src/GameSource/Sound/World/BrnEmitterEffect.cpp"
SHADOWED = ("src/SharedClasses/Sound/World/BrnStaticSoundMap.h",)
NUMERIC_CHECKS = 14


def names_block(source):
    """The file's anonymous namespace (E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH), or nothing."""
    match = re.search(r"\nnamespace\s*\n\{\n.*?\n\}\s*//\s*namespace\n", source, re.S)
    return match.group(0) if match else ""


def wiring(tree):
    source = tree.read(EFFECT_CPP)
    try:
        body = code_only(definition(source, "void EmitterEffect::ProcessUpdate()"))
    except ValueError:
        body = ""
    return [
        ("ProcessUpdate runs on after the \"lpModule\" assert (l.165): no early return, no radius > 0 guard",
         body != "" and re.search(r"\breturn\b", body) is None and
         re.search(r"lfRadius\s*>\s*0", body) is None),
        ("the fall-off is Curve::GetOutput(1 - t, E_ONE_MINUS_EQPWR) and the pitch output is asserted "
         "below E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH (= 3, `cmpwi cr6,r9,3` @0x826E6DB4)",
         re.search(r"Curve::GetOutput\s*\(\s*1\.0f\s*-\s*\w+\s*,\s*CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR\s*\)",
                   body) is not None and
         re.search(r"E_WORLD_EMITTER_EFFECT_OUTPUT_AZIMUTH\s*=\s*3\b", code_only(names_block(source))) is not None),
    ]


def numeric(tree):
    try:
        source = tree.read(EFFECT_CPP)
        body = definition(source, "void EmitterEffect::ProcessUpdate()")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    shadow = None
    if tree.rev is not None:
        shadow = {relative: tree.read(relative) for relative in SHADOWED}
    return compile_and_run(Path(__file__).with_name("FxEmitterProcessUpdate.cpp"),
                           "fxemitter_process_update_body.inc", body, "FxEmitterProcessUpdate", shadow=shadow,
                           extra_files={"fxemitter_process_update_names.inc": names_block(source)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxemitter_process_update", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
