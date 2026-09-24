"""FX-EMITTER (crash parity 2026-09-24): EmitterState::IsAttachedToThis's position tolerance.

EmitterStateManager::UpdateParams asks every live EmitterState whether a queried StaticSoundEntity is
the one it is already attached to (a "yes" suppresses a second attach). ARTIST @0x826BADA0 compares
|test.xyz - mine.xyz| lane by lane against a splatted rodata float (`lvlx v0, unk_820AA0E0` /
`vspltw128 v127, v0, 0` / `vcmpgtfp128.`; the w lane is masked by `vrlimi128 v13, v0, 1, 1`), then the
packed types (`srwi 16` both sides). 0x820AA0E0 holds 0x37800000 = 2^-16 = 1.52587890625e-05
(tools/re/x360rd.py), rwmath's SMALL_FLOAT. The PC carried an invented 0.01f placeholder
("UNRECOVERED"), 655x the console tolerance: two same-type entities within 1 cm merged.

Numeric: tests/FxEmitterStateAttached.cpp compiles the PRODUCTION IsAttachedToThis body and the file's
anonymous-namespace constant block against a fixture EmitterState holding a REAL StaticSoundEntity.
--rev reads a b5 revision (the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxemitter_state_attached.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

STATE_CPP = "src/GameSource/Sound/Module/LogicModule/BrnEmitterState.cpp"
SHADOWED = ("src/SharedClasses/Sound/World/BrnStaticSoundMap.h",)
NUMERIC_CHECKS = 9


def constants_block(source):
    match = re.search(r"\nnamespace\s*\n\{\n.*?\n\}\s*//\s*namespace\n", source, re.S)
    if match is None:
        raise ValueError("the anonymous-namespace constant block")
    return match.group(0)


def wiring(tree):
    raw = tree.read(STATE_CPP)
    block = code_only(constants_block_or_empty(raw))
    return [
        ("the position tolerance is the image's 0x820AA0E0 = 0x37800000 = 1.52587890625e-05f (2^-16), "
         "no 0.01f placeholder left in the constant block",
         re.search(r"const\s+f32\s+KF_ATTACH_POSITION_EPSILON\s*=\s*1\.52587890625e-05f\s*;", block) is not None
         and "0.01f" not in block),
    ]


def constants_block_or_empty(source):
    try:
        return constants_block(source)
    except ValueError:
        return ""


def numeric(tree):
    try:
        source = tree.read(STATE_CPP)
        constants = constants_block(source)
        body = definition(source, "bool EmitterState::IsAttachedToThis(void* lpvTestAttachment)")
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    shadow = None
    if tree.rev is not None:
        shadow = {relative: tree.read(relative) for relative in SHADOWED}
    return compile_and_run(Path(__file__).with_name("FxEmitterStateAttached.cpp"),
                           "fxemitter_state_body.inc", body, "FxEmitterStateAttached", shadow=shadow,
                           extra_files={"fxemitter_state_constants.inc": constants})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxemitter_state_attached", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
