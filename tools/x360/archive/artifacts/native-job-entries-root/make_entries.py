from pathlib import Path
import hashlib, json, subprocess

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
SPECS = [
    ('Relocator', 'RelocatorJob', 'gaRelocatorJobs', 'KI_NUM_RELOCATOR_JOBS', 0xC80, 55, '82AD28C0'),
    ('RadixSort', 'RadixSortJob', 'gaRadixSortJobs', '6', 1, 59, '82AD2020'),
    ('ContactGenerator', 'ContactGeneratorJob', 'gaContactGeneratorJobs', 'KI_NUM_CONTACT_GENERATOR_JOBS', 0x10300, 55, '82920F10'),
    ('PolygonSoupTester', 'PolygonSoupTesterJob', 'gaPolygonSoupTesterJobs', 'KI_NUM_POLYGON_SOUP_TESTER_JOBS', 0x19040, 55, '829157B8'),
]
assert subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip() == '13bad667d' + subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()[9:]
assert not subprocess.check_output(['git', '-C', str(ROOT), 'diff', '--name-only']).strip()
base = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
before = {}

def save(path, text):
    p = ROOT / path
    if p.exists():
        raw = p.read_bytes()
        backup = HERE / (p.name + '.before')
        with backup.open('xb') as stream:
            stream.write(raw)
        before[path] = {'sha256': hashlib.sha256(raw).hexdigest(), 'backup': str(backup)}
    else:
        before[path] = None
    p.write_bytes(text.replace('\n', '\r\n').encode('utf-8'))

for folder, cls, array, count, stride, line, address in SPECS:
    path = 'src/GameShared/Jobs/%s/%s.cpp' % (folder, folder)
    source = r'D:\P4\B5_MAIN\Burnout\MAIN\Code\GameShared\Jobs\%s\%s.cpp' % (folder, folder)
    csource = json.dumps(source)
    includes = '#include "GameShared/Jobs/%s/%s.h"\n' % (folder, cls)
    includes += '#include "GameShared/Jobs/CgsJobAssertBuffer.h"\n'
    includes += '#include "GameShared/GameClasses/Core/CgsAssert.h"\n'
    includes += '#include "GameShared/GameClasses/Development/CgsStrStream.h"\n'
    includes += '#include "SDKs/EATech/eajobs/job_types.h"\n'
    if folder == 'PolygonSoupTester':
        includes += '#include "SDKs/EATech/eajobs/job_thread_id.h"\n'
        thread = 'EA::Jobs::GetJobThreadId()'
        index = '(luThreadId - 0xF8000088ull) / 4'
    else:
        includes += '#include "SDKs/EATech/eathread/BrnEAThreadX360.h"\n'
        thread = 'static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()))'
        index = '(luThreadId - 0xF8000088ull) >> 2'
    array_decl = ('\nnamespace { RadixSortJob gaRadixSortJobs[6]; }\n' if folder == 'RadixSort' else '\n')
    arg = ('static_cast<SortInfo*>(lParam1.mpValue)' if folder == 'RadixSort' else 'lParam1.mpValue')
    if folder == 'Relocator':
        arg += ', static_cast<u32>(luJobThreadIndex)'
    text = includes + array_decl + '''
CGS_STATIC_ASSERT(sizeof(%s) == 0x%X, "ARTIST worker array stride");

// ARTIST %s: native worker handle selects one of six contexts.
void %sEntry(EA::Jobs::Param, EA::Jobs::Param lParam1,
             EA::Jobs::Param, EA::Jobs::Param)
{
    const u64 luThreadId = %s;
    const u64 luJobThreadIndex = %s;
    if (luJobThreadIndex >= %s)
    {
        CgsDev::StrStream lMessage(byte_83015F20, sizeof(byte_83015F20));
        const u64 luReportedThreadId = %s;
        lMessage << "SPU Id out of range: " << luReportedThreadId << "\\n";
        const char* lpcSource = %s;
        CgsDev::Assert::PrintStringed(lpcSource, 0, %s, lMessage.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
        CgsDev::Assert::PrintStringed(lpcSource, 0, %s, lMessage.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
    }
    %s[static_cast<u32>(luJobThreadIndex)].Execute(%s);
}
''' % (cls, stride, address, folder, thread, index, count, thread, csource, line, line, array, arg)
    save(path, text)

save('src/SDKs/EATech/eajobs/job_thread_id.h', '''#pragma once

#include "types.hpp"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

namespace EA { namespace Jobs {
typedef u64 JobThreadId;

// ARTIST 82915920 and 829158F8, respectively.
JobThreadId ToJobThreadId(EA::Thread::ThreadId lThreadId);
JobThreadId GetJobThreadId();
}}
''')
save('src/SDKs/EATech/eajobs/job_thread_id.cpp', '''#include "SDKs/EATech/eajobs/job_thread_id.h"

namespace EA { namespace Jobs {
JobThreadId ToJobThreadId(EA::Thread::ThreadId lThreadId)
{
    return static_cast<JobThreadId>(reinterpret_cast<uintptr_t>(lThreadId));
}

JobThreadId GetJobThreadId()
{
    return ToJobThreadId(EA::Thread::GetThreadId());
}
}}
''')

with (HERE / 'before-edit.json').open('x', encoding='utf-8') as stream:
    json.dump({'base': base, 'files': before, 'specs': SPECS}, stream, indent=2)
print('Wrote four native entries and two genuine SDK job-id providers.')
