"""Exercise complete production render bodies against ARTIST argument witnesses."""
import argparse
from pathlib import Path
import fxgs_common as common

parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
common.WORKFLOW = next(p for p in repo.parents if (p / 'tools/build/msvc_flags.txt').exists())
tree = common.Tree(args.rev)
window = tree.read('src/GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.cpp')
textures = tree.read('src/GameShared/GameClasses/System/Resource/CgsDebugPoolTextures.cpp')
inc = 'namespace CgsDev { namespace DebugUI {\n'
inc += '\n'.join(common.definition(window, s) for s in (
    'RGBA Window::ScaleColour(', 'f32 Window::CalcCaptionHeight(', 'void Window::Render('))
inc += '\n}}\nnamespace CgsResource {\n'
inc += common.definition(textures, 'void DebugPoolTextures::RenderThumbnails(')
inc += '\n}\n'
result = common.compile_and_run(repo / 'tests/DebugMenuResourceRepairs.cpp',
    'ResourceRepairs.inc', inc, 'ResourceRepairs', extra_flags=f'/I"{repo / "src"}"')
raise SystemExit(result is None or bool(result[1]))
