"""Render the production player/rival markers and record the actual atlas UV arguments."""
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
    names = ('KAF_TEXTURE_WIDTH', 'KAF_TEXTURE_HEIGHT', 'KAF_ICON_WIDTH', 'KAF_ICON_HEIGHT',
             'KAF_MINI_ICON_WIDTH', 'KAF_MINI_ICON_HEIGHT', 'KF_MINI_ICON_TEXTURE_OFFSET',
             'KAI_EVENTTYPE_TO_ICON_FRAME', 'KF_DEVICE_TO_NORMALISED_X', 'KF_DEVICE_TO_NORMALISED_Y',
             'KF_RIVAL_HALFWIDTH', 'KF_RIVAL_HALFHEIGHT', 'KF_SELECTED_GROW_TIME',
             'KF_PLAYER_ICON_PULSE_PERIOD')
    constants = []
    for name in names:
        match = re.search(r'\bconst\s+(?:f32|s32)\s+' + name + r'\b[^;]+;', source)
        if not match:
            return report('run_playtest_full_map_player_uv', [], None, 13)
        constants.append(match[0].replace('CrashNavIconRenderer::E_CRASHNAVICON_NUM', '2')
                         .replace('CrashNavIconRenderer::KU_ICON_EVENT_TYPE_COUNT', '11'))
    bodies = '\n'.join(definition(source, 'void CrashNavIconRenderer::' + name)
                       for name in ('InitEventTypeUvs()', 'RenderRivals(', 'RotatateRect('))
    numeric = compile_and_run(Path(__file__).with_name('PlaytestFullMapPlayerUV.cpp'),
        'playtest_full_map_player_uv.inc', '\n'.join(constants) + '\n' + bodies,
        'PlaytestFullMapPlayerUV')
    return report('run_playtest_full_map_player_uv', [], numeric, 13)

if __name__ == '__main__':
    sys.exit(main())
