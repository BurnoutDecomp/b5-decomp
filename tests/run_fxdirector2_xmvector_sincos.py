"""FX-DIRECTOR2 (crash parity 2026-09-25): XboxMath::XMVectorSinCos, the XDK sine / cosine the console inlines.

  The console never calls a sine: every caller inlines the XDK's XMVectorSinCos -- a 2pi range reduction (vmulfp128,
  vrfin, vnmsubfp) and the degree-23 / degree-22 Taylor polynomials from the tables at 0x82000BD0..0x82000C60, each
  term one fused vmaddfp. The PC had only std::sin / std::cos, which miss it by an ulp or more over much of the range
  (at pi the console's sine is -3.1e-7, not -8.7e-8). src/SDKs/XboxMath/XMVectorSinCos.h is the function as the
  console runs it (the campaign rounding rule, rules 3 and 4).

Numeric: tests/FxDirector2XMVectorSinCos.cpp against FxDirector2XMVectorSinCosGolden.h -- the console's own words
(CameraRig::Construct's inlined block, 0x8220B1B8..0x8220B3F0) run on a PPC/VMX128 emulator over 243 angles, bit for
bit -- plus one check that the table tells the polynomial from std::sin / std::cos.
--as-std builds the fixture with std::sin / std::cos as the function under test, to show the test fails on them.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector2_xmvector_sincos.py [--rev <b5 rev>] [--as-std]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, compile_and_run, report

HEADER = "src/SDKs/XboxMath/XMVectorSinCos.h"
NUMERIC_CHECKS = 243 + 1


def numeric(tree, as_std):
    header = tree.read(HEADER) if tree.rev is not None else None
    if tree.rev is not None and not header:
        print("NUMERIC: the revision has no " + HEADER)
        return None
    shadow = {HEADER: header} if tree.rev is not None else None
    return compile_and_run(Path(__file__).with_name("FxDirector2XMVectorSinCos.cpp"), "fxd2_sincos_unused.inc", "",
                           "FxDirector2XMVectorSinCos", shadow=shadow,
                           extra_flags="/DFXD2_SINCOS_AS_STD" if as_std else "")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--as-std", action="store_true", help="test std::sin / std::cos instead (must fail)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector2_xmvector_sincos", [], numeric(tree, args.as_std), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
