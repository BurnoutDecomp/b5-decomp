"""Execute the selected host loading producer admission and original virtual Render slots.

--root overlays an isolated selected source tree, so mixed deferred loading WIP
cannot be mistaken for the publication candidate. No game or graphics device runs.
"""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, code_only, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path)
parser.add_argument("--old-network-proxy", action="store_true")
parser.add_argument("--early-initial", action="store_true")
args = parser.parse_args()

class SelectedTree(Tree):
    def read(self, relative):
        selected = args.root / relative if args.root else None
        return selected.read_text(encoding="utf-8-sig") if selected and selected.exists() else super().read(relative)

tree = SelectedTree()
game = tree.read("src/GameSource/Game/BrnGameModule.cpp")
flow = tree.read("src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp")
main = definition(game, "bool BrnGameModule::GameMain()")
start = main.index("                    bool lbRenderLoadingGui = false;")
last = definition(main[start:], "if (lbRenderLoadingGui)")
admission = main[start:main.index(last, start) + len(last)]
if args.old_network_proxy:
    admission = admission.replace("lbRenderLoadingGui = gBrnScriptedLoadStage != 8;",
                                  "lbRenderLoadingGui = mpNetworkOutputBuffer == nullptr;")
if args.early_initial:
    admission = admission.replace("> MainGameFlowStateInitialLoadingScreen::E_LOADINGSTAGE_GUIMODULE", ">= MainGameFlowStateInitialLoadingScreen::E_LOADINGSTAGE_GUIMODULE")
renders = "\n".join(definition(flow, "void " + name + "::Render()") for name in (
    "LoadingScriptedState", "MainGameFlowStateStartScreen", "MainGameFlowStateMarketingScreens",
    "MainGameFlowStateCheckDiskSpace", "MainGameFlowStateMemoryCard", "MainGameFlowStateCompleteLoading"))
renders += "\n" + definition(tree.read(
    "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowInGameState.cpp"),
    "void MainGameFlowStateInGame::Render()")
live = code_only(main)
create = code_only(definition(game, "void BrnGameModule::CreateStaticIOBuffers()"))
dispatch = code_only(definition(game, "int BrnGameModule::DoDispatch()"))
wiring = [
    ("selected host order is actual GUI update, game consumer, admitted GUI producer, virtual Render, IO teardown",
     live.index("mGuiModule.Update()") < live.index("BridgeGuiToGame(mGuiModule.GetGuiOutQueue())")
     < live.index("bool lbRenderLoadingGui") < live.index("lpState->Render()")
     < live.index("DestroyIOBuffer<BrnDirector::DirectorIO::OutputBuffer>")),
    ("real final-step IO is created before the flow update and survives its virtual Render",
     live.index("CreateStaticIOBuffers()") < live.index("lpState->Update()") < live.index("lpState->Render()")
     and all(name in create for name in ("mpDirectorOutputBuffer", "mpGuiInputBuffer", "mpWorldUpdateOutputBuffer", "mpEffectsOutputBuffer"))),
    ("stage8 dispatch uses its own real renderer/world/effects dispatch IO and has no Network output dependency",
     all(name in dispatch for name in ("IOHelper<RendererIO::InputBuffer>", "IOHelper<BrnWorldIO::DispatchInputBuffer>",
                                      "IOHelper<BrnEffects::EffectsIO::DispatchInputBuffer>"))
     and "mpNetworkOutputBuffer" not in dispatch),
]
result = compile_and_run(Path(__file__).with_name("PCLoadingCommandProducer.cpp"),
    "loading_virtual_renders.inc", renders, "PCLoadingCommandProducer",
    extra_files={"loading_host_admission.inc": admission})
raise SystemExit(report("run_pc_loading_command_producer", wiring, result, 1))
