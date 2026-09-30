"""Production compressed stages/interface/zlib; disk and job scheduling are explicit boundaries."""
import argparse,random,zlib
from pathlib import Path
from fxgs_common import REPO,Tree,definition,compile_and_run,report,STRSTREAM_CPP

parser=argparse.ArgumentParser()
parser.add_argument('--old-stage',action='store_true')
parser.add_argument('--drop-carry',action='store_true')
args=parser.parse_args()
tree=Tree();base='src/GameShared/GameClasses/';resource=base+'System/Resource/'
jobs='src/GameShared/Jobs/DecompressionJob/'
stages=tree.read(resource+'CgsBundleLoaderModule.cpp')
if args.old_stage:
    sig='bool BundleLoaderModule::StreamCompressedDataAsJobFunc('
    stages=stages.replace(definition(stages,sig),definition(Tree('dd55406d').read(resource+'CgsBundleLoaderModule.cpp'),sig))
interface=tree.read(jobs+'DecompressionJobInterface.cpp')
if args.drop_carry:
    old='mpEntries[0] = lLast;'
    assert interface.count(old)==1
    interface=interface.replace(old,'mpEntries[0] = {};')
driver=tree.read(resource+'CgsResourceBundleLoaderModule.cpp')
adapter='namespace CgsResource {\n'+definition(driver,'void BundleLoaderModule::Construct()')+'\n}\n'
hash_code='#include "GameShared/GameClasses/Core/CgsAssert.h"\n#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"\nnamespace CgsResource {\n'
hash_code+=definition(tree.read(resource+'CgsResourceID.cpp'),'s32 ID::HashString(')+'\n}\n'
payload=random.Random(371).randbytes(700000)
arrays=''
for name,data in [('Payload',payload),('Packed',zlib.compress(payload)),('SmallPacked',zlib.compress(payload[:64]))]:
    arrays+='static const unsigned char '+name+'[]={'+','.join(map(str,data))+'};\n'
shadow={
 'src/GameShared/GameClasses/Module/CgsModuleSingleBuffered.h':
    '#pragma once\nnamespace CgsModule { struct ModuleSingleBuffered { bool mbIsNewModule=false; void Construct(){} }; }\n',
 'src/GameShared/GameClasses/Memory/CgsHeapMalloc.h':
    '#pragma once\n#include "types.hpp"\nnamespace CgsMemory { class HeapMalloc { public:\n'
    'void Construct(void*,s32){} void* Malloc(s32,s32); void Free(void*); int outstanding=0; }; }\n',
 'src/SDKs/EATech/eajobs/job.h':'''#pragma once
#include "SDKs/EATech/eajobs/job_types.h"
namespace EA { namespace Jobs {
struct Job {
 using Entry=void(*)(Param,Param,Param,Param);
 Entry entry=nullptr; void* data=nullptr; unsigned size=0; bool done=true;
 explicit Job(const char*){}
 void Clear(){entry=nullptr;data=nullptr;size=0;done=false;}
 void SetCode(JobEnvironment,const void* p,unsigned){entry=reinterpret_cast<Entry>(const_cast<void*>(p));}
 void SetData(void* p,unsigned n){data=p;size=n;}
 void SetName(const char*){}
 bool IsDone() const{return done;}
 void Run(){entry(Param(reinterpret_cast<void*>(0x1234)),Param(data),Param(),Param());done=true;}
 void WaitOn(){if(!done)Run();}
}; }}
''',
 'src/SDKs/EATech/eajobs/job_scheduler.h':'''#pragma once
#include <cstdlib>
#include "SDKs/EATech/eajobs/job.h"
namespace EA { namespace Jobs { class JobScheduler { public:
 Job* pending=nullptr; int submitted=0;
 void AddJobs(Job* p,int count){if(count!=1||pending)std::abort();pending=p;++submitted;}
 void Complete(){if(pending){if(!pending->done)pending->Run();pending=nullptr;}}
}; }}
'''
}
numeric=compile_and_run(Path(__file__).with_name('PCBundleCompressed.cpp'),
    'pc_bundle_compressed.inc',adapter,'PCBundleCompressed',shadow=shadow,extra_flags='/F8388608',
    extra_files={'pc_compressed_payload.inc':arrays,'pc_bundle_compressed.cpp':stages,
                 'pc_bundle_hash.cpp':hash_code,'pc_decompression_interface.cpp':interface},
    extra_sources=['pc_bundle_compressed.cpp','pc_bundle_hash.cpp','pc_decompression_interface.cpp',
        REPO/jobs/'CgsDecompressor.cpp',STRSTREAM_CPP,
        *[REPO/base/'Module'/n for n in ['CgsIOBuffer.cpp','CgsBaseEventReceiverQueue.cpp']],
        REPO/base/'Containers/CgsPriorityQueue.cpp',
        *[REPO/resource/n for n in ['CgsResourceBundle2.cpp','CgsResourceIOEvents.cpp',
         'CgsBundleLoaderModuleIO_InputBuffer.cpp','CgsBundleLoaderModuleIO_InputBuffer_Update.cpp',
         'CgsBundleLoaderModuleIO_InputBuffer_Record.cpp','CgsBundleLoaderModuleIO_OutputBuffer.cpp']],
        *[REPO/'vendor/zlib/src'/n for n in ['inflate.c','inftrees.c','inffast.c','adler32.c','crc32.c','zutil.c']]])
raise SystemExit(report('run_pc_bundle_compressed',[],numeric,15))
