"""Original pending-reset sweep and empty-queue caller contract; CPU only."""
from pathlib import Path
import argparse
import os
import re
import sys
from fxgs_common import Tree,definition,code_only,compile_and_run,report

parser=argparse.ArgumentParser()
parser.add_argument('--old-caller',action='store_true',help='captured published c6789ddf deferred caller')
parser.add_argument('--early-clear',action='store_true',help='private control clears before request admission')
args=parser.parse_args()
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
tree=Tree()
manager=definition(tree.read('src/GameSource/Physics/DeformationManager/BrnDeformationManager.cpp'),
                   '    void DeformationManager::ProcessDebugResetDeformationModels(')
if args.early_clear:
    needle='            if (mpaModels[liModelIndex].ShouldResetDeformationNextUpdate())'
    assert manager.count(needle)==1
    manager=manager.replace(needle,'            mpaModels[liModelIndex].ResetDeformationNextUpdate(false);\n'+needle)
header=tree.read('src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h')
accessors='\n'.join(definition(header,s) for s in [
    'void ResetDeformationNextUpdate(bool lbReset)',
    'bool ShouldResetDeformationNextUpdate() const'])
caller_tree=Tree('c6789ddf10c7ebee0a0864807038dcbb38e7f741') if args.old_caller else tree
caller=definition(caller_tree.read('src/GameSource/Physics/BrnPhysicsModule.cpp'),
                  '    void PhysicsModule::HandleGameActionsPostScene(')
# Preserve the production prefix verbatim; the event loop is outside this test.
prefix=caller[:caller.index('        while (lpEventData)')]+'\n    }'
code=code_only(caller);compact=re.sub(r'\s+','',code)
call='mDeformationManager.ProcessDebugResetDeformationModels(lpSimModuleInputBuffer,lpSceneInterface);'
order=call in compact and code.index('GetFirstEvent(')<code.index('mDeformationManager.ProcessDebugResetDeformationModels(')<code.index('while (lpEventData)')
wiring=[('original caller arguments/order before empty-queue boundary',order),
        ('source caller no longer substitutes ReportDeferral','ReportDeferral' not in code_only(prefix))]
methods='namespace BrnPhysics { namespace Deformation {\n'+manager+'\n} }\nnamespace BrnPhysics {\n'+prefix+'\n}\n'
numeric=compile_and_run(Path(__file__).with_name('DeformationPendingReset.cpp'),
    'deformation_pending_reset.inc',methods,'DeformationPendingReset',
    extra_files={'deformation_pending_accessors.inc':accessors})
raise SystemExit(report('run_deformation_pending_reset',wiring,numeric,40))
