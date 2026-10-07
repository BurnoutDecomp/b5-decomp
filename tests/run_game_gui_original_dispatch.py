"""Original full GUI bridge order and loading caller wiring; no device or game launch.

--drop-replay and --old-order mutate extracted production text only. The real
GUI module body is a typed boundary observer here and has its own tests.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--drop-replay", action="store_true")
parser.add_argument("--old-order", action="store_true")
parser.add_argument("--vehicle-only", action="store_true")
parser.add_argument("--loading-true", action="store_true")
args = parser.parse_args()
tree = Tree()
game = tree.read("src/GameSource/Game/BrnGameModule.cpp")
flow = tree.read("src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp")
if args.loading_true:
    flow = flow.replace("&s_GameDataInput, &s_GameDataOutput, false);",
                        "&s_GameDataInput, &s_GameDataOutput, true);")
    flow = flow.replace("lpGameDataInput, lpGameDataOutput, false);",
                        "lpGameDataInput, lpGameDataOutput, true);")
gui_leg = definition(game, "void BrnGameModule::DoUpdate_GUI(")
resource_bridge = definition(game, "void BrnGameModule::BridgeGuiToResource(")
if args.vehicle_only:
    resource_bridge = resource_bridge.replace("                lRequests.GetWheelList(lpRequest->mpReceiverQueue, 1);\n", "")
if args.drop_replay:
    gui_leg = gui_leg.replace("        BridgeReplayToGui(lpGuiInput, lpReplayOutput);\n", "")
if args.old_order:
    gui_leg = gui_leg.replace("        BridgeWorldToGui(lpGuiInput, lpWorldOutput);\n"
                              "        BridgeControllerToGui(lpGuiInput, lpInputOutput);",
                              "        BridgeControllerToGui(lpGuiInput, lpInputOutput);\n"
                              "        BridgeWorldToGui(lpGuiInput, lpWorldOutput);")
initial = code_only(definition(flow, "void MainGameFlowStateInitialLoadingScreen::Update()"))
partial = code_only(definition(flow, "void LoadingScriptedState::Update()"))
helper = code_only(definition(flow, "void LoadingScriptedState::RenderGUI("))

def ordered(text, *terms):
    positions = [text.find(term) for term in terms]
    return all(p >= 0 for p in positions) and positions == sorted(positions)

wiring = [
    ("initial loading preserves update/resource/game/latch/render order", ordered(initial,
        "GetGameTimer().Update()", "GetGuiModule().Update(", "BridgeGuiToResource(",
        "BridgeGuiToGame(", "IsGuiPhaseComplete()", "RenderGUI(")),
    ("partial loading updates GUI before sound and renders only below stage 8", ordered(partial[
        partial.index("GetGuiModule().Update("):],
        "GetGuiModule().Update(", "BridgeGuiToSound(", "BridgeGuiToResource(",
        "BridgeGuiToGame(", "if (gBrnScriptedLoadStage != 8)", "RenderGUI(")),
    ("loading render creates renderer output before bridge and original GUI camera", ordered(helper,
        "GetRenderModule().Update(", "BridgeRendererToGui(", "CgsGui::GetGuiCamera()",
        "SetCamera(", "CaptureRenderInputPC(", "GetGuiModule().Render(")),
    ("full GUI leg has no inactive replay/null source admission", all(term not in code_only(gui_leg)
        for term in ("if (lpReplayOutput", "OutputBuffer_PreSim l", "GetCompletedRenderInputPC"))),
    ("both original loading GUI updates pass false, while the full GUI entry passes true",
        "&s_GameDataInput, &s_GameDataOutput, false);" in partial
        and "lpGameDataInput, lpGameDataOutput, false);" in initial
        and "lpGameDataInput, lpGameDataOutput, true);" in code_only(gui_leg)),
]
base = tree.read("src/GameShared/GameClasses/Module/CgsIOBuffer.cpp")
bodies = base + "\nnamespace BrnGame {\n" + gui_leg + "\n" + resource_bridge + "\n}\n"
result = compile_and_run(Path(__file__).with_name("GameGuiOriginalDispatch.cpp"),
    "game_gui_original_dispatch.inc", bodies, "GameGuiOriginalDispatch", extra_sources=[STRSTREAM_CPP])
raise SystemExit(report("run_game_gui_original_dispatch", wiring, result, 0))
