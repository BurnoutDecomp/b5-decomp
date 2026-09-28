"""OWNERLIST 2026-09-27, lane L5 MENUS: the crash-nav / pause camera's distance gates.

  BrnDirector::ArbStateCrashNav::Update @0x8226DC98 turns the fly-by about when the squared camera-to-car
  distance exceeds unk_82FAAAF0 (case 3, @0x8226DE64) and brings it back in when unk_82FAA990 exceeds it
  (case 4, @0x8226E040). Both are .bss dynamic-init vectors -- the image reads 0 there by definition -- and the
  PC carried a 1.0 "undumped" placeholder for each. tools/re/findinit.py names their CRT thunks
  (0x82C48488..0x82C484AC, 0x82C484B0..0x82C484D4), which splat flt_8200D518 == 160000.0 and
  flt_8200D51C == 122500.0. The squared distance is vsubfp + vmsum3fp128 (ROUNDING_RULE.md rule 1).

Numeric: tests/MenusCrashNavGates.cpp compiles the extracted production constants and
PlayerToCameraDistanceSquared against stand-ins carrying the production member names.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_menus_crashnav_gates.py [--rev <b5 rev>]
        [--src-root <dir holding src/...>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

CRASHNAV_CPP = "src/GameSource/Director/Arbitrator/States/BrnArbStateCrashNav.cpp"
DISTANCE = "inline f32 PlayerToCameraDistanceSquared("
CONSTANTS = ("SF_CRASHNAV_BLURRINESS", "KF_MAX_CRASH_NAV_PIC_PARADISE_OUTER_DISTANCE_SQ",
             "KF_MAX_CRASH_NAV_PIC_PARADISE_INNER_DISTANCE_SQ")
NUMERIC_CHECKS = 12


class RootTree(Tree):
    """A b5 source tree rooted somewhere else (e.g. a lane's shadow mirror): <root>/src/..."""

    def __init__(self, root):
        super().__init__(None)
        self.root = Path(root)

    def read(self, relative):
        path = self.root / relative
        if not path.exists():
            return super().read(relative)
        text = path.read_text(encoding="utf-8-sig")
        return text


def production(tree):
    source = tree.read(CRASHNAV_CPP)
    lines = []
    for name in CONSTANTS:
        match = re.search(r"const\s+f32\s+" + name + r"\s*=\s*([^;]+);", source)
        if match is None:
            return None, f"constant {name} not found"
        lines.append(f"const f32 {name} = {match.group(1).strip()};")
    try:
        function = definition(source, DISTANCE)
    except ValueError:
        return None, "PlayerToCameraDistanceSquared not found"
    return "\n".join(lines) + "\n" + function, None


def numeric(tree):
    text, why = production(tree)
    if text is None:
        print(f"NUMERIC: {why} in this revision")
        return None
    inc = ("namespace BrnDirector\n{\nnamespace\n{\n" + text + "\n}\n"
           "namespace CrashNavGates\n{\n"
           "f32 Outer() { return KF_MAX_CRASH_NAV_PIC_PARADISE_OUTER_DISTANCE_SQ; }\n"
           "f32 Inner() { return KF_MAX_CRASH_NAV_PIC_PARADISE_INNER_DISTANCE_SQ; }\n"
           "f32 Blurriness() { return SF_CRASHNAV_BLURRINESS; }\n"
           "f32 Distance(const ArbStateSharedInfo& lrInfo, const Camera::Camera& lrCamera)\n"
           "{ return PlayerToCameraDistanceSquared(lrInfo, lrCamera); }\n"
           "}\n}\n")
    return compile_and_run(Path(__file__).with_name("MenusCrashNavGates.cpp"), "crashnav_gates.inc", inc,
                           "MenusCrashNavGates")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-root", help="read the b5 sources from <dir>/src/... (a shadow mirror)")
    args = parser.parse_args()
    tree = RootTree(args.src_root) if args.src_root else Tree(args.rev)
    return report("run_menus_crashnav_gates", [], numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
