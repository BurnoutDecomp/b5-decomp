"""FX-CRASHVFX (crash parity 2026-09-25): BrnGameModule::DoDispatch @0x823DC458 writes
DispatchThreadInputBuffer::mbIsRenderingAtFullFrameRate (+0x99B0) from the director camera's current flag set: full
rate only for E_FLAG_RACING_GAMEPLAY_CAMERA (bit 3) or E_FLAG_ROAD_FOLLOWING_CAM (bit 27). The PC never wrote it, so
ParticleModule::GenerateRenderRequests never raised eRenderDataFlagReducedFrameRate and the particle module stayed in
its normal-driving arm in every crash (debris never collided, 2 s buckets, short lifetimes).

WIRING -- DoDispatch writes the flag under the dispatch buffer's write lock, before the stand-in particle producer and
the real one (EffectsModule::GenerateDispatchLists), both of which read it.
NUMERIC -- tests/FxCrashVfxFullFrameRate.cpp compiles the production statement against the real CameraState and
compares it with 0x823DC5C8..0x823DC630's own stores on emu64 for 93 flag patterns
(tests/FxCrashVfxFullFrameRateData.h, scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_fullrate_data.py).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_full_frame_rate.py [--rev <b5 rev>]
                                                                                   [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, compile_and_run, definition, report

SOURCE = "src/GameSource/Game/BrnGameModule.cpp"
SIGNATURE = "int BrnGameModule::DoDispatch("
STATEMENT = re.compile(r"const bool lbFullFrameRate\s*=[^;]*;")
NUMERIC_CHECKS = 1


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


def body(tree):
    try:
        return code_only(definition(tree.read(SOURCE).replace("\r\n", "\n"), SIGNATURE))
    except ValueError:
        return ""


def wiring(tree):
    text = body(tree)
    write = "SetIsRenderingAtFullFrameRate(lbFullFrameRate)"
    at = text.find(write)
    locked = at >= 0 and text.rfind("LockForWrite()", 0, at) > text.rfind("UnlockForWrite()", 0, at) \
        and text.find("UnlockForWrite()", at) > at
    yield ("DoDispatch writes mbIsRenderingAtFullFrameRate under the dispatch buffer's write lock", locked)
    stand_in = text.find("PCBringUpProduceParticleRenderData(")
    producer = text.find("GenerateDispatchLists(")
    yield ("...before the stand-in particle producer and EffectsModule::GenerateDispatchLists (both read it)",
           at >= 0 and 0 <= at < stand_in and at < producer)


def numeric(tree):
    match = STATEMENT.search(body(tree))
    if match is None:
        print("NUMERIC: cannot build -- DoDispatch has no `const bool lbFullFrameRate = ...;`")
        return None
    return compile_and_run(Path(__file__).with_name("FxCrashVfxFullFrameRate.cpp"), "fxcrashvfx_full_frame_rate.inc",
                           match.group(0) + "\n", "FxCrashVfxFullFrameRate")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    return report("run_fxcrashvfx_full_frame_rate", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
