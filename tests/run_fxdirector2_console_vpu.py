"""FX-DIRECTOR2 (crash parity 2026-09-25): the rig camera's SDK inlines, rounded as the console rounds them.

  BehaviourRig::Update @0x822427C0 inlines the RenderWare vpu Mult / InverseOfMatrixWithOrthonormal3x3 /
  TransformVector and a vmaddfp spring push. The console runs them as fused vmaddfp cascades in a fixed order (the
  campaign rounding rule, rule 3); the vendor SDK on the PC rounds every product and partial sum. The rig's sites now
  use src/GameSource/Director/Camera/Utils/BrnConsoleVpu.h, which reproduces the console bit for bit.

Numeric: tests/FxDirector2ConsoleVpu.cpp against FxDirector2ConsoleVpuGolden.h -- the rig Update's own windows
(0x82242D20..0x82242E64, 0x82242F90..0x8224301C, 0x82242C0C..0x82242C3C) run on a PPC/VMX128 emulator over 32 random
frames -- 128 bit-for-bit checks, plus four proving the table tells each console form from the vendor one.
Wiring: BehaviourRig.cpp's five sites call the ConsoleVpu forms.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector2_console_vpu.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, report

HEADER = "src/GameSource/Director/Camera/Utils/BrnConsoleVpu.h"
SINCOS_H = "src/SDKs/XboxMath/XMVectorSinCos.h"
RIG_CPP = "src/GameSource/Director/Camera/Behaviours/BehaviourRig.cpp"
NUMERIC_CHECKS = 32 * 4 + 4


def wiring(tree):
    rig = re.sub(r"\s+", "", code_only(tree.read(RIG_CPP)))
    yield ("BehaviourRig::Update pushes the frame along At with ONE vmaddfp128 (0x82242C34)",
           "lWork.Pos()=Utils::ConsoleVpu::MultiplyAdd(lWork.At(),mAccelSpring.GetLength(),lWork.Pos());" in rig)
    yield ("... inverts the working frame and brings the looked-at car and its velocity into it the console's way "
           "(0x82242D20..0x82242E60)",
           "Utils::ConsoleVpu::InverseOfMatrixWithOrthonormal3x3(lWork)" in rig
           and "Utils::ConsoleVpu::Mult(mLookingAtRef.GetTransform(lrInfo),lInverseWork)" in rig
           and "Utils::ConsoleVpu::TransformVector(lInverseWork," in rig)
    yield ("... composes the camera as rig x work the console's way (0x82242F90)",
           "lCameraTransform=Utils::ConsoleVpu::Mult(lRig,lWork);" in rig)


def numeric(tree):
    shadow = None
    if tree.rev is not None:
        texts = {relative: tree.read(relative) for relative in (HEADER, SINCOS_H)}
        if not all(texts.values()):
            print("NUMERIC: the revision has no " + " / ".join(r for r, t in texts.items() if not t))
            return None
        shadow = texts
    return compile_and_run(Path(__file__).with_name("FxDirector2ConsoleVpu.cpp"), "fxd2_vpu_unused.inc", "",
                           "FxDirector2ConsoleVpu", shadow=shadow)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector2_console_vpu", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
