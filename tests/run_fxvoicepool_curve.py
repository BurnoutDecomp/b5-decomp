"""FX-VOICEPOOL (crash parity 2026-09-24), third piece: CgsSound::Utils::Curve::GetOutput @0x82689698
(DWARF CgsSoundUtils.cpp:159) -- the equal-power curve under every InterpolateLine / PathLine /
Slope sound ramp and the music, stream and world-emitter fall-offs.

Against ARTIST:
  * the table is DATA: gafArraySinTable[513] (DWARF cpp:85) at .data 0x82F2D920, entry i the
    5-decimal literal of sin(i * 3.14159 / 1024) (entries 511 and 512 both 1.0), no CRT writer.
    The PC derived sin(i * pi / 1022): 1.1e-3 off at entry 277, equal at only 2 of 512 entries.
  * E_POWER: fmuls x * -511 (flt_820AD414), fctiwz, `slwi 2 ; subf` -- entry trunc(511 x).
  * E_ONE_MINUS_EQPWR does its OWN lookup (0x82689780..0x826897B0): fmsubs x * 511 - 511
    (flt_820AA7B0) in one rounding, fctiwz, 1 - entry trunc(511 (1 - x)). The PC called
    GetOutput(1 - x, E_POWER), whose extra rounding reads a neighbouring entry for 16928 of the
    floats in [0, 1]; an unfused x * 511 - 511 would still miss 176.
  * the assert fires on `bgt 1.0` / `!bge 0.0` only (a NaN passes); a NaN index wraps to entry 0.

Numeric: tests/FxVoicepoolCurve.cpp compiles the PRODUCTION GetOutput (plus the table, its constant
and the read helper where the revision has them) against the image's table words. --rev reads a b5
revision (the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxvoicepool_curve.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

UTILS_CPP = "src/GameShared/GameClasses/Sound/CgsSoundUtils.cpp"
NUMERIC_CHECKS = 24
GET_OUTPUT = "f32 Curve::GetOutput("
TABLE = "f32 gafArraySinTable[513]"
READ_HELPER = "static f32 ReadSinTable("
LAST_ELEMENT = re.compile(r"static const f32 KF_LAST_ELEMENT_IN_ARRAY\s*=\s*[^;]*;")


def optional(source, signature):
    try:
        return definition(source, signature)
    except ValueError:
        return ""


def arm(body, label):
    """The code of one `case <label>:` arm, up to the next case or default label."""
    code = code_only(body)
    match = re.search(r"\bcase\s+" + label + r"\s*:", code)
    if match is None:
        return ""
    rest = code[match.end():]
    following = re.search(r"\bcase\s+\w+\s*:|\bdefault\s*:", rest)
    return rest[:following.start()] if following else rest


def wiring(tree):
    source = tree.read(UTILS_CPP)
    body = optional(source, GET_OUTPUT)
    one_minus = arm(body, "E_ONE_MINUS_EQPWR")
    power = arm(body, "E_POWER")
    return [
        ("E_ONE_MINUS_EQPWR does its own fused lookup (fmsubs 0x82689790), not a call into E_POWER",
         bool(one_minus) and "GetOutput(" not in one_minus and "fma(" in one_minus
         and "ReadSinTable(" in one_minus),
        ("E_POWER reads the carried image table gafArraySinTable[513] (0x82F2D920); no sine is derived",
         TABLE in code_only(source) and "ReadSinTable(" in power
         and re.search(r"\bsinf?\s*\(", code_only(body)) is None),
    ]


def numeric(tree):
    source = tree.read(UTILS_CPP)
    try:
        body = definition(source, GET_OUTPUT)
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    pieces, flags = [], ""
    constant = LAST_ELEMENT.search(code_only(source))
    if constant:
        pieces.append(constant.group(0))
    table = optional(source, TABLE)
    if table:
        pieces.append(table + ";")
        flags = "/DFXVP_HAS_SIN_TABLE"
    helper = optional(source, READ_HELPER)
    if helper:
        pieces.append(helper)
    pieces.append(body)
    return compile_and_run(Path(__file__).with_name("FxVoicepoolCurve.cpp"),
                           "fxvoicepool_curve_bodies.inc", "\n\n".join(pieces), "FxVoicepoolCurve",
                           extra_flags=flags)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxvoicepool_curve", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
