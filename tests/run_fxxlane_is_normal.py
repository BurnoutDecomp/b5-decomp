"""Regression for BrnMath::IsNormal(Vector3) @0x822B1CF8 and IsNormal(Vector2) (sub_8276AC48)
(crash parity G07-D1 leftover, FX-XLANE).

Both console bodies return TRUE unless |mag - 1.0| > flt_82002138 (x360rd 0x3C23D70A = 0.01): the
cntlzw after vcmpgtfp makes a NaN (or infinite) vector read as normal. The PC Vector3 body returned
fabs(m - 1) <= eps (FALSE for NaN / inf), and the Vector2 overload had no body at all, which left
TestCarHNG's :2216 assert flagged.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxxlane_is_normal.py [--pre-fix <b5 rev>]
The pre-fix file (b5 c4d0d14f) fails 7/11 checks (NaN + inf polarity, and five for the missing
Vector2 body); the fix passes 11/11.
"""
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fxdeformlat_common import anonymous_namespace, build_and_run, definition, pre_fix_rev, read

TU = "src/GameSource/Math/BrnMathUtils.cpp"
V2_SIGNATURE = "    bool IsNormal(Vector2 lVector)"


def main():
    rev = pre_fix_rev(sys.argv)
    text = read(TU, rev)
    parts = []
    if V2_SIGNATURE in text:
        parts.append("#define FXXLANE_HAS_IS_NORMAL_VECTOR2 1")
    parts.append("namespace BrnMath {")
    if "UnitLengthMagnitude" in text:            # the fix's file-local helper namespace
        parts.append(anonymous_namespace(text))
    parts.append(definition(text, "    bool IsNormal(Vector3 lVector)"))
    if V2_SIGNATURE in text:
        parts.append(definition(text, V2_SIGNATURE))
    parts.append("}")
    rc = build_and_run(Path(__file__).with_name("FxXlaneIsNormal.cpp"), {"methods.inc": "\n".join(parts)},
                       "fxxlane_is_normal")
    sys.exit(rc)


if __name__ == "__main__":
    main()
