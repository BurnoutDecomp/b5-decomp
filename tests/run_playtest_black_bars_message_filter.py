"""Exercise the production bar-height message filter against ARTIST's threshold edges."""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, extract, code_only, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev")
    args = parser.parse_args()
    tree = Tree(args.rev)
    bodies, missing = extract(tree, "src/GameSource/Gui/BrnGuiHudMessageDirector_gUI_00.cpp",
                              ("void HudMessageDirector::SetBlackBarSize(",))
    numeric = None if missing else compile_and_run(Path(__file__).with_name("PlaytestBlackBarsMessageFilter.cpp"),
        "playtest_black_bars_message_filter.inc", "\n".join(bodies), "PlaytestBlackBarsMessageFilter")
    module = code_only(tree.read("src/GameSource/Gui/BrnGuiModule.cpp"))
    wiring = [("GuiModule drives the bar-size message filter", "mHudMessageDirector.SetBlackBarSize(" in module)]
    return report("run_playtest_black_bars_message_filter", wiring, numeric, 7)

if __name__ == "__main__":
    sys.exit(main())
