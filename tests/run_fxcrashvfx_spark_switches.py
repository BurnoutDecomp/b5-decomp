"""FX-CRASHVFX (crash parity 2026-09-24): the two spark switches -- HandleSparkContacts' byte_82CDB40C and
HandleRaceCarRaceCarSparks' byte_82CDB40D, the function-local `static bool8_t lbDisableThisEffect` of each (DecFIGS
DWARF EffectsModule.cpp:1455 / :1580).

Both bytes are INITIALISED .data holding 0x01 in the ARTIST image (IDA flag word 0x00009501, FF_IVL set) and
nothing writes them (findinit.py: one reader each; no pointer to either anywhere in the image). The Remaster agrees:
BurnoutPR.exe's ProcessHingedPartContacts (sub_981D70) and ProcessRaceCarContacts (sub_9823E0) keep their gates and
call neither function. The tree read byte_82CDB40C as a .bss FALSE (b5 6b2d999c), so the PC drew along-line
"scrape sparks" off detached and hinged parts that the console never draws.

  1. WIRING -- both switches are the image's TRUE, cited by address and flag word.
  2. NUMERIC -- tests/FxCrashVfxSparkSwitches.cpp calls the PRODUCTION HandleSparkContacts with a sparking contact
     and compares the result with the console's own words (tests/FxCrashVfxSparkSwitchesData.h, written by
     scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_switch_data.py): no record, no random draw.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_spark_switches.py [--rev <b5 rev>]
                                                                                 [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, Tree, code_only, compile_and_run, definition, report

EFFECTS_CPP = "src/GameSource/Effects/EffectsModule.cpp"
SHADOW_HEADERS = ("src/GameSource/Effects/Particles/BrnParticleModuleIO_EventTypes.h",)
RANDOM_CPP = REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"
SIGNATURE = "void EffectsModule::HandleSparkContacts("
NUMERIC_CHECKS = 5


class RootTree(Tree):
    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = Path(root) if root else None

    def read(self, relative):
        if self.root is not None and (self.root / relative).exists():
            return (self.root / relative).read_text(encoding="utf-8-sig")
        try:
            return super().read(relative)
        except FileNotFoundError:
            return ""


def wiring(tree):
    effects = tree.read(EFFECTS_CPP).replace("\r\n", "\n")
    try:
        body = code_only(definition(effects, SIGNATURE))
    except ValueError:
        body = ""
    yield ("HandleSparkContacts' switch is the image's TRUE (byte_82CDB40C == 0x01): the console never posts it",
           re.search(r"static const bool sbSparkContactsDisabled\s*=\s*true;", body) is not None
           and "byte_82CDB40C" in effects and "0x00009501" in effects)
    yield ("HandleRaceCarRaceCarSparks' switch is the image's TRUE (byte_82CDB40D == 0x01)",
           re.search(r"const bool KB_RACE_CAR_SPARKS_DISABLED\s*=\s*true;", code_only(effects)) is not None)


def numeric(tree):
    effects = tree.read(EFFECTS_CPP).replace("\r\n", "\n")
    try:
        body = definition(effects, SIGNATURE)
    except ValueError:
        print("NUMERIC: cannot build -- HandleSparkContacts is absent")
        return None
    consts = []
    for name in sorted(set(re.findall(r"\bK[A-Z]{1,2}_[A-Z0-9_]+\b", code_only(body)))):
        match = re.search(r"^[ \t]*const\s+[\w:]+\s+%s\s*=\s*[^;]+;" % re.escape(name), effects, flags=re.M)
        if match:
            consts.append(match.group(0).strip())
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    return compile_and_run(Path(__file__).with_name("FxCrashVfxSparkSwitches.cpp"), "fxcrashvfx_switch_body.inc",
                           body + "\n", "FxCrashVfxSparkSwitches", shadow=shadow, extra_sources=(RANDOM_CPP,),
                           extra_files={"fxcrashvfx_switch_consts.inc": "\n".join(consts) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_spark_switches", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
