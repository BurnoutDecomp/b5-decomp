"""ARTIST 8226FFBC..8226FFCC rewinds the +424 normal-intro handle.

Run the entire production CAR_UNLOCK switch arm. Distinct recording handles
ensure a rewind of the browse camera cannot satisfy the test.
"""
import argparse
from pathlib import Path
import re

from fxgs_common import Tree, compile_and_run, definition, report

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rev")
args = parser.parse_args()
source = Tree(args.rev).read(
    "src/GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.cpp")
update = definition(source, "void ArbStateCarSelect::Update(ArbStateSharedInfo& lrSharedInfo)")
arm = re.search(r"case E_STATE_CAR_UNLOCK:.*?(?=case E_STATE_ACTIVE:)", update, re.S)[0]
code = ("void Fixture::Step() {\nauto& lrGameState = mGame; auto& lrSharedInfo = mShared;\n"
        "switch(meState) {\n" + arm + "\ndefault: break;\n}}\n")
result = compile_and_run(Path(__file__).with_name("JunkyardUnlockCamera.cpp"),
    "junkyard_unlock_camera.inc", code, "JunkyardUnlockCamera")
raise SystemExit(report("run_junkyard_unlock_camera", [], result, 12))
