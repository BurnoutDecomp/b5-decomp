"""Exercise ARTIST BlackBarRenderer's event contract and real draw body with a recording buffer."""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, code_only, extract, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    tree = Tree(args.rev)
    cpp = "src/GameSource/Gui/CustomRenderer/Renderers/BrnBlackBarRenderer.cpp"
    signatures = ("void BlackBarRenderer::Construct(", "void BlackBarRenderer::RecvEvent(",
                  "CgsID BlackBarRenderer::GetID(", "void BlackBarRenderer::RenderComponent(")
    bodies, missing = extract(tree, cpp, signatures)
    manager = code_only(tree.read("src/GameSource/Gui/CustomRenderer/BrnCustomRenderer.cpp"))
    bridge = code_only(tree.read("src/GameSource/Game/GameBridgeDirectorToX.cpp"))
    wiring = [
        ("director publishes GuiEventSetBlackBars through inbound queue", "AddGuiEvent(lBlackBars, lpGuiInputBuffer)" in bridge),
        ("bar height comes from the valid camera's effects", "E_FLAG_VALID" in bridge and "lrEffects.mfBlackBarAmount" in bridge),
        ("manager's final black-bar slot assignment mounts the renderer", bool(re.findall(
            r"mapCustomRenderComponents\[E_BLACKBAR\]\s*=\s*([^;]+);", manager)) and re.findall(
            r"mapCustomRenderComponents\[E_BLACKBAR\]\s*=\s*([^;]+);", manager)[-1].strip() == "&mBlackBarRenderer"),
    ]
    if missing:
        print("Missing production bodies:", ", ".join(missing))
        numeric = None
    else:
        numeric = compile_and_run(Path(__file__).with_name("PlaytestCinematicBars.cpp"),
            "playtest_cinematic_bars.inc", "\n".join(bodies), "PlaytestCinematicBars")
    return report("run_playtest_cinematic_bars", wiring, numeric, 14)

if __name__ == "__main__":
    sys.exit(main())
