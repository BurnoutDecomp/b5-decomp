"""Production request routing/update ordering with observable module/device boundaries."""
import os
from pathlib import Path
from fxgs_common import REPO,Tree,definition,compile_and_run,report,STRSTREAM_CPP
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
resource='src/GameShared/GameClasses/System/Resource/'
source=Tree().read(resource+'CgsResourceModule.cpp')
names=['bool ResourceModule::AddOpenReadStreamRequest(',
       'bool ResourceModule::AddCloseReadStreamRequest(',
       'void ResourceModule::ProcessPendingFileSystemResponses(',
       'void ResourceModule::ProcessResourceRequests(ResourceIO::InputBuffer* lpResIn,\n',
       'void ResourceModule::ProcessBundleLoaderStreamRequests(',
       'void ResourceModule::ProcessPoolOutputResponses(',
       'void ResourceModule::ProcessResourceResponses(',
       'void ResourceModule::ProcessPoolResourceRequests(',
       'void ResourceModule::ProcessMemoryResponses(',
       'bool ResourceModule::Update(']
code='namespace CgsResource {\n'+ '\n'.join(definition(source,n).replace('ResourceModule::','Shuttle::') for n in names)+'\n}\n'
base='src/GameShared/GameClasses/'
sources=[STRSTREAM_CPP]
sources += [REPO/base/'Module'/n for n in ['CgsIOBuffer.cpp','CgsBaseEventReceiverQueue.cpp']]
sources += [REPO/resource/n for n in ['CgsResourceIOEvents.cpp','CgsResourceModuleIO.cpp',
    'CgsResourceModuleIO_InputBuffer_GetResourceQueue.cpp',
    'CgsPoolModuleIO_OutputBuffer.cpp','CgsBundleLoaderModuleIO_InputBuffer.cpp',
    'CgsBundleLoaderModuleIO_InputBuffer_Update.cpp','CgsBundleLoaderModuleIO_InputBuffer_Record.cpp',
    'CgsBundleLoaderModuleIO_OutputBuffer.cpp']]
sources += [REPO/base/'Memory'/n for n in ['CgsMemoryModuleIO.cpp']]
result=compile_and_run(Path(__file__).with_name('PCResourceShuttle.cpp'),
    'pc_resource_shuttle.inc',code,'PCResourceShuttle',extra_sources=sources)
raise SystemExit(report('run_pc_resource_shuttle',[],result,13))
