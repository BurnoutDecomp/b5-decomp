"""The actual panel bind posts ARTIST's rank-progress request once, at the correct size."""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rev')
    args = parser.parse_args()
    tree = Tree(args.rev)
    source = tree.read('src/GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.cpp')
    numeric = None
    try:
        shape = definition(source, 'struct RankProgressRequestPayload437') + ';'
        body = definition(source, 'bool CrashNavPanel::RecEvent(')
        bind = body[body.index('        // ---- event 64:'):].rsplit('}', 1)[0]
        numeric = compile_and_run(Path(__file__).with_name('PlaytestFullMapRankRequest.cpp'),
            'playtest_full_map_rank_request.inc', bind, 'PlaytestFullMapRankRequest',
            extra_files={'playtest_full_map_rank_request_shape.inc': shape})
    except ValueError:
        pass
    manager = code_only(tree.read('src/GameSource/Gui/CustomRenderer/BrnCustomRenderer.cpp'))
    wiring = [('map road signs receive the real text renderer',
               'mCrashNavIconRenderer.SetTextRenderer(lpTextRenderer);' in manager),
              ('map road signs receive the real language manager',
               'mCrashNavIconRenderer.SetLanguageManager(lpLanguageManager);' in manager)]
    return report('run_playtest_full_map_rank_request', wiring, numeric, 4)

if __name__ == '__main__':
    sys.exit(main())
