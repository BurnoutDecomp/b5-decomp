"""FX-CRASHVFX (crash parity 2026-09-25, item 6): THE PROP-STRIKE VFX -- BrnEffects::PropCollisions::Initialise
@0x822937A8, BuildPropToMaterialTable @0x82288640, UpdateLocatorVfx @0x822993A0 and VFXRuntimeMaterialLef::
TriggerLocators @0x82299168 / CreateEffect @0x822936F0.

What a player sees on the console when a prop is struck near the camera: the LION effects baked on the prop's
material locators (sparks off a lamp post, splinters off a fence), placed along the car's direction of travel. The
prop VFX locator queue PropEntityModule posts reaches the effects input every frame; the console's EffectsModule::
Update hands it to UpdateLocatorVfx. Before this work PropCollisions had no type, the queue went nowhere, and the one
body there was (TriggerLocators) ran all four rows of the effect's frame through the prop transform.

  1. WIRING -- the bodies exist and run unannounced; their literals are the image's, each cited; TriggerLocators
     carries only the locator's position through the prop transform (three fused rows) and refines vrsqrtefp.
  2. NUMERIC -- tests/FxCrashVfxPropCollisions.cpp compiles the PRODUCTION PropCollisions.cpp whole and compares the
     table Initialise builds, the LION calls, every placed effect's slot, the effect ring and mRandom, bit for bit,
     with 0x822937A8 / 0x822993A0 run on emu64 (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_propcoll_data.py;
     2 Initialise cases x 3 checks, 14 UpdateLocatorVfx cases x 4 checks).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_prop_collisions.py
                                                                   [--rev <b5 rev>] [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

PROP_CPP = "src/GameSource/Effects/Props/PropCollisions.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/Props/PropCollisions.h",
                  "src/GameSource/Effects/Particles/ParticleModule.h")
EXTRA_SOURCES = (REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp",
                 REPO / "src/GameShared/GameClasses/System/Resource/CgsResourcePtr.cpp",
                 REPO / "src/GameShared/GameClasses/System/Resource/CgsBaseResourcePtr.cpp")

NUMERIC_CHECKS = 2 * 3 + 14 * 4


class RootTree(Tree):
    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = Path(root) if root else None

    def read(self, relative):
        if self.root is not None and (self.root / relative).exists():
            return (self.root / relative).read_text(encoding="utf-8-sig").replace("\r\n", "\n")
        try:
            return super().read(relative).replace("\r\n", "\n")
        except FileNotFoundError:
            return ""


def body(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def wiring(tree):
    source = tree.read(PROP_CPP)
    update = body(source, "void PropCollisions::UpdateLocatorVfx(")
    initialise = body(source, "void PropCollisions::Initialise(")
    build = body(source, "void PropCollisions::BuildPropToMaterialTable(")
    yield ("PropCollisions has its bodies (Initialise, BuildPropToMaterialTable, UpdateLocatorVfx), none announcing: "
           "the table from the collection and the physics data, the locator events into TriggerLocators",
           update != "" and initialise != "" and build != ""
           and "LogNotReconstructed" not in update + initialise + build
           and "BuildPropToMaterialTable();" in initialise and "mRandom.Construct();" in initialise
           and "VFXRuntimeMaterialLef::Initialise();" in initialise
           and "VFXRuntimeMaterialLef::TriggerLocators(" in update and "ContactVisible(" in update
           and "GetResourceId()" in build)
    consts = code_only(source)
    wanted = (("KF_PROP_VFX_VISIBLE_DISTANCE_SQR", "2500.0f", "flt_8200D510"),
              ("KF_PROP_VFX_SMASH_SPEED_MPH", "50.0f", "flt_820138DC"))
    yield ("the literals are the image's, each cited by address (flt_8200D510 2500 = 50 m squared, flt_820138DC 50 mph)",
           all(re.search(r"const f32 %s\s*=\s*%s;" % (name, re.escape(value)), consts) for name, value, _ in wanted)
           and all(re.search(r"const f32 %s\s*=\s*%s;\s*//\s*%s" % (name, re.escape(value), address), source)
                   for name, value, address in wanted))
    trigger = body(source, "void VFXRuntimeMaterialLef::TriggerLocators(")
    yield ("TriggerLocators carries only the locator's position through the prop transform (three fused rows) and "
           "stores the velocity basis as it is, refined rsqrt, no Normalize / TransformVector",
           trigger != "" and "TransformVector(" not in trigger and "TransformPoint(" not in trigger
           and "Normalize(" not in trigger and trigger.count("MaddSplat4(lPropTransform.") == 3
           and trigger.count("RefinedRsqrt(Dot3(") == 2 and "SetVelocity(" in trigger
           and "SetStateBlendFactor(lRandom.RandomFloat())" in trigger)


def numeric(tree):
    source = tree.read(PROP_CPP)
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    return compile_and_run(Path(__file__).with_name("FxCrashVfxPropCollisions.cpp"), "fxcrashvfx_propcoll_tu.inc",
                           source + "\n", "FxCrashVfxPropCollisions", shadow=shadow, extra_sources=EXTRA_SOURCES)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_prop_collisions", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
