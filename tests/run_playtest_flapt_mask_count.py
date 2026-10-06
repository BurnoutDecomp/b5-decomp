"""Run the actual FLAPT mask open/close bodies against zero/one/two-mesh layers."""
from pathlib import Path
import argparse
import os
import sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
p=argparse.ArgumentParser();p.add_argument('--rev');args=p.parse_args()
source=Tree(args.rev).read('src/GameSource/Gui/Flapt/BrnFlaptRenderer.cpp')
parts=[definition(source,'void FlaptRenderer::'+name+'(') for name in ('StartDrawingMask','PopMask','RenderMask')]
adapter=definition(Tree().read('src/GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm2dRenderBuffer.cpp'),
                   'Im2dTransform Im2dTransformToLogicalPC(')
result=compile_and_run(Path(__file__).with_name('PlaytestFlaptMaskCount.cpp'),
    'playtest_flapt_mask_count.inc','\n'.join(parts),'PlaytestFlaptMaskCount',
    extra_files={'playtest_flapt_logical.inc':adapter})
raise SystemExit(report('run_playtest_flapt_mask_count',[],result,30))
