"""Structural regression for BrnSound::Vehicles::Engines::SweetenersEffect::Attach @0x826FD7A0
(crash parity FOLLOWUPS 12, FX-XLANE). Reads the production body from BrnSweetenersEffect.cpp
(comments and string literals stripped, so a comment can never satisfy a check).

Console facts checked (ARTIST words):
  - the module RNG is stepped exactly TWICE (mulld 0x826FD8B4 / 0x826FD8D4), draw 1 -> the HIGH word,
    draw 2 -> the LOW word, and nothing reseeds the effect's own mRandomGenerator;
  - SqaureWave::Construct(seed, r5 = up (flt_82001CC0 0.0, flt_82002138 0.01),
                                r6 = down (flt_82F2CD50 0.025, flt_82F2CD4C 0.15))  (0x826FD8E8;
    the callee stores r5 -> mUpTimeWindow 0x826AB840, r6 -> mDownTimeWindow 0x826AB848);
  - mfDelayToBang is UPDATED to -1.0 (previous = current, 0x826FD814..81C); mfDeleayToVFXFire is
    FLUSHED to -1.0 (0x826FD82C/30); meRaceCarEngineState is not written.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxxlane_sweeteners_attach.py [--pre-fix <b5 rev>]
The pre-fix body (b5 f559c8bd) fails 6/8 checks; the fix passes 8/8.
"""
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fxdeformlat_common import definition, pre_fix_rev, read

TU = "src/GameSource/Sound/Vehicles/Engines/BrnSweetenersEffect.cpp"


def code_only(text):
    text = re.sub(r'/\*[\s\S]*?\*/', ' ', text)
    text = re.sub(r'//[^\n]*', ' ', text)
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', text)


def main():
    rev = pre_fix_rev(sys.argv)
    body = code_only(definition(read(TU, rev), "bool SweetenersEffect::Attach()"))
    statements = [s for s in body.split(';')]
    draw_statements = [s for s in statements if 'RandomUInt' in s]
    construct = re.search(r'mPopsSquareWave\s*\.\s*Construct\s*\(([\s\S]*?)\)\s*;', body)
    construct_args = construct.group(1) if construct else ''
    up = re.search(r'MinMax\s*\(\s*0\.0f\s*,\s*0\.01f\s*\)', construct_args)
    down = re.search(r'MinMax\s*\(\s*0\.025f\s*,\s*0\.15f\s*\)', construct_args)

    checks = [
        ("the module RNG is stepped exactly twice (0x826FD8B4 / 0x826FD8D4)",
         body.count('RandomUInt') == 2),
        ("no SetSeed of the effect's own mRandomGenerator (the console has none)",
         'SetSeed' not in body),
        ("the two draws are sequenced (one RandomUInt per statement) and the first is the high word",
         len(draw_statements) == 2 and all(s.count('RandomUInt') == 1 for s in draw_statements)
         and '<< 32' in construct_args),
        ("square-wave windows: up (0.0, 0.01) then down (0.025, 0.15) (flt_82001CC0/82002138, 82F2CD50/82F2CD4C)",
         bool(up and down and up.start() < down.start())),
        ("mfDelayToBang is Update(-1.0f)d, not flushed (0x826FD814..81C)",
         re.search(r'mfDelayToBang\s*\.\s*Update\s*\(\s*-1\.0f\s*\)', body) is not None
         and 'mfDelayToBang.Flush' not in body.replace(' ', '')),
        ("meRaceCarEngineState is not written by Attach (no store to +0x1C0/+0x1C4)",
         re.search(r'meRaceCarEngineState\s*\.\s*(Flush|Update)|meRaceCarEngineState\s*=', body) is None),
        ("mfDeleayToVFXFire is Flush(-1.0f)ed (0x826FD82C/30; control)",
         re.search(r'mfDeleayToVFXFire\s*\.\s*Flush\s*\(\s*-1\.0f\s*\)', body) is not None),
        ("mFadeOutEngine.Initialize(1.0, 1.0, 0.0, linear) kept (0x826FD838; control)",
         re.search(r'mFadeOutEngine\s*\.\s*Initialize\s*\(\s*1\.0f\s*,\s*1\.0f\s*,\s*0\.0f', body) is not None),
    ]
    failures = 0
    for name, passed in checks:
        if not passed:
            failures += 1
            print("FAIL: " + name)
    print(f"run_fxxlane_sweeteners_attach: {len(checks)} checks, {failures} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
