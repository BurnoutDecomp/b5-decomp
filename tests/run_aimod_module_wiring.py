"""FX-AIMOD (crash parity 2026-09-22): structural regression for AIModule wiring the console has.

Each check extracts a PRODUCTION function body from the real source file and requires the console's
stores/calls, citing the ARTIST addresses that prove them. Pure wiring (a store, a call, an argument)
is what this runner covers; numeric behaviour lives in the AIMod*.cpp unit tests.

Run from the workflow checkout:
    python b5-decomp/tests/run_aimod_module_wiring.py [--rev <b5 git rev>]
--rev reads every source file from that b5-decomp revision instead of the working tree (the RED side:
e.g. --rev <commit>~1 for the commit that landed a check).
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]


class Tree:
    def __init__(self, rev):
        self.rev = rev

    def read(self, relative):
        if self.rev is None:
            return (REPO / relative).read_text(encoding="utf-8-sig")
        return subprocess.run(["git", "-C", str(REPO), "show", f"{self.rev}:{relative}"],
                              check=True, capture_output=True, text=True, encoding="utf-8").stdout


def function_body(source, signature):
    start = source.index(signature)
    depth = 0
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}]', source[start:]):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[start:start + token.end()]
    raise ValueError("unterminated body: " + signature)


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


AIMODULE = "src/GameSource/World/AI/BrnAIModule.cpp"
EVENTS = "src/GameSource/World/AI/BrnAIModule_Events.cpp"


def checks(tree):
    """Yield (name, passed) pairs."""
    events = tree.read(EVENTS)
    mode_start = code_only(function_body(events, "void AIModule::OnModeStart("))
    # G04-D2: 0x82791DF4 lbz 0x94 ; cntlzw ; extrwi -> stbx 0x4EB7C and 0x82791E24 lbz 0x94 -> stbx 0x4EB7D
    enable = re.search(r"mbEnableDrivingInput\s*=\s*!\s*lpGameModeParams->mbIsOnline\s*;", mode_start)
    online = re.search(r"mbIsInOnlineGameMode\s*=\s*lpGameModeParams->mbIsOnline\s*;", mode_start)
    balance = mode_start.find("SetupRaceBalancingManager(")
    yield ("G04-D2 OnModeStart: mbEnableDrivingInput = !params.mbIsOnline (0x82791DF4..0x82791E1C)", enable is not None)
    yield ("G04-D2 OnModeStart: mbIsInOnlineGameMode = params.mbIsOnline (0x82791E24/0x82791E38)", online is not None)
    yield ("G04-D2 both stores precede SetupRaceBalancingManager (0x82791E44), as on the console",
           enable is not None and online is not None and 0 <= balance
           and enable.start() < balance and online.start() < balance)

    construct = code_only(function_body(tree.read(AIMODULE), "void AIModule::Construct()"))
    # G04-D6: 0x82794D34 li r30,0 ; 0x827952F4 stwx r30 -> +0x4E9F8 ; 0x8279530C stwx r30 -> +0x4E9FC
    yield ("G04-D6 Construct stores 0 to mePlayerActiveRaceCarIndex (0x827952F4)",
           re.search(r"mePlayerActiveRaceCarIndex\s*=\s*E_ACTIVE_RACE_CAR_INDEX_0\s*;", construct) is not None)
    yield ("G04-D6 Construct stores 0 to mePlayerGlobalRaceCarIndex (0x8279530C)",
           re.search(r"mePlayerGlobalRaceCarIndex\s*=\s*E_GLOBAL_RACE_CAR_INDEX_0\s*;", construct) is not None)
    yield ("G04-D6 no INVALID seed of the active cursor",
           "mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID" not in construct)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None)
    args = parser.parse_args()
    results = list(checks(Tree(args.rev)))
    failures = [name for name, passed in results if not passed]
    for name in failures:
        print("FAIL", name)
    print(f"AIModModuleWiring: {len(results)} checks, {len(failures)} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
