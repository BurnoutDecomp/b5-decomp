"""FX-GATE item 6 (crash parity 2026-09-25): XboxMath::XMVectorACos is the console's XMVectorACos (X360 0x821F0980).

The console's inverse cosine is the XDK math library's polynomial + one-Newton-step rsqrt pipeline, not a correctly
rounded acos: XMVectorACos(1) = 0x35000000, XMVectorACos(-1) = 0x40490FD9, and every |x| > 1 is NaN. The shared
header src/SDKs/XboxMath/XMVectorACos.h writes it operation for operation, with the coefficients read from the image
(0x82000C30 / 0x82000C40 / 0x82000C50 / 0x82000C60, 0x82001C40).

  1. WIRING -- every game call site the console routes through XMVectorACos calls XboxMath::XMVectorACos and no
     longer std::acos / acosf (the SITES table below: file, the call's console function and `bl` address).
  2. NUMERIC -- tests/FxGateXMVectorACos.cpp checks the header against the console's own words run on emu64
     (FxGateXMVectorACosData.h, 3324 rows), the landmarks, NaN for |x| > 1, and that std::acos is not the console.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxgate_xmvector_acos.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, report

HEADER = "src/SDKs/XboxMath/XMVectorACos.h"
NUMERIC_CHECKS = 3340

# (file, the console function and its `bl XMVectorACos`, the PC expression that must be the call)
SITES = [
    ("src/GameSource/World/AI/BrnAIUtils_Angles.cpp",
     "FindUnsignedAngleBetween2DVectors 0x82766B84", "XboxMath::XMVectorACos(lfDot)"),
    ("src/GameSource/GameState/ModeManager/GameModes/BrnOnlineStuntRunMode.cpp",
     "OnlineStuntRunMode::GetBestStartGridID 0x823319AC", "XboxMath::XMVectorACos(lfCosAngle)"),
]


def wiring(tree):
    for relative, console, call in SITES:
        text = tree.read(relative)
        code = code_only(text)
        squashed = re.sub(r"\s+", "", code)
        ok = (re.sub(r"\s+", "", call) in squashed and "std::acos(" not in code
              and not re.search(r"(?<![\w:])acosf?\s*\(", code))
        yield (f"{Path(relative).name}: {console} -> {call}, no std::acos / acosf left", ok)


def numeric(tree):
    header = tree.read(HEADER)
    if not header:
        print("NUMERIC: cannot build -- " + HEADER + " is missing at this revision")
        return None
    return compile_and_run(Path(__file__).with_name("FxGateXMVectorACos.cpp"), "unused.inc", "\n",
                           "FxGateXMVectorACos", shadow={HEADER: header})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxgate_xmvector_acos", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
