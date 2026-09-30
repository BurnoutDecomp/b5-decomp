"""Native production decompressor and four-parameter job entry with real zlib."""
import argparse,zlib
from pathlib import Path
from fxgs_common import REPO,Tree,definition,compile_and_run,report

parser=argparse.ArgumentParser()
parser.add_argument('--old-init',action='store_true')
args=parser.parse_args()
path='src/GameShared/Jobs/DecompressionJob/CgsDecompressor.cpp'
source=Tree().read(path)
if args.old_init:
    old=Tree('087d7dae').read(path)
    for signature in ['s32 Decompressor::BeginDecompressingFirstEntry()',
                      's32 Decompressor::BeginDecompressingNextEntry()']:
        source=source.replace(definition(source,signature),definition(old,signature))
arrays=''
for name,size,salt in [('A',65536,17),('B',32768,91)]:
    data=bytes(((i*29+(i>>8)*salt) ^ (i>>4)) & 255 for i in range(size))
    compressed=zlib.compress(data)
    arrays+=f'static const unsigned char compressed{name}[]={{'+','.join(map(str,compressed))+'};\n'
shadow={'src/GameShared/GameClasses/Memory/CgsHeapMalloc.h':
    '#pragma once\n#include "types.hpp"\nnamespace CgsMemory { class HeapMalloc { public:\n'
    'void* Malloc(s32,s32); void Free(void*); int outstanding=0; }; }\n'}
numeric=compile_and_run(Path(__file__).with_name('PCDecompressionWorker.cpp'),
    'pc_compressed_data.inc',arrays,'PCDecompressionWorker',shadow=shadow,
    extra_files={'pc_decompression_worker.cpp':source},
    extra_sources=['pc_decompression_worker.cpp',*[REPO/'vendor/zlib/src'/name for name in
        ['inflate.c','inftrees.c','inffast.c','adler32.c','crc32.c','zutil.c']]])
raise SystemExit(report('run_pc_decompression_worker',[],numeric,14))
