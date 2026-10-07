"""Actual replay debug allocation/graph methods; observed allocator, no producer/UI execution.

--wrong-alignment changes the temporary original descriptor request only.
"""
import argparse
import os
import re
import sys
from pathlib import Path
sys.dont_write_bytecode=True
from fxgs_common import REPO,Tree,compile_and_run,definition

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--wrong-alignment',action='store_true')
args=parser.parse_args()
tree=Tree()
source=tree.read('src/GameSource/Replays/BrnReplayDebugComponent.cpp')
construct=definition(source,'void DebugComponent::Construct(')
if args.wrong_alignment:
    construct=construct.replace('m_alignment = 16;','m_alignment = 8;',1)
bodies='namespace BrnReplays {\n'+construct+'\n'+definition(source,'void DebugComponent::ClearGraph(')+'\n}\n'
# Actual canonical base constructor and virtual defaults close the C++ object
# lifetime; no copied class layout or fabricated base implementation is used.
base=tree.read('src/GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.cpp')
names=('Update','RenderWorld','RenderHUD','GetName','GetPath','IsSimple','OnActivate','OnRegister')
base_bodies=[definition(base,'    DebugComponent::DebugComponent()')]
for name in names:
    match=re.search(r'(?m)^ *[^\n]+DebugComponent::'+name+r'\([^\n]*',base)
    base_bodies.append(definition(base,match.group(0)))
bodies+='\nnamespace CgsDev {\n'+'\n'.join(base_bodies)+'\n}\n'
numeric=compile_and_run(Path(__file__).with_name('ReplayDebugConstruction.cpp'),
    'replay_debug_construction.inc',bodies,'ReplayDebugConstruction',extra_sources=[
        REPO/'vendor/renderware/src/rw/BaseResourceDescriptor.cpp'])
if numeric is None: raise SystemExit(1)
checks,failures=numeric
print(f'run_replay_debug_construction: {checks-failures}/{checks} pass ({failures} fail)')
raise SystemExit(bool(failures))
