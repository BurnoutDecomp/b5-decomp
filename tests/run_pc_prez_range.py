"""Original linear pre-Z range through production renderer setup and mesh admission."""
import argparse,os,re
from pathlib import Path
from fxgs_common import Tree,definition,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
os.environ.pop('BRN_PREZ_ALL',None)
p=argparse.ArgumentParser();p.add_argument('--old-context',action='store_true');args=p.parse_args()
root='src/GameSource/Graphics/BrnRendererModule'
current=Tree();body=definition(Tree('be68dc2a' if args.old_context else None).read(root+'.cpp'),
    'bool BrnRendererModule::BuildDispatchLists(').replace('BrnRendererModule::','PreZRenderer::')
if not args.old_context:
    body = definition(current.read(root+'.cpp'), 'void BrnRendererModule::InitializeDispatchContextPC(').replace('BrnRendererModule::','PreZRenderer::') + '\n' + body
header=current.read(root+'.h')
defaults='\n'.join(re.search(r'^    '+name+r' = [^;]+;',header,re.M)[0]
    for name in ['mbPreZNearOnly','mfPreZDistanceThreshold'])
commands=current.read('src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.cpp')
assert 'const f32 lfDepth = lMeshClipBox.wAxis.w;' in commands
predicate=re.search(r'!\(lfDepth > lpContext->mvPreZDistanceThreshold\[0\]\)',commands)[0]
code='void PreZRenderer::Defaults(){\n'+defaults+'\n}\n'+body
code+='\nbool Admitted(const CgsGraphics::DispatchObjectContext* lpContext,float lfDepth){return '+predicate+';}\n'
result=compile_and_run(Path(__file__).with_name('PCPreZRange.cpp'),'pc_prez_range.inc',code,'PCPreZRange')
raise SystemExit(report('run_pc_prez_range',[],result,7))
