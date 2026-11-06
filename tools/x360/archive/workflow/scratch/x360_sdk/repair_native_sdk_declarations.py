"""Run once AFTER the frozen full-native-thread-sync sweep is terminal."""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
report=json.loads((ROOT/'build/x360/full-native-thread-sync/compile-results.json').read_text())
assert report['state']=='complete','Do not mutate source during the frozen sweep'

def write(name,text):
    (ROOT/name).write_bytes(text.replace('\n','\r\n').encode('utf-8'))

pool='src/GameShared/GameClasses/System/FileSystem/CgsDeviceOperationPool'
s=(ROOT/(pool+'.cpp')).read_text()
a=s.index('// OS critical-section hooks')
b=s.index('namespace CgsFileSystem',a)
s=s[:a]+'// Use the native SDK critical-section declarations.\n'+s[b:]
s=s.replace('&mFutex)', '&mFutex.mCriticalSection)')
write(pool+'.cpp',s)

s=(ROOT/(pool+'.h')).read_text()
a=s.index('// Forked-vendor EA::Thread::Semaphore')
b=s.index('// CgsFileSystem::OperationPool',a)
s=s[:a]+'#include "eathread/eathread_semaphore.h"\n#include <xtl.h>\n\n'+s[b:]
a=s.index('    // One queued file-system operation.')
b=s.index('    struct OperationPool',a)
s=s[:a]+'''    // Native Futex platform storage occupies32 bytes; the SDK critical section
    // fits in its first28. The original flag table begins atpool+20hex.
    union OperationFutex
    {
        u64 mData[4];
        CRITICAL_SECTION mCriticalSection;
    };

    CGS_STATIC_ASSERT((sizeof(OperationFutex) == 0x20), "ARTIST operation futex");
    CGS_STATIC_ASSERT((sizeof(Operation) == 0x130), "ARTIST operation stride");
    CGS_STATIC_ASSERT((offsetof(Operation, miPriority) == 0x12C), "ARTIST operation priority");

'''+s[b:]
needle='        EA::Thread::Semaphore   mOperationCount;'
a=s.index('    };',s.index(needle))
s=s[:a]+'''
        void CheckLayout()
        {
            CGS_STATIC_ASSERT((offsetof(OperationPool, mabUsedOperations) == 0x20), "ARTIST operation flags");
            CGS_STATIC_ASSERT((offsetof(OperationPool, mauOperationNumbers) == 0x60), "ARTIST operation sequence table");
            CGS_STATIC_ASSERT((offsetof(OperationPool, maOperations) == 0x260), "ARTIST operations");
            CGS_STATIC_ASSERT((offsetof(OperationPool, muNextOperationNumber) == 0x4E60), "ARTIST operation sequence");
            CGS_STATIC_ASSERT((offsetof(OperationPool, mOperationCount) == 0x4E68), "ARTIST operation semaphore");
        }
'''+s[a:]
write(pool+'.h',s)

p='src/GameShared/GameClasses/System/PS3/CgsHardwareInitPS3.cpp'
s=(ROOT/p).read_text()
a=s.index('// ---------------------------------------------------------------------------\n// X360 platform (XDK) APIs')
b=s.index('// ---------------------------------------------------------------------------\n// Static storage',a)
s=s[:a]+s[b:]
s='#include <xtl.h>\n'+s
write(p,s)

p='src/GameSource/Game/X360/BrnSystemHWX360Massive.cpp'
s=(ROOT/p).read_text()
a=s.index('// XDK sign-in query')
b=s.index('// ---------------------------------------------------------------------------',a)
s=s[:a]+s[b:]
s='#include <xtl.h>\n'+s
write(p,s)
print('Recovered operation-pool storage and removed conflicting SDK redeclarations')
