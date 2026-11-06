"""One-time native clock/sleep/processor and Realmc wait restoration."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'


def edit(name,transform):
    p=ROOT/name;s=p.read_text(encoding='utf-8');s=transform(s)
    p.write_bytes(s.replace('\r\n','\n').replace('\n','\r\n').encode('utf-8'))


def facade(s):
    a=s.index('                // X360 calls XSetThreadProcessor')
    b=s.index('\n            }',a)
    s=s[:a]+'''                XSetThreadProcessor(lpPub->mhThread, static_cast<DWORD>(liProcessor % 6));'''+s[b:]
    a=s.index('    // ---- EA::Thread::Futex')
    s=s[:a]+'''    // ARTIST82B42628: biased 32-bit millisecond clock used by absolute deadlines.
    u32 GetThreadTime()
    {
        return ::GetTickCount() + 0x2710u;
    }

'''+s[a:]
    s=s.replace('    // FLAG: _beginthreadex\'s start routine is unsigned(__stdcall*)(void*); the\n    // X360 routine\'s intptr_t return is folded into that ABI on the host.',
                '    // The native XDK CRT entry takes unsigned(__stdcall*)(void*).')
    return s


edit('src/SDKs/EATech/eathread/BrnEAThreadX360.cpp',facade)


def facade_header(s):
    s=s.replace('#include "types.hpp"', '#include "types.hpp"\n#include <cstddef>\n#include <xtl.h>   // native HANDLE and OS declarations',1)
    a=s.index('// On the MSVC/Windows host the X360 OS primitives')
    b=s.index('namespace EA',a);s=s[:a]+s[b:]
    needle='    u32      ThreadSleep(const u32* lpuMilliseconds);'
    assert s.count(needle)==1
    s=s.replace(needle,needle+'\n    // 0x82B42628: GetTickCount()+10000, with native 32-bit wrap.\n    u32      GetThreadTime();')
    a=s.index('    // FLAG: +0x04 is never touched by this TU')
    b=s.index('    struct EAThreadDynamicData',a);s=s[:a]+s[b:]
    needle='    // X360: 24 statically-allocated EAThreadDynamicData slots'
    a=s.index(needle)
    s=s[:a]+'''    CGS_STATIC_ASSERT((sizeof(EAThreadDynamicData) == 0x44), "ARTIST thread record stride");
    CGS_STATIC_ASSERT((offsetof(EAThreadDynamicData, mnThreadId) == 4), "ARTIST OS thread id");
    CGS_STATIC_ASSERT((offsetof(EAThreadDynamicData, mnStatus) == 8), "ARTIST thread status");
    CGS_STATIC_ASSERT((offsetof(EAThreadDynamicData, mnReturnValue) == 0xC), "ARTIST thread return");
    CGS_STATIC_ASSERT((offsetof(EAThreadDynamicData, mpStartContext) == 0x10), "ARTIST thread entry arguments");
    CGS_STATIC_ASSERT((offsetof(EAThreadDynamicData, mhThreadHandleCopy) == 0x18), "ARTIST copied thread handle");
    CGS_STATIC_ASSERT((offsetof(EAThreadDynamicData, mpBeginThreadUserWrapper) == 0x1C), "ARTIST thread wrapper");
    CGS_STATIC_ASSERT((offsetof(EAThreadDynamicData, mnRefCount) == 0x20), "ARTIST thread references");
    CGS_STATIC_ASSERT((offsetof(EAThreadDynamicData, mName) == 0x24), "ARTIST thread name");

'''+s[a:]
    return s


edit('src/SDKs/EATech/eathread/BrnEAThreadX360.h',facade_header)


def umbrella(s):
    s=s.replace('#include "types.hpp"\n#include "types.hpp"','#include "types.hpp"')
    s=s.replace('    // Time is a double count of milliseconds; the two sentinel deadlines.\n    typedef double ThreadTime;',
                '    // ARTIST uses a 32-bit millisecond word, passed by address to waits.\n    typedef u32 ThreadTime;')
    s=s.replace('const ThreadTime kTimeoutNone      = 4294967295.0;', 'const ThreadTime kTimeoutNone      = 0xFFFFFFFFu;')
    a=s.index('    // Sleep the calling thread for a relative time')
    b=s.index('    // NOTE: ThreadId GetThreadId()',a)
    s=s[:a]+'    // ThreadSleep(const u32*) and GetThreadTime() are owned by BrnEAThreadX360.h.\n\n'+s[b:]
    s=s.replace('// (ThreadTime/kTimeout*/kThreadId*/priority+processor constants) and the free\n// ThreadSleep(ThreadTime) overload, none of which BrnEAThreadX360.h defines.',
                '// (ThreadTime/kTimeout*/kThreadId*/priority+processor constants).')
    return s


edit('src/SDKs/EATech/eathread/eathread.h',umbrella)
edit('src/SDKs/EATech/eathread/eathread_thread.h',lambda s:s.replace('// eathread.h supplies the base scalar layer (ThreadTime, kTimeout*) and the free\n// ThreadSleep(ThreadTime) overload that belongs to this header\'s surface. The',
        '// eathread.h supplies the native base scalar layer (ThreadTime, kTimeout*). The'))
edit('src/SDKs/Realmc/RealmcCore.cpp',lambda s:s.replace('#include <intrin.h>  // _Interlocked* (MSVC) -- portable stand-in for the X360\n                     // lwarx/stwcx. reservation idiom.',
        '#include <xtl.h>   // native interrupt-masked _Interlocked* intrinsics'))


def queue(s):
    s=s.replace('EA::Thread::ThreadSleep(10);','{ const u32 luSleepMilliseconds = 10; EA::Thread::ThreadSleep(&luSleepMilliseconds); }')
    s=s.replace('mCondition.Wait(&mMutex, ltDeadline) == EA::Thread::Condition::kResultTimeout',
                'mCondition.Wait(&mMutex, &ltDeadline) == EA::Thread::Condition::kResultError')
    s=s.replace('// The console\'s Condition::Wait reports a timeout as -1 (its semaphore\'s timeout\n// code); the linked EAThread Condition reports it as kResultTimeout.',
                '// ARTIST shuts down only for Condition::Wait result -1 (error). A normal\n// timeout result -2 leaves the queue available for another wait.')
    return s


edit('src/SDKs/Realmc/RealmcMessageQueue.cpp',queue)
edit('src/GameShared/GameClasses/System/Threads/CgsThreadLayout.cpp',lambda s:s.replace('EA::Thread::ThreadSleep(lSleepMs);','EA::Thread::ThreadSleep(&lSleepMs);'))
edit('src/GameShared/GameClasses/Containers/CgsLocklessQueue.h',lambda s:s.replace('EA::Thread::ThreadSleep(0);',
    'const u32 luSleepMilliseconds = 0;\n            EA::Thread::ThreadSleep(&luSleepMilliseconds);').replace('EA::Thread::ThreadSleep(static_cast<EA::Thread::ThreadTime>(liSleepTime));',
    'const u32 luSleepMilliseconds = static_cast<u32>(liSleepTime);\n            EA::Thread::ThreadSleep(&luSleepMilliseconds);'))
edit('src/SDKs/EATech/rwcore/filesys/device.cpp',lambda s:s.replace('EA::Thread::ThreadSleep(1);',
    '{ const u32 luSleepMilliseconds = 1; EA::Thread::ThreadSleep(&luSleepMilliseconds); }'))
edit('src/SDKs/EATech/eajobs/detail.cpp',lambda s:s.replace('// Native EAThread takes its own ThreadTime value.\n            EA::Thread::ThreadSleep(static_cast<EA::Thread::ThreadTime>(lSleepMs));',
    '// Native EAThread receives the address of the 32-bit millisecond count.\n            const u32 luSleepMilliseconds = static_cast<u32>(lSleepMs);\n            EA::Thread::ThreadSleep(&luSleepMilliseconds);'))
print('Restored ten native thread/time call paths')
