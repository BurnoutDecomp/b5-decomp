"""Exercise production bundle ownership, GUI registration and Apt callback bodies.

File IO, heap allocation, resource fixups, parser/VM completion, TLS and shared-pointer
primitives are fixture boundaries. Use the explicit negative-control switches for
missing pins, the rejected zero-resource GUI gate and the old free-state offset.
--rev selects a b5 revision that supplies the required production bodies.
"""
import argparse
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, REPO

parser = argparse.ArgumentParser()
parser.add_argument('--rev')
parser.add_argument('--legacy-free-state-offset', action='store_true', help='negative control: restore the old FreeAnimation state store at byte0x14')
parser.add_argument('--require-new-apt-resources', action='store_true', help='negative control: restore the rejected successful-zero-load GUI registration gate')
parser.add_argument('--omit-apt-retain', action='store_true', help='negative control: remove the real bundle reference acquisition')
parser.add_argument('--skip-malformed', action='store_true', help='allow a pre-validation negative control to finish')
args = parser.parse_args()
tree = Tree(args.rev)
base = 'src/GameShared/GameClasses/System/Resource/'
source = tree.read(base + 'CgsResourceBundleLoader.cpp')
if args.omit_apt_retain:
    retain = definition(source, 'static s32 RetainBundle(')
    source = source.replace(retain, 'static s32 RetainBundle(Bundle&) { return 0; }')
code = 'namespace CgsResource {\n'
for name, signatures in [
    ('CgsResourceID.cpp', ['s32 ID::HashString(']),
    ('CgsResourceBundle2.cpp', ['u32 BundleV2::ResourceEntry::GetUncompressedSize(',
                              'u32 BundleV2::ResourceEntry::GetUncompresssedAlignment(']),
    ('CgsResourcePool.cpp', ['Entry* Pool::FindResource(', 'Entry* Pool::FindResourceWithDependencies(',
                           's32 Pool::FindResourceIndexWithDependencies(',
                           's16  Pool::GetEntryRefCount(', 'void Pool::SetEntryRefCount(',
                           'void Pool::IncEntryRefCount(', 'void Pool::DecEntryRefCount(',
                           'void Pool::SetEntryStatus(']),
]:
    content = tree.read(base + name)
    code += '\n'.join(definition(content, signature) for signature in signatures) + '\n'
code += '}\n' + source[source.index('// The PC bundle loader.'):]
# Exercise the actual GUI registry, unload retirement, load-result gate and
# streamed ResourceHandle lookup used by ServicePlatformRequests. No copied gate.
gui = 'src/GameShared/GameClasses/Gui/'
pc = tree.read(gui + 'Model/Resources/CgsGuiResourceModulePC.cpp')
registry_start = pc.index('        const u32 KU_MAX_APT_BUNDLE_LEADS =')
registry_end = pc.index('        bool IsAptDataAttributed(', registry_start)
constants = []
for name in ['KU_APT_STREAMED_BANK_MAX_NODES', 'KU_APTDATA_RESOURCE_TYPE_ID']:
    constants.append(re.search(r'^        const u32 ' + name + r'\s*=.*$', pc, re.MULTILINE).group(0))
code += '\nnamespace CgsGui {\n' + '\n'.join(constants) + '\n' + pc[registry_start:registry_end]
for signature in [
    'bool IsAptDataAttributed(', 'void RecordAptBundleLead(',
    'void RetireUnloadedAptRegistrations(', 'void ParseAptBundleName(',
    'void RecordAptBundleLoadResult(',
    'CgsResource::ResourceHandle AcquireStreamedAptResource(',
    'void* GetLoadedAptBundleLeadHeader(',
]:
    body = definition(pc, signature)
    if args.require_new_apt_resources and signature == 'void RecordAptBundleLoadResult(':
        assert body.count('if (liLoaded < 0)') == 1
        body = body.replace('if (liLoaded < 0)', 'if (liLoaded <= 0)')
    code += body + '\n'
code += '}\n'
# Execute the production callback, free hook and destructor bodies against the
# typed parser/TLS/shared-pointer boundaries in pc_apt_callback_fixture.h.
aux_callbacks = tree.read(gui + 'View/AptInterface/CgsAptAux.cpp')
free_callback = definition(aux_callbacks, 'void AptCallbackFile::FreeAnimation(')
if args.legacy_free_state_offset:
    free_callback = free_callback.replace(
        'static_cast<AptDataHeader*>(lpDataBlock)->meCurrentState',
        '*reinterpret_cast<u32*>(static_cast<char*>(lpDataBlock) + 0x14)')
code += '\nnamespace CgsGui {\n' + free_callback + '\n'
code += definition(aux_callbacks, 'void AptCallbackFile::LoadAnimation(') + '\n}\n'
native_file = tree.read('src/SDKs/EATech/include/Apt/AptFile.cpp')
code += definition(native_file, 'void AptFile_FreeLoadedBlock(') + '\n'
code += definition(native_file, 'AptFile::~AptFile(') + '\n'
numeric = compile_and_run(
    Path(__file__).with_name('PCBundleOwnership.cpp'), 'pc_bundle_ownership.inc', code,
    'PCBundleOwnership', extra_flags='/D_CRT_SECURE_NO_WARNINGS' + (' /DPC_BUNDLE_SKIP_MALFORMED' if args.skip_malformed else '')
        + (' /DPC_APT_SKIP_CALLBACKS' if args.omit_apt_retain or args.require_new_apt_resources else ''),
    extra_sources=[REPO / base / 'CgsResourceTypeBase.cpp', REPO / base / 'CgsEntryListResource.cpp',
                   REPO / 'vendor/renderware/src/rw/BaseResourceDescriptor.cpp'])
gui = 'src/GameShared/GameClasses/Gui/'
aux = tree.read(gui + 'View/AptInterface/CgsAptAux.cpp')
load = definition(aux, 'void AptCallbackFile::LoadAnimation(')
file_source = tree.read('src/SDKs/EATech/include/Apt/AptFile.cpp')
free = definition(file_source, 'void AptFile_FreeLoadedBlock(')
dtor = definition(file_source, 'AptFile::~AptFile(')
remove = definition(tree.read(gui + 'View/AptInterface/CgsAptDataHandler.cpp'),
                    'void AptDataHandler::RemoveAptData(')
service = definition(pc, 'void GuiResourceModule::ServicePlatformRequests(')
streamed_start = service.index('else if (lpRequest->miPoolId == miAptStreamedBundleBank)')
streamed_end = service.index('else if (lpRequest->miPoolId == miAptPersistentBundleBank)', streamed_start)
streamed_load = service[streamed_start:streamed_end]
wiring = [
    ('production streamed load sends every load result to the tested registration gate',
     'RecordAptBundleLoadResult(lpPool, lpRequest->macFileName, liLoaded);' in streamed_load
     and 'if (liLoaded > 0)' not in streamed_load),
    ('production streamed acquire uses the tested ResourceHandle lookup',
     'lResponse.mResourceHandle = AcquireStreamedAptResource(luReqHash);' in service),
    ('native LoadAnimation publishes a root only after real bundle acquisition',
     load.index('PCAptOwnership::Retain(') < load.index('AptCompleteAnimationAsyncLoad(')),
    ('native ACTIVE state is restored after successful completion',
     load.index('AptCompleteAnimationAsyncLoad(') < load.index('meCurrentState = AptDataHeader::E_APTDATASTATE_ACTIVE')),
    ('resource release follows native Unresolve and FreeAnimation',
     dtor.index('pAnimation->Unresolve(') < dtor.index('AptFile_FreeLoadedBlock(')
     and free.index('gpAptFreeAnimationHook(pDataBlock)') < free.index('PCAptOwnership::Release(')),
    ('unregister does not clear roots still referenced by importing movies',
     'InvalidateLoadedData' not in remove),
]
expected_numeric = 81 if args.omit_apt_retain or args.require_new_apt_resources else 97
if numeric is not None and numeric[0] != expected_numeric:
    print(f'FAIL  expected {expected_numeric} numeric checks, executed {numeric[0]}')
    raise SystemExit(1)
raise SystemExit(report('run_pc_bundle_ownership', wiring, numeric, expected_numeric))
