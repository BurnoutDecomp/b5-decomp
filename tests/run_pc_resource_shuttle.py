"""Production request routing/update ordering with observable module/device boundaries."""
import os
import argparse
from pathlib import Path
from fxgs_common import REPO,Tree,definition,compile_and_run,report,STRSTREAM_CPP
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=Path)
args = parser.parse_args()
resource='src/GameShared/GameClasses/System/Resource/'
source=Tree().read(resource+'CgsResourceModule.cpp')
names=['bool ResourceModule::AddOpenReadStreamRequest(',
       'bool ResourceModule::AddCloseReadStreamRequest(',
       'void ResourceModule::ProcessPendingFileSystemResponses(',
       'void ResourceModule::ProcessResourceRequests(const ResourceIO::InputBuffer* lpResIn,',
       'void ResourceModule::ProcessBundleLoaderStreamRequests(',
       'void ResourceModule::ProcessPoolOutputResponses(',
       'void ResourceModule::ProcessResourceResponses(',
       'void ResourceModule::ProcessPoolResourceRequests(',
       'void ResourceModule::ProcessMemoryResponses(',
       'bool ResourceModule::Update(']
code='namespace CgsResource {\n'+ '\n'.join(definition(source,n).replace('ResourceModule::','Shuttle::') for n in names)+'\n}\n'
# This transport fixture exercises lifecycle/accessors, not AppendResourceQueue.
# The hierarchy fixture links and exercises that complete production provider.
io_source=Tree().read(resource+'CgsResourceModuleIO.cpp')
io_names=['void FileSystemStatusInterface::Construct(', 'void OutputBuffer::Construct(',
          'void InputBuffer::Construct(', 'void InputBuffer::Destruct(',
          'const ResourceRequestQueue<16384>* InputBuffer::GetResourceQueue() const',
          'const FileSystemStatusInterface* OutputBuffer::GetFileSystemStatusInterface() const',
          'FileSystemStatusInterface* OutputBuffer::GetFileSystemStatusInterface()\n']
code+='namespace CgsResource { namespace ResourceIO {\n'+ '\n'.join(
    definition(io_source,n) for n in io_names)+'\n} }\n'
base='src/GameShared/GameClasses/'
sources=[STRSTREAM_CPP, REPO/'vendor/renderware/src/rw/BaseResourceDescriptor.cpp']
sources += [REPO/base/'Core/CgsStringUtils.cpp']
sources += [REPO/base/'Module'/n for n in ['CgsIOBuffer.cpp','CgsBaseEventReceiverQueue.cpp']]
sources += [REPO/resource/n for n in ['CgsResourceIOEvents.cpp',
    'CgsResourceModuleIO_InputBuffer_GetResourceQueue.cpp',
    'CgsPoolModuleIO_OutputBuffer.cpp','CgsBundleLoaderModuleIO_InputBuffer.cpp',
    'CgsBundleLoaderModuleIO_InputBuffer_Update.cpp','CgsBundleLoaderModuleIO_InputBuffer_Record.cpp',
    'CgsBundleLoaderModuleIO_OutputBuffer.cpp']]
sources += [REPO/base/'Memory'/n for n in ['CgsMemoryModuleIO.cpp']]
result=compile_and_run(Path(__file__).with_name('PCResourceShuttle.cpp'),
    'pc_resource_shuttle.inc',code,'PCResourceShuttle',extra_sources=sources,output_dir=args.output_dir)
raise SystemExit(report('run_pc_resource_shuttle',[],result,14))
