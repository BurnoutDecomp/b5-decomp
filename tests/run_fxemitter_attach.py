"""FX-EMITTER (crash parity 2026-09-24): the world-emitter attach gate -- EmitterEffect::Attach.

The dev assert `luEmitter < static_cast< uint32_t >( lWorldEmitters.mNumWorldEmitters() )`
(BrnEmitterEffect.cpp) paused six lanes' live runs. Against ARTIST EmitterEffect::Attach @0x826F5740:
  * luEmitter is the entity's packed W lane read as ONE u32, high half (`lfs f0,0xC(r29)` / `stfs` /
    `lwz` / `srwi r29,r11,16`, 0x826F57E4..0x826F57F8);
  * the assert (0x826F5800/04) and the gate (0x826F5830/38) both read `lwz r11,0x4B8(r11)` off the
    list's mpAttributeData -- the SCALAR attribute mNumWorldEmitters (ARTIST embedded AttribSys schema:
    worldemitterlist layout 1216, mWorldEmitters RefSpec[50] @+0, mNumWorldEmitters Int32 @+1208).
    The PC read Num_mWorldEmitters, the array header's element count. In the shipped
    SOUND/BURNOUTGLOBALDATA.BIN the header says 50 and the scalar says 38 (slots 38..49 are empty
    RefSpecs), so the PC admitted types 38..49 into empty slots without the assert.
The live trigger itself was DATA: build/game's TRK_UNIT*_GR.BNDL StaticSoundMaps were converted before
parent b81ce4f9 fixed the porter's W-lane flip, so every emitter read its radius as its type (see
scratch/CRASHPARITY_0922/fixes/FX-EMITTER.md); the console gate rightly asserts on such data.

Numeric: tests/FxEmitterAttach.cpp compiles the PRODUCTION Attach body with the REAL worldemitterlist.h,
worldemitter.h and BrnStaticSoundMap.h (shadowed with the revision's text under --rev) over a list
layout built like the shipped vault collection. --rev reads a b5 revision (the RED side: <fix>~1).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxemitter_attach.py [--rev <rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

EFFECT_CPP = "src/GameSource/Sound/World/BrnEmitterEffect.cpp"
LIST_H = "src/GameSource/AttribSys/Generated/classes/worldemitterlist.h"
SHADOWED = (LIST_H,
            "src/GameSource/AttribSys/Generated/classes/worldemitter.h",
            "src/SharedClasses/Sound/World/BrnStaticSoundMap.h")
NUMERIC_CHECKS = 21


def attach_body(source):
    return definition(source, "bool EmitterEffect::Attach()")


def strings_blanked(text):
    """The assert's message literal spells `lWorldEmitters.mNumWorldEmitters()` too -- count code only."""
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', text)


def wiring(tree):
    try:
        body = strings_blanked(code_only(attach_body(tree.read(EFFECT_CPP))))
    except ValueError:
        body = ""
    header = code_only(tree.read(LIST_H))
    scalar = r"static_cast\s*<\s*u32\s*>\s*\(\s*lWorldEmitters\s*\.\s*mNumWorldEmitters\s*\(\s*\)\s*\)"
    assert_on_scalar = re.search(r"CGS_ASSERT\s*\(\s*luEmitter\s*<\s*" + scalar, body) is not None
    gate_on_scalar = re.search(r"if\s*\(\s*luEmitter\s*<\s*" + scalar + r"\s*\)", body) is not None
    # the array length may only be PRINTED (the [DIAG] refusal line), never compared against
    compares_array = re.search(r"luEmitter\s*(<(?!<)|>=)\s*[^;{]*Num_mWorldEmitters", body) is not None
    return [
        ("Attach asserts AND gates on the scalar mNumWorldEmitters() (0x826F5804 / 0x826F5838), "
         "never on the array length Num_mWorldEmitters()",
         assert_on_scalar and gate_on_scalar and not compares_array),
        ("worldemitterlist.h: mNumWorldEmitters() reads _LayoutStruct::mNumWorldEmitters, pinned at "
         "+0x4B8 in a 1216-byte layout (ARTIST schema)",
         re.search(r"const\s+s32\s*&\s*mNumWorldEmitters\s*\(\s*\)\s*const", header) is not None and
         re.search(r"offsetof\s*\(\s*_LayoutStruct\s*,\s*mNumWorldEmitters\s*\)\s*==\s*0x4B8", header) is not None and
         re.search(r"sizeof\s*\(\s*_LayoutStruct\s*\)\s*==\s*1216", header) is not None),
    ]


def numeric(tree):
    try:
        body = attach_body(tree.read(EFFECT_CPP))
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    shadow = None
    if tree.rev is not None:
        shadow = {relative: tree.read(relative) for relative in SHADOWED}
    return compile_and_run(Path(__file__).with_name("FxEmitterAttach.cpp"),
                           "fxemitter_attach_body.inc", body, "FxEmitterAttach", shadow=shadow)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision (the RED side: <fix>~1)")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxemitter_attach", wiring(tree), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
