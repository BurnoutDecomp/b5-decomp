"""FX-GATE (crash parity 2026-09-25): rw::math::vpu::SLerp @0x82216858 rewritten from the console's words, and the new
XboxMath::XMVectorATan @0x821F0A70 (src/SDKs/XboxMath/XMVectorATan.h) it needs.

  1. WIRING -- SLerp takes its rotation angle from the axis / angle query with XMVectorATan, its arc from the inlined
     XMVectorSinCos, and no longer calls std::sin or the vendor Normalize.
  2. NUMERIC -- tests/FxGateSLerp.cpp against tests/FxGateSLerpData.h: XMVectorATan on 600 inputs, and SLerp WHOLE on
     284 frame pairs, each row the console's function run on emu64 from its first word to its return with every
     callee interpreted (scratch/CRASHPARITY_0922/fixes/FX-GATE.slerp/gen_slerp_data.py); all 16 result lanes and the
     4 angle-out lanes. The revision's own matrix44affine_operation.h is compiled (shadowed); a revision without
     XMVectorATan.h cannot build the test. The last 38 rows are a yaw frame converging on an axis-aligned target (the
     heading spaces' 0.20 / 0.07 blends): |a|^2 from vmsum3fp128 @0x822165CC is a denormal the VMX flushes to 0 (the
     lerp arm); 8c12ed5a, which did not flush it, gives a NaN matrix or a wrong angle on 26 of them.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_slerp.py [--rev <b5 rev>] [--as-std]
      --as-std   the ATan rows through std::atan instead (shows the XDK routine is not the correctly rounded atan)
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

HEADER = "vendor/renderware/include/rw/math/vpu/matrix44affine_operation.h"
ATAN = "src/SDKs/XboxMath/XMVectorATan.h"
SLERP = "inline Matrix44Affine SLerp(const Matrix44Affine& lrFrom, const Matrix44Affine& lrTo,"
DATA = Path(__file__).with_name("FxGateSLerpData.h")


def numeric_total():
    text = DATA.read_text(encoding="utf-8")
    return len(re.findall(r"^\s*\{ 0x[0-9A-F]{8}, 0x[0-9A-F]{8} \},$", text, re.M)) + text.count('{ "')


def wiring(tree):
    header = tree.read(HEADER)
    try:
        body = code_only(definition(header.replace("\r\n", "\n"), SLERP))
    except ValueError:
        body = ""
    yield ("SLerp's axis / angle query takes XboxMath::XMVectorATan (bl 0x821F0A70 in sub_82216510)",
           "XboxMath::XMVectorATan(" in code_only(header) and "QueryRotation(" in body)
    yield ("SLerp's arc takes the inlined XboxMath::XMVectorSinCos (0x82216BB8..)", "XboxMath::XMVectorSinCos(" in body)
    yield ("SLerp has no std::sin and no vendor Normalize (vrsqrtefp + Newton steps, ROUNDING_RULE 5)",
           bool(body) and "std::sin" not in body and re.search(r"(?<![\w:])Normalize\s*\(", body) is None)
    yield ("src/SDKs/XboxMath/XMVectorATan.h exists", bool(tree.read(ATAN)))


def numeric(tree, as_std):
    header = tree.read(HEADER)
    atan = tree.read(ATAN)
    if not atan:
        print("NUMERIC: cannot build -- src/SDKs/XboxMath/XMVectorATan.h is not in this revision")
        return None
    shadow = {"src/rw/math/vpu/matrix44affine_operation.h": header, "src/SDKs/XboxMath/XMVectorATan.h": atan}
    return compile_and_run(Path(__file__).with_name("FxGateSLerp.cpp"), "fxgate_slerp_unused.inc", "", "FxGateSLerp",
                           extra_flags="/DFXGATE_ATAN_AS_STD" if as_std else "", shadow=shadow)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--as-std", action="store_true", help="run the ATan rows through std::atan")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_slerp", list(wiring(tree)), numeric(tree, args.as_std), numeric_total())


if __name__ == "__main__":
    sys.exit(main())
