"""OWNERLIST 2026-09-27, lane L5 MENUS: the ICE movie playback the pause / crash-nav camera rides on.

ArbStateCrashNav::Prepare @0x822660A8 loops the pause playlist through ICEMoviePlayer -> ICEWrapper::PlayMovie. The
camera it shows is ICEWrapper's ICECamera, which MainDirector::UpdateICE moves every frame. On the PC none of the chain ran:
  1. ICEWrapper::Prepare @0x8253DD90 dropped its first stores and calls:
       - the resource-manager store (stwx r27, +0x11B24 @0x8253DDFC). Without it the first PlayMovie faulted in
         DirectorResourceManager::GetKeyAnim;
       - ICEMemory::Construct / HeapMalloc::Prepare, and ICE::spICEMemory = &mICEMemory;
       - ICECameraMover::Construct, the inlined ICEPointers::Construct, and ICEManager::Construct (@0x8253DEAC);
       - stage 1's second mover Construct.
  2. MainDirector::UpdateICE @0x82238FC0 had no body, and MainDirector::Update never called it (a GATE comment stood at
     0x8227434C..0x8227436C).
  3. ICEWrapper::Update @0x82540180 sat in ICEWrapper.cpp, a TU the build does not mount. It runs
     ICETimer::Update -> ICEManager::Update -> ICECameraMover::Update(1.0f).

Wiring only: each check reads the production body, with comments stripped, so that no comment can satisfy it.
The ICE angle maths is tests/run_menus_ice_math.py. The arbitrator's CRASH_NAV entry is tests/run_menus_pausecam_arbitrator.py.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_menus_ice_playback.py [--rev <b5 rev>]
        [--src-root <dir holding src/...>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, body_or_empty, code_only, report

PREPARE_CPP = "src/GameSource/Director/BrnDirectorICEWrapperPrepare.cpp"
WRAPPER_MOUNTED_CPP = "src/SDKs/Packages/ICE/ICEWrapper_wG_11.cpp"
UPDATE_ICE_CPP = "src/GameSource/Director/BrnMainDirector_wM_01.cpp"
MAIN_DIRECTOR_CPP = "src/GameSource/Director/BrnMainDirector.cpp"


class RootTree(Tree):
    """A b5 source tree rooted somewhere else (e.g. a lane's shadow mirror): <root>/src/..."""

    def __init__(self, root):
        super().__init__(None)
        self.root = Path(root)

    def read(self, relative):
        path = self.root / relative
        if not path.exists():
            return super().read(relative)
        return path.read_text(encoding="utf-8-sig")


def has(text, pattern):
    return re.search(pattern, text) is not None


def wiring(tree):
    prepare = body_or_empty(tree.read(PREPARE_CPP), "bool ICEWrapper::Prepare(")
    update = body_or_empty(tree.read(WRAPPER_MOUNTED_CPP), "void ICEWrapper::Update(")
    update_ice = body_or_empty(tree.read(UPDATE_ICE_CPP), "void MainDirector::UpdateICE(")
    main_update = body_or_empty(tree.read(MAIN_DIRECTOR_CPP), "void MainDirector::Update(")
    return [
        ("ICEWrapper::Prepare stores the resource manager (stwx +0x11B24 @0x8253DDFC)",
         has(prepare, r"\bmpResourceManager\s*=")),
        ("ICEWrapper::Prepare constructs and prepares the ICE heap (ICEMemory::Construct / Prepare)",
         has(prepare, r"\bmICEMemory\.Construct\(") and has(prepare, r"\bmICEMemory\.Prepare\(")),
        ("ICEWrapper::Prepare publishes ICE::spICEMemory (dword_82FB62C0)",
         has(prepare, r"\bspICEMemory\s*=")),
        ("ICEWrapper::Prepare constructs the camera mover on both stages (two ICECameraMover::Construct)",
         len(re.findall(r"\bmCameraMover\.Construct\(", prepare)) >= 2),
        ("ICEWrapper::Prepare constructs the ICE manager with the ICE pointers (@0x8253DEAC)",
         has(prepare, r"\bmICEManager\.Construct\(")),
        ("ICEWrapper::Update is defined in a mounted TU (ICEWrapper_wG_11.cpp)",
         update != ""),
        ("ICEWrapper::Update runs ICETimer::Update, ICEManager::Update and ICECameraMover::Update",
         has(update, r"\bmICETimer\.Update\(") and has(update, r"\bmICEManager\.Update\(")
         and has(update, r"\bmCameraMover\.Update\(")),
        ("MainDirector::UpdateICE has a body that updates the ICE wrapper (@0x82238FC0)",
         has(update_ice, r"\bmICEWrapper\.Update\(")),
        ("MainDirector::Update calls UpdateICE (0x8227434C..0x8227436C)",
         has(main_update, r"\bUpdateICE\(")),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--src-root", help="read the b5 sources from <dir>/src/... (a shadow mirror)")
    args = parser.parse_args()
    tree = RootTree(args.src_root) if args.src_root else Tree(args.rev)
    return report("run_menus_ice_playback", wiring(tree), (0, 0), 0)


if __name__ == "__main__":
    sys.exit(main())
