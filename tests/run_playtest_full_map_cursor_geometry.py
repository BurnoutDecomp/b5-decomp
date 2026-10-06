"""Original cursor draw geometry and CRT-produced event icon sizes."""
from pathlib import Path
import argparse
import re
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rev')
    args = parser.parse_args()
    tree = Tree(args.rev)
    source = tree.read('src/GameSource/Gui/CustomRenderer/Renderers/BrnCrashNavIconRenderer.cpp')
    names = ('KF_DEVICE_TO_NORMALISED_X','KF_DEVICE_TO_NORMALISED_Y',
             'KF_CURSOR_HOVER_OFFSET_Y','KF_CURSOR_HOVER_OFFSET_X',
             'KF_CURSOR_HOVER_HALFHEIGHT','KF_CURSOR_HOVER_HALFWIDTH',
             'KF_CURSOR_OFFSET_Y','KF_CURSOR_OFFSET_X','KF_CURSOR_HALFHEIGHT','KF_CURSOR_HALFWIDTH',
             'KF_SELECTED_GROW_RATE','KAF_CURSOR_UV','KAF_ICON_HALFWIDTH','KAF_ICON_HALFHEIGHT',
             'KAF_MINI_ICON_HALFWIDTH','KAF_MINI_ICON_HALFHEIGHT',
             'KF_DRIVETHROUGH_HOVER_LIFT','KF_DRIVETHROUGH_HALFHEIGHT','KF_DRIVETHROUGH_HALFWIDTH',
             'KF_PLAYER_ICON_PULSE_PERIOD')
    constants = []
    for name in names:
        match = re.search(r'\bconst\s+f32\s+' + name + r'\b[^;]+;', source)
        if not match:
            return report('run_playtest_full_map_cursor_geometry', [], None, 10)
        constants.append(match[0].replace('CrashNavIconRenderer::E_CRASHNAVICON_NUM','2'))
    body = definition(source,'void CrashNavIconRenderer::RenderCursor(')
    numeric = compile_and_run(Path(__file__).with_name('PlaytestFullMapCursorGeometry.cpp'),
        'playtest_full_map_cursor_geometry.inc','\n'.join(constants)+'\n'+body,
        'PlaytestFullMapCursorGeometry')
    return report('run_playtest_full_map_cursor_geometry', [], numeric, 10)

if __name__ == '__main__':
    sys.exit(main())
