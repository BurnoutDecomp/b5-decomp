"""FX-TAILS-B (crash parity 2026-09-24, item 4): CgsSound::Utils::PathLine<2>::Update @0x8268F2D0.

ARTIST clamps the stage position f = t / length with `fneg ; fsel` + `fsubs ; fsel` (0x8268F3F8..0x8268F410,
rwmath's Max(0, x) / Min(1, x) forms): a NaN comes out as 1.0, so a NaN step or length -- or 0 / 0 on a
zero-length stage, right after the "maLength[mnCurrentStage]" tripwire -- reads the curve at its end
(the stage's finish level). The PC clamped with two ifs, which let the NaN through to GetOutput: NaN out
of E_LINEAR, entry 0 (the stage's start) out of E_POWER. Callers: RoadnoiseEffect::UpdateParams,
AISkidEffect::UpdateParams, SweetenersEffect::UpdateCarStart / UpdateParams.

Numeric: tests/FxTailsBPathLine.cpp compiles the PRODUCTION PathLine<2>::AddStage / AddLinkedStage / Update
and Curve::GetOutput (+ table, constant, read helper) against the real CgsSoundUtils.h PathLine. --rev reads
a b5 revision (the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxtailsb_path_line.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report
from run_fxvoicepool_curve import optional, LAST_ELEMENT, GET_OUTPUT, TABLE, READ_HELPER, UTILS_CPP

UPDATE = "template <>\nvoid PathLine<2>::Update(f32 lfDeltaTime)"
ADD_STAGE = "template <>\ns32 PathLine<2>::AddStage("
ADD_LINKED = "template <>\ns32 PathLine<2>::AddLinkedStage("
NUMERIC_CHECKS = 10


def wiring(tree):
    body = body_or_empty(tree.read(UTILS_CPP), UPDATE)
    return [
        ("PathLine<2>::Update clamps the stage position with the fsel pair (rw::math::fpu::Clamp, "
         "0x8268F3F8..0x8268F410: a NaN -> 1.0), not two ifs",
         re.search(r"rw::math::fpu::Clamp\(\s*mfElapsedTime\s*/\s*maLength\[\s*lnStage\s*\]\s*,\s*0\.0f\s*,\s*1\.0f\s*\)",
                   body) is not None and re.search(r"if\s*\(\s*lfFraction\s*[<>]", body) is None),
    ]


def numeric(tree):
    utils = tree.read(UTILS_CPP)
    try:
        path_bodies = "\n\n".join(definition(utils, signature) for signature in (ADD_STAGE, ADD_LINKED, UPDATE))
        get_output = definition(utils, GET_OUTPUT)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    pieces = []
    constant = LAST_ELEMENT.search(code_only(utils))
    if constant:
        pieces.append(constant.group(0))
    table = optional(utils, TABLE)
    if table:
        pieces.append(table + ";")
    helper = optional(utils, READ_HELPER)
    if helper:
        pieces.append(helper)
    pieces.append(get_output)
    return compile_and_run(Path(__file__).with_name("FxTailsBPathLine.cpp"), "fxtailsb_path_line_bodies.inc",
                           path_bodies, "FxTailsBPathLine",
                           extra_files={"fxtailsb_path_curve_bodies.inc": "\n\n".join(pieces)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxtailsb_path_line", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
