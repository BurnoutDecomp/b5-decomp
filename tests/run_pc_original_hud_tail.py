"""Original HUD/movie command ordering; --old-order is the rejected late-dispatch control."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, code_only, compile_and_run, report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser()
parser.add_argument('--old-order',action='store_true')
args=parser.parse_args()
source=Tree().read('src/GameSource/Graphics/BrnRendererModule.cpp')
render=definition(source,'void BrnRendererModule::Render(')
start=render.index('// ARTIST 8240E000..E054')
block=definition(render[start:],'if (mbRenderHudImmediateMode)')
wiring=[('private movie plane/linger and Apt dispatch are retired',all(term not in code_only(render) for term in (
    'PCMovieFrame','maPCMovieFrames','gu64LastMoviePresentTick','lbLoadingFadePending','DispatchRenderBufferPC')))]
if args.old_order:
    block=block.replace('        mIm2dRenderBuffer.Dispatch(&mIm2dRenderer);\n','')
    block=block.replace('        mLoadingScreenRenderer.RenderForeground(&mIm2dRenderer);',
        '        mLoadingScreenRenderer.RenderForeground(&mIm2dRenderer);\n'
        '        mIm2dRenderBuffer.Dispatch(&mIm2dRenderer);')
result=compile_and_run(Path(__file__).with_name('PCOriginalHudTail.cpp'),
    'original_hud_tail.inc',block,'PCOriginalHudTail')
raise SystemExit(report('run_pc_original_hud_tail',wiring,result,8))
