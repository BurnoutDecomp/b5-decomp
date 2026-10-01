import argparse,os
from pathlib import Path
from fxgs_common import Tree,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
path='src/pc/gcm/renderengine/WindowPresentationPCLeaf.h'
source=Tree().read(path)
p=argparse.ArgumentParser();p.add_argument('--no-flip-filter',action='store_true');a=p.parse_args()
if a.no_flip_filter:
    assert source.count('SelectFilter(lpDevice);')==2
    source=source.replace('SelectFilter(lpDevice);','',1) # ConfigureFlip only; legacy remains the reference.
# Observe the real output just before Present discards it, then make the actual
# native call. GPU readback belongs to this correctness oracle, not the game.
before='mFlip.Active()?mFlip.Present():mpSwapChain->Present'
assert source.count(before)==1
source=source.replace(before,'mFlip.Active()?InspectAndPresent(mFlip):mpSwapChain->Present')
source=source.replace('mpSwapChain->Present(nullptr, nullptr, lhWindow, nullptr, 0)',
                      'InspectAndPresentLegacy(mpSwapChain,lhWindow)')
result=compile_and_run(Path(__file__).with_name('PCFlipPresentation.cpp'),'pc_flip_present_unused.inc','','PCFlipPresentation',
    shadow={path:source},extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_flip_presentation',[],result,39))
