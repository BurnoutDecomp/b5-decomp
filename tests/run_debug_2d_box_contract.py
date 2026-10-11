"""Grouped production geometry, queue and pixel regression for the original 2D box contracts.

Prepared for the combined Traffic/AI validation round. No native game/device is needed;
only the final graphics submission and text/configuration boundaries are test sinks.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
render_path = 'src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender'
queue_path = 'src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender'
menu_path = 'src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.cpp'
render = tree.read(render_path + '.cpp')
buffered = tree.read(queue_path + '.cpp')
menu = tree.read(menu_path)
methods = 'namespace CgsDev {\n'
if 'bool Debug2DImmediateRender::Do2DBoxesIntersect(' in render:
    methods += definition(render, 'bool Debug2DImmediateRender::Do2DBoxesIntersect(') + '\n'
for signature in (
    'Vector2 Debug2DImmediateRender::GetVirtualScreenSize(',
    'bool Debug2DImmediateRender::Is2DPointOnScreen(',
    'bool Debug2DImmediateRender::Is2DLineOnScreen(',
    'void Debug2DImmediateRender::DispatchVertices(',
    'void Debug2DImmediateRender::AddVertex(',
    'void Debug2DImmediateRender::EmitQuad(',
    'void Debug2DImmediateRender::DrawBox(Vector2',
    'void Debug2DImmediateRender::DrawBox(f32',
    'void Debug2DImmediateRender::DrawLine(',
    'void Debug2DImmediateRender::DrawFrame(f32',
    'void Debug2DImmediateRender::DrawFrame(Vector2',
    'void Debug2DImmediateRender::DrawHorizontalBar(',
    'void Debug2DImmediateRender::DrawTextWithBackground(',
):
    methods += definition(render, signature) + '\n'
for signature in (
    'void DebugRender::Draw2DBox(Vector2',
    'void DebugRender::Draw2DBox(f32',
    'void DebugRender::Dispatch2D(',
):
    methods += definition(buffered, signature) + '\n'
methods += '}\nnamespace CgsDev::DebugUI {\n'
for signature in (
    'MenuItem::MenuItem()',
    'void    MenuItem::Update(',
    'void    MenuItem::Render(',
    'void    MenuItem::ComputeSize(',
    'bool    MenuItem::IsUseful(',
    'bool    MenuItem::IsVisible(',
    'void    MenuItem::GetDisplayName(',
    'void    MenuItem::GetItemString(',
    'Window* MenuItem::OpenAsWindow(',
    'void MenuItem::RenderMenuItemText(',
    'void MenuItem::RenderMenuItemBackground(',
):
    methods += definition(menu, signature) + '\n'
methods += '}\n'
shadow = {render_path + '.h': tree.read(render_path + '.h'),
          queue_path + '.h': tree.read(queue_path + '.h')}
result = compile_and_run(Path(__file__).with_name('Debug2DBoxContract.cpp'),
                         'debug_2d_box_contract.inc', methods, 'Debug2DBoxContract',
                         shadow=shadow, extra_sources=(STRSTREAM_CPP,))
raise SystemExit(report('run_debug_2d_box_contract', [], result, 46))
