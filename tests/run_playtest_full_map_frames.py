"""Published CrashNav banks must survive repeated host presentations of one update."""
from pathlib import Path
import argparse
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rev')
    args = parser.parse_args()
    tree = Tree(args.rev)
    source = tree.read('src/GameSource/Gui/CustomRenderer/Renderers/BrnCrashNavIconRenderer.cpp')
    header = tree.read('src/GameSource/Gui/CustomRenderer/Renderers/BrnCrashNavIconRenderer.h')
    numeric = None
    try:
        snapshot = definition(header, 'struct FrameInputPC') + ';'
        bodies = '\n'.join(definition(source, 'void CrashNavIconRenderer::' + name)
                           for name in ('Update()', 'RestoreFrameInputPC()', 'SetRenderEnabled('))
        render_icons = definition(source, 'void CrashNavIconRenderer::RenderIcons(')
        consume = render_icons[render_icons.index('    // ---- roll the per-frame state'):].rsplit('}', 1)[0]
        render_cursor = definition(source, 'void CrashNavIconRenderer::RenderCursor(')
        cursor_consume = 'mGuiEventMapCursorStatus.miAnimationState = 4;'
        assert cursor_consume in render_cursor
        numeric = compile_and_run(Path(__file__).with_name('PlaytestFullMapFrames.cpp'),
            'playtest_full_map_frame_bodies.inc', bodies, 'PlaytestFullMapFrames', extra_files={
                'playtest_full_map_frame_shape.inc': snapshot,
                'playtest_full_map_consume.inc': consume + '\n' + cursor_consume,
            })
    except (ValueError, AssertionError):
        pass
    wiring = []
    render = definition(source, 'void CrashNavIconRenderer::RenderComponent(')
    wiring.append(('frame replay precedes drawing',
                   'RestoreFrameInputPC();' in render and
                   render.index('RestoreFrameInputPC();') < render.index('lpRenderBuffer->BeginRendering();')))
    return report('run_playtest_full_map_frames', wiring, numeric, 7)

if __name__ == '__main__':
    sys.exit(main())
