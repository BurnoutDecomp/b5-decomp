"""Exercise production synchronous bundle ownership; --rev HEAD is the pre-fix control.

The loader, ID hash, size getters and pool reference/lookup accessors are extracted unchanged.
Only file IO, heap allocation and resource fixup callbacks are fixture boundaries.
"""
import argparse
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, REPO

parser = argparse.ArgumentParser()
parser.add_argument('--rev')
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
numeric = compile_and_run(
    Path(__file__).with_name('PCBundleOwnership.cpp'), 'pc_bundle_ownership.inc', code,
    'PCBundleOwnership', extra_flags='/D_CRT_SECURE_NO_WARNINGS' + (' /DPC_BUNDLE_SKIP_MALFORMED' if args.skip_malformed else ''),
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
wiring = [
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
raise SystemExit(report('run_pc_bundle_ownership', wiring, numeric, 68))
