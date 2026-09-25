"""FX-AIBUZZ reviewer-G item (b) (crash parity 2026-09-25): PathLine's output write rounds once, like the console.

ARTIST writes the stage value with `fmadds f0, f1(GetOutput), f31(finish - start), f0(maStart[mnCurrentStage])`:
  PathLine<2>::Update @0x8268F2D0 -- fmadds @0x8268F428 (stfs mfCurrentValue +0x30)
  PathLine<3>::Update             -- fmadds @0x8268F178 (stfs mfCurrentValue +0x40)
The PC multiplied, rounded, then added (287f98da named both sites and left them). Only the output statement
changes; the rest of the two bodies stays as FX-TAILS-B verified it.

  1. WIRING -- both output statements are std::fmaf(Curve::GetOutput(...), range, start).
  2. NUMERIC -- tests/FxAiBuzzPathLineFma.cpp runs the PRODUCTION PathLine<2> AddStage / AddLinkedStage / Update, the
     generic PathLine<3> AddStage / Update / Update(dt, finish) and Curve::GetOutput on the real CgsSoundUtils.h
     struct: 10 double-rounding counterexamples (single stages, a linked stage, the two-argument update), each
     against the exactly rounded f * range + start, plus a power-of-two control and the assert count.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxaibuzz_path_line_fma.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, body_or_empty, compile_and_run, report
from run_fxvoicepool_curve import optional, LAST_ELEMENT, GET_OUTPUT, TABLE, READ_HELPER, UTILS_CPP

TWO = ["template <>\ns32 PathLine<2>::AddStage(",
       "template <>\ns32 PathLine<2>::AddLinkedStage(",
       "template <>\nvoid PathLine<2>::Update(f32 lfDeltaTime)"]
THREE = ["template <u32 tuNumStages>\ns32 PathLine<tuNumStages>::AddStage(",
         "template <u32 tuNumStages>\nvoid PathLine<tuNumStages>::Update(f32 lfDeltaTime, f32 lfFinish)",
         "template <u32 tuNumStages>\nvoid PathLine<tuNumStages>::Update(f32 lfDeltaTime)"]
STAGE_CONSTANTS = re.compile(r"static const f32 KF_(?:STAGE_LENGTH_SCALE|MIN_STAGE_LENGTH)\s*=[^;]+;")
NUMERIC_CHECKS = 12


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def wiring(tree):
    source = tree.read(UTILS_CPP).replace("\r\n", "\n")
    two = squash(body_or_empty(source, TWO[2]))
    three = squash(body_or_empty(source, THREE[2]))
    return [
        ("PathLine<2>::Update writes std::fmaf(GetOutput, range, maStart[mnCurrentStage]) (fmadds @0x8268F428)",
         "mfCurrentValue=std::fmaf(Curve::GetOutput(lfFraction,maCurveTypes[lnStage]),lfRange,"
         "maStart[mnCurrentStage]);" in two),
        ("PathLine<3>::Update writes std::fmaf(GetOutput, finish - start, start) (fmadds @0x8268F178)",
         "mfCurrentValue=std::fmaf(Curve::GetOutput(lfFraction,maCurveTypes[lnStage]),"
         "maFinish[lnStage]-maStart[lnStage],maStart[lnStage]);" in three),
    ]


def numeric(tree):
    utils = tree.read(UTILS_CPP).replace("\r\n", "\n")
    try:
        path_bodies = [definition(utils, signature) for signature in TWO + THREE]
        get_output = definition(utils, GET_OUTPUT)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    constants = STAGE_CONSTANTS.findall(code_only(utils))
    if len(constants) != 2:
        print("NUMERIC: cannot build -- the two PathLine stage-length constants are absent")
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
    return compile_and_run(Path(__file__).with_name("FxAiBuzzPathLineFma.cpp"), "fxaibuzz_path_line_bodies.inc",
                           "\n".join(constants) + "\n\n" + "\n\n".join(path_bodies) + "\n", "FxAiBuzzPathLineFma",
                           extra_files={"fxaibuzz_path_curve_bodies.inc": "\n\n".join(pieces)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxaibuzz_path_line_fma", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
