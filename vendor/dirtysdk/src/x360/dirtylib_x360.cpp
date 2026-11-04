// DirtySDK 5.5.3 X360 dirtylib platform core, recovered from ARTIST.
#include "dirtylib.h"
#include "dirtylib_x360_internal.h"

namespace {
struct NetIdleEntry { void (*proc)(void*); void* ref; };
NetCritT gGlobalCrit;
NetCritT gIdleCrit;
NetIdleEntry gIdle[32];
s32 gIdleCount;
volatile LONG gThreadActive = -1;
HANDLE volatile gThreadRun;
LARGE_INTEGER gCounterTicksPerMs;
u32 (*gTickProc)();

u32 CounterTick()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<u32>(counter.QuadPart / gCounterTicksPerMs.QuadPart);
}

u32 SystemTick() { return GetTickCount(); }
}

extern "C" void NetCritInit(NetCritT* crit, const char*)
{
    if (!crit) crit = &gGlobalCrit;
    crit->owner = 0;
    crit->count = 0;
    crit->lock = 0;
    InitializeCriticalSection(&crit->section);
}

// ARTIST82B29A90: the native kill operation only releases the spin flag.
extern "C" void NetCritKill(NetCritT* crit)
{
    if (!crit) crit = &gGlobalCrit;
    crit->lock = 0;
}

extern "C" s32 NetCritTry(NetCritT* crit)
{
    if (!crit) crit = &gGlobalCrit;
    if (crit->owner != GetCurrentThreadId())
    {
        if (InterlockedExchange(&crit->lock, 1)) return 0;
        EnterCriticalSection(&crit->section);
        crit->owner = GetCurrentThreadId();
    }
    ++crit->count;
    return 1;
}

extern "C" void NetCritEnter(NetCritT* crit)
{
    NetCritT* actual = crit ? crit : &gGlobalCrit;
    if (!NetCritTry(crit))
    {
        for (;;)
        {
            EnterCriticalSection(&actual->section);
            if (!InterlockedExchange(&actual->lock, 1)) break;
            LeaveCriticalSection(&actual->section);
            Sleep(1);
            if (NetCritTry(crit)) return;
        }
        actual->owner = GetCurrentThreadId();
        ++actual->count;
    }
}

extern "C" void NetCritLeave(NetCritT* crit)
{
    if (!crit) crit = &gGlobalCrit;
    if (crit->count <= 1)
    {
        crit->owner = 0;
        crit->count = 0;
        crit->lock = 0;
        LeaveCriticalSection(&crit->section);
    }
    else --crit->count;
}

extern "C" void NetIdleReset(void) { gIdleCount = 0; }

extern "C" void NetIdleAdd(void (*proc)(void*), void* ref)
{
    if (proc)
    {
        const s32 index = gIdleCount;
        gIdle[index].proc = proc;
        gIdleCount = index + 1;
        gIdle[index].ref = ref;
    }
    else NetPrintfCode("dirtylib: attempt to add an invalid idle function\n");
}

extern "C" void NetIdleDel(void (*proc)(void*), void* ref)
{
    if (!proc)
    {
        NetPrintfCode("dirtylib: attempt to delete an invalid idle function\n");
        return;
    }
    for (s32 i = 0; i < gIdleCount; ++i)
        if (gIdle[i].proc == proc && gIdle[i].ref == ref)
        {
            gIdle[i].proc = 0;
            gIdle[i].ref = 0;
            return;
        }
}

extern "C" void NetIdleDone(void)
{
    NetCritEnter(&gIdleCrit);
    NetCritLeave(&gIdleCrit);
}

extern "C" void NetIdleCall(void)
{
    if (!NetCritTry(&gIdleCrit)) return;
    for (s32 i = 0; i < gIdleCount; ++i)
    {
        if (gIdle[i].proc) gIdle[i].proc(gIdle[i].ref);
        else
        {
            --gIdleCount;
            gIdle[i] = gIdle[gIdleCount];
            gIdle[gIdleCount].proc = 0;
            gIdle[gIdleCount].ref = 0;
            --i;
        }
    }
    NetCritLeave(&gIdleCrit);
}

namespace {
DWORD WINAPI IdleThread(void*)
{
    gThreadActive = 1;
    while (gThreadRun)
    {
        NetIdleCall();
        Sleep(50);
    }
    gThreadActive = 0;
    return 0;
}
}

extern "C" u32 NetTick(void) { return gTickProc(); }

extern "C" void NetLibCreate(s32 priority)
{
    NetIdleReset();
    gGlobalCrit.owner = 0;
    gGlobalCrit.count = 0;
    gThreadActive = -1;
    gGlobalCrit.lock = 0;
    InitializeCriticalSection(&gGlobalCrit.section);
    gIdleCrit.owner = 0;
    gIdleCrit.count = 0;
    gIdleCrit.lock = 0;
    InitializeCriticalSection(&gIdleCrit.section);
    gThreadRun = reinterpret_cast<HANDLE>(1);
    DWORD threadId;
    gThreadRun = CreateThread(0, 0, IdleThread, 0, 0, &threadId);
    if (gThreadRun)
    {
        SetThreadPriority(gThreadRun, priority);
        CloseHandle(gThreadRun);
    }
    if (QueryPerformanceFrequency(&gCounterTicksPerMs))
    {
        gTickProc = CounterTick;
        gCounterTicksPerMs.QuadPart /= 1000;
    }
    else gTickProc = SystemTick;
}

extern "C" void NetLibDestroy(void)
{
    gThreadRun = 0;
    while (gThreadActive > 0) Sleep(1);
    gGlobalCrit.lock = 0;
    gIdleCrit.lock = 0;
}
