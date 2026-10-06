"""Actual MoveCursor body, with native fixed-world bounds and old-code control."""
from pathlib import Path
import argparse,sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,compile_and_run,report

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rev');args=ap.parse_args()
    body=definition(Tree(args.rev).read('src/GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.cpp'),
                    'void CrashNavMap::MoveCursor(')
    numeric=compile_and_run(Path(__file__).with_name('PlaytestFullMapPanBounds.cpp'),
        'playtest_full_map_pan_bounds_bodies.inc','namespace BrnGui {\n'+body+'\n}','PlaytestFullMapPanBounds')
    return report('run_playtest_full_map_pan_bounds',[],numeric,49)
if __name__=='__main__':sys.exit(main())
