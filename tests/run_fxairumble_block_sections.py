"""FX-AI-RUMBLE G04-D1: AIModule::OnModeStart's checkpoint loop (X360 0x82791E90..0x82791F14).

Extracts the PRODUCTION loop from AIModule::OnModeStart (BrnAIModule_Events.cpp), plus
RouteRequestManager::SetBlockSections and GameModeParams::GetCheckpointCount / GetCheckpointData,
and checks the RouteRequestManager slots after a mode start. A revision without the loop runs an
empty harness (the pre-fix behaviour), so the old tree reports the missing copy as failed checks.

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxairumble_block_sections.py [--rev <b5 rev>]
"""
import re
import sys
sys.dont_write_bytecode = True
from aimod_common import Tree, parse_args, compile_and_run, definition

EVENTS = "src/GameSource/World/AI/BrnAIModule_Events.cpp"
RRM = "src/GameSource/World/AI/BrnRouteRequestManager.cpp"
PARAMS = "src/GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.cpp"

LOOP_HEAD = re.compile(r"^    for \(s32 (\w+) = 0; \1 < lpGameModeParams->GetCheckpointCount\(\); \+\+\1\)", re.M)


def onmodestart_loop(events):
    body = definition(events, "void AIModule::OnModeStart(")
    match = LOOP_HEAD.search(body)
    if not match:
        return "        // [pre-fix] OnModeStart has no checkpoint loop"
    return definition(body[match.start():], match.group(0))


def chunks_for(events, rrm, params):
    return ["namespace BrnGameState {",
            definition(params, "s32 GameModeParams::GetCheckpointCount("),
            definition(params, "const CheckpointData* GameModeParams::GetCheckpointData("),
            # the non-const overload; anchored on the line start so it cannot match inside the const one
            definition(params, "\nCheckpointData* GameModeParams::GetCheckpointData("),
            "}",
            "namespace BrnAI {",
            definition(rrm, "void RouteRequestManager::SetBlockSections("),
            "struct BlockSectionHarness",
            "{",
            "    RouteRequestManager mRouteRequestManager;",
            "    void Run(const BrnGameState::GameModeParams* lpGameModeParams)",
            "    {",
            onmodestart_loop(events),
            "    }",
            "};",
            "}"]


def main():
    tree = Tree(parse_args().rev)
    chunks = chunks_for(tree.read(EVENTS), tree.read(RRM), tree.read(PARAMS))
    sys.exit(compile_and_run("FxAiRumbleBlockSections.cpp", chunks, prefix="brn_fxairumble_blk_"))


if __name__ == "__main__":
    main()
