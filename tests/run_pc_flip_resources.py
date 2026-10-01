import argparse,os
from pathlib import Path
from fxgs_common import REPO,Tree,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
p=argparse.ArgumentParser();p.add_argument('--no-upload',action='store_true');p.add_argument('--no-sublevel-dirty',action='store_true');a=p.parse_args()
shadow={}
if a.no_upload or a.no_sublevel_dirty:
    path='src/pc/gcm/renderengine/TextureUploadPCLeaf.h';text=Tree().read(path)
    if a.no_upload:text=text.replace('lhResult=lpDevice->UpdateTexture(mpTexture,lpDestination);','lhResult=S_OK;')
    if a.no_sublevel_dirty:text=text.replace('SUCCEEDED(lhResult) && luLevel &&','false && luLevel &&')
    shadow[path]=text
result=compile_and_run(Path(__file__).with_name('PCFlipResources.cpp'),'pc_flip_resources_unused.inc','','PCFlipResources',
    extra_sources=[REPO/'src/pc/gcm/renderengine/texture.cpp'],extra_flags='d3d9.lib d3dcompiler.lib user32.lib',shadow=shadow)
raise SystemExit(report('run_pc_flip_resources',[],result,25))
