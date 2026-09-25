"""FX-GATE (crash parity 2026-09-25): XboxMath::XMScalarSinCos (src/SDKs/XboxMath/XMScalarSinCos.h) is the XDK's
scalar sine / cosine, X360 0x821F0C08..0x821F0D74, bit for bit.

  NUMERIC -- tests/FxGateXMScalarSinCosData.h: 600 inputs (+-0, +-pi and its neighbours, +-2pi, huge, +-inf, NaN,
  400 in +-10, 100 in +-1000, 100 in the AftertouchCrash orbit's range), each the console's own words run whole on
  emu64 (scratch/CRASHPARITY_0922/fixes/FX-GATE.sincos/gen_atc_data.py, vmsum4fp128 as ROUNDING_RULE 1).
  --as-std runs the same rows through std::sin / std::cos: RED (the check has teeth). A revision without the header
  cannot build the test: RED.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_xmscalar_sincos.py [--rev <b5 rev>] [--as-std]
"""
from pathlib import Path
import argparse
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, compile_and_run, report

HEADER = "src/SDKs/XboxMath/XMScalarSinCos.h"
NUMERIC_CHECKS = 600


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--as-std", action="store_true", help="run std::sin / std::cos instead (expected RED)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    try:
        text = tree.read(HEADER)
    except OSError:
        text = ""
    wiring = [("src/SDKs/XboxMath/XMScalarSinCos.h defines XboxMath::XMScalarSinCos",
               "inline void XMScalarSinCos(float* lpfSin, float* lpfCos, float lfValue)" in text)]
    numeric = None
    if wiring[0][1]:
        numeric = compile_and_run(Path(__file__).with_name("FxGateXMScalarSinCos.cpp"), "fxgate_unused.inc", "",
                                  "FxGateXMScalarSinCos", extra_flags="/DFXGATE_AS_STD" if args.as_std else "",
                                  shadow={HEADER: text})
    else:
        print("NUMERIC: cannot build -- the header is missing")
    return report("run_fxgate_xmscalar_sincos", wiring, numeric, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
