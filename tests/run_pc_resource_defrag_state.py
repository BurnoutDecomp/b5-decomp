"""Original defrag polling at a planner/allocator boundary; --old-poll is the control."""
import argparse
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, STRSTREAM_CPP, REPO

parser = argparse.ArgumentParser()
parser.add_argument('--old-poll', action='store_true')
args = parser.parse_args()
base = 'src/GameShared/GameClasses/System/Resource/'
current = Tree()
poll = Tree('64f9b755' if args.old_poll else None)
code = 'namespace CgsResource {\n'
for source, signatures in [
    (current.read(base + 'CgsResourcePool.cpp'), ['bool Pool::IsDefragmenting(',
        's32  Pool::GetDefragMemType(', 's32  Pool::GetNumEntriesInPurgatory(']),
    (current.read(base + 'PoolModuleStates/CgsBaseDefragPoolModuleState.cpp'),
        ['Pool* BaseDefragPoolModuleState::GetPool(',
         'EBatchAllocResult BaseDefragPoolModuleState::GetAllocationResult(']),
    (poll.read(base + 'PoolModuleStates/CgsIntelliFragPoolModuleState.cpp'),
        ['IntelliFragPoolModuleState::EIntelliFragResult IntelliFragPoolModuleState::Update(']),
    (poll.read(base + 'PoolModuleStates/CgsEmergencyFragPoolModuleState.cpp'),
        ['EmergencyFragPoolModuleState::EEmergencyFragResult EmergencyFragPoolModuleState::Update(']),
]:
    code += '\n'.join(definition(source, signature) for signature in signatures) + '\n'
code += '}\n'
numeric = compile_and_run(Path(__file__).with_name('PCResourceDefragState.cpp'),
    'pc_resource_defrag_state.inc', code, 'PCResourceDefragState', extra_sources=[STRSTREAM_CPP,
        REPO / 'vendor/renderware/src/rw/BaseResourceDescriptor.cpp'])
raise SystemExit(report('run_pc_resource_defrag_state', [], numeric, 20))
