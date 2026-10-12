"""Run whole production heap-window input and colour bodies with state observers."""
from pathlib import Path
import fxgs_common as common

repo = Path(__file__).resolve().parents[1]
common.WORKFLOW = next(p for p in repo.parents if (p / 'tools/build/msvc_flags.txt').exists())
source = (repo / 'src/GameShared/GameClasses/System/Resource/CgsDebugPoolWindow.cpp').read_text(encoding='utf-8-sig')
helpers = '\n'.join(common.definition(source, signature) for signature in (
    'u32 ColourByte(f32 value)', 'u32 ScrollSegments(f32 width)'))
bodies = '\n'.join(common.definition(source, signature) for signature in (
    'void DebugPoolWindow::Update(', 'CgsDev::RGBA DebugPoolWindow::CalculateColour('))
inc = 'namespace CgsResource { namespace {\n' + helpers + '\n}\n' + bodies + '\n}\n'
result = common.compile_and_run(repo / 'tests/DebugMenuResourceWindow.cpp',
    'ResourceWindow.inc', inc, 'ResourceWindow', extra_flags=f'/I"{repo / "src"}"')
raise SystemExit(result is None or bool(result[1]))
