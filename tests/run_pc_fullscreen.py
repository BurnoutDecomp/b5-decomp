"""Real Win32/D3D9 and config restart checks, using test windows and a temporary INI."""
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
source = read(path)
main = read('src/GameSource/Main/BrnMain.cpp')
code = '\n'.join(definition(main, signature) for signature in (
    'void getGameSaveDir(', 'void LoadConfig(', 'void SaveFullscreenConfigPC(', 'void SaveConfig('))
code += '\n' + '\n'.join(definition(source, signature) for signature in (
    'static LRESULT CALLBACK windowProc(', 'static bool RegisterDeviceNotif(',
    'static void DisableSystemBackdrop(', 'static HWND CreateGameWindow('))
headers = ['src/pc/gcm/renderengine/WindowPresentationPCLeaf.h',
           'src/pc/gcm/renderengine/GraphicsSettingsPCLeaf.h',
           'src/pc/gcm/renderengine/DisplayResizePCLeaf.h',
           'src/GameSource/Graphics/BrnAntiAliasTiling.h']
numeric = compile_and_run(HERE / 'PCFullscreen.cpp', 'pc_fullscreen_window.inc', code, 'PCFullscreen',
                          shadow={header:read(header) for header in headers}, extra_flags='d3d9.lib user32.lib shell32.lib')
raise SystemExit(report('run_pc_fullscreen', [], numeric, 87))
