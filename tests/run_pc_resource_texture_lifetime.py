"""Production pool retirement/raster relocation with real native GPU textures."""
from pathlib import Path
import argparse,os
from fxgs_common import Tree,REPO,STRSTREAM_CPP,definition,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser();parser.add_argument('--omit-retirement',action='store_true');parser.add_argument('--omit-propagation',action='store_true');args=parser.parse_args()
base='src/GameShared/GameClasses/System/Resource/'
tree=Tree();pool=tree.read(base+'CgsResourcePool.cpp')
code='#include "pc/gcm/renderengine/MeshPreparation.h"\nnamespace CgsResource {\n'+definition(pool,'void Pool::FixUpEntry(')+'\n'+definition(pool,'void Pool::FreeMemoryForResource(')+'\n'+definition(pool,'void Pool::DeleteMemoryForEntry(')+'\n}\n'
if args.omit_retirement:
    needle='renderengine::TextureResource_OnEntryFreed(lpEntry, lpEntry->mResource.m_baseResources[0]);'
    assert code.count(needle)==2;code=code.replace(needle,'')
if args.omit_propagation:
    needle='lpOwner->Propogate();';assert code.count(needle)==2;code=code.replace(needle,'')
result=compile_and_run(Path(__file__).with_name('PCResourceTextureLifetime.cpp'),'pc_resource_texture_lifetime.inc',code,
    'PCResourceTextureLifetime',extra_flags='d3d9.lib d3dcompiler.lib user32.lib',extra_sources=[
        REPO/'src/pc/gcm/renderengine/texture.cpp',
        REPO/'src/GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.cpp',
        REPO/base/'CgsResourceTypeBase.cpp',REPO/base/'CgsSmallResource.cpp',
        REPO/base/'CgsBaseResourcePtr.cpp',REPO/base/'CgsResourcePtr.cpp',
        REPO/'vendor/renderware/src/rw/BaseResourceDescriptor.cpp',STRSTREAM_CPP])
raise SystemExit(report('run_pc_resource_texture_lifetime',[],result,48))
