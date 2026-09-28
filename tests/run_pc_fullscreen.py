"""Focused real Win32/D3D9 check; creates only a hidden test window, no game/save."""
import argparse
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

HERE = Path(__file__).resolve().parent
p = argparse.ArgumentParser()
p.add_argument('--source-root', type=Path)
a = p.parse_args()
def read(path):
    return (a.source_root / path).read_text(encoding='utf-8') if a.source_root else Tree().read(path)
path = 'src/GameShared/GameClasses/System/PC/CgsHardwareInitPC.cpp'
code = definition(read(path), 'static LRESULT CALLBACK windowProc(')
header = 'src/pc/gcm/renderengine/WindowPresentationPCLeaf.h'
numeric = compile_and_run(HERE / 'PCFullscreen.cpp', 'pc_fullscreen_window.inc', code, 'PCFullscreen',
                          shadow={header:read(header)}, extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_fullscreen', [], numeric, 26))
