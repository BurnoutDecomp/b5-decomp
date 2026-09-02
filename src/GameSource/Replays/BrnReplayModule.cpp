#include "GameSource/Replays/BrnReplayModule.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"   // ReplayIO::RequestInterface
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"   // BaseSerialiser
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"   // AllocatorList
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // CgsMemory::LinearMalloc
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // WriteToLog
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstdio>   // snprintf

// ============================================================================
// GameSource/Replays/BrnReplayModule.cpp
//
// BrnReplays::ReplayModule -- the constructor + the two off-path module methods,
// reconstructed from the X360 ARTIST build.
//
//   * ReplayModule::ReplayModule        (X360 @0x827E03D0) [boot-trace: EXECUTED]
//   * ReplayModule::Update_Dispatch     (X360 @0x8265D1B0)
//   * ReplayModule::WaitForSerialiseJobs(X360 @0x8264B4A8)
//
// CONSTRUCTOR (store-for-store against the asm spine):
//   *this           = &off_820CE500     // base ModuleSingleBuffered vtable
//   RWMutex(this+0x10,  0, 1)           // base mInputBuffer.mMutex
//   RWMutex(this+0x118, 0, 1)           // base mOutputBuffer.mMutex
//   *this           = &off_820CFEF0     // derived ReplayModule vtable
//   *(this+0x23C)   = 0                 // mbFlag23C
//   Job::ctor(this+0x2B10, 0)
//   RtlInitializeCriticalSection(this+0x3B00)   // mLockA
//   RtlInitializeCriticalSection(this+0x3F80)   // mLockB
//   for i=3..0: Job::ctor(this+0x4940 + i*0x350, 0)
//   Job::ctor(this+0x5680, 0)
//   *(this+0x6264)  = &off_820CE890     // mpTailInterfaceVTable
//
// In human C++ the two vtable writes + the two RWMutex constructions are the
// ModuleSingleBuffered base sub-object's own construction, and the two
// RtlInitializeCriticalSection calls are the EA::Thread::Futex members' own ctors
// (the committed Futex is exactly that CRITICAL_SECTION-backed lock, reused by name,
// so its ctor performs the init -- same pattern as CgsGraphics::MoviePlayer). This
// body therefore reproduces only the trailing scalar stores past the base.
//
// FLAG -- DEFERRED sub-construction. The X360 ctor chains EA::Jobs::Job::Job(.,0) on
// each embedded job (the +0x2B10, the four +0x4940, and the +0x5680 serialise job).
// Those jobs have no complete reconstructed layout (asm-sized opaque placeholders in
// the header), so their sub-constructors are NOT chained here -- chaining them would
// require fabricating the embedded layout/ctor-overload, which the rules forbid. The
// GPUDiskWriteStream / DataStreamCommandPoster / Futex embeds construct trivially via
// their real types. Every scalar store the ctor makes IS reproduced by name.
// ============================================================================

// off_820CE890 is the trailing contained-interface vtable symbol; it is not
// reconstructed, so the ctor leaves mpTailInterfaceVTable null (FLAGGED above).

namespace BrnReplays
{
    ReplayModule::ReplayModule()
    {
        mbPrepared     = false;   // the module has not prepared (X360 base +0x228)
        mpLinearMalloc = 0;   // ReplayModule::Prepare acquires it (X360 +0x878)

        // Base (ModuleSingleBuffered: both vtables + the two RWMutexes) and the
        // embedded mGpuWriteStream / mLockA / mLockB / mCommandPoster construct
        // automatically before this body. mLockA / mLockB's Futex ctors perform the
        // two RtlInitializeCriticalSection calls the X360 ctor makes at +0x3B00/+0x3F80.

        mbFlag23C = false;            // X360 stb 0 @ +0x23C

        // Embedded jobs (@+0x2B10, the four @+0x4940, @+0x5680) sub-construction DEFERRED.

        mpTailInterfaceVTable = nullptr;  // X360 stamps &off_820CE890 here (FLAGGED).
    }

    // X360 0x8265D1B0. Tail-call the GPU disk write stream's Dispatch (the asm is a
    // branch into GPUDiskWriteStream::Dispatch with this advanced to +0x980).
    void ReplayModule::Update_Dispatch()
    {
        mGpuWriteStream.Dispatch();
    }

    // X360 0x8264B4A8. If a serialise job is in flight, wait for it to finish, end the
    // command poster, and clear the poster-active flag; then always clear the
    // serialise-active flag.
    void ReplayModule::WaitForSerialiseJobs()
    {
        if (mbSerialiseActive)
        {
            // X360: EA::Jobs::Job::WaitOn(mSerialiseJob). The serialise job is an
            // asm-sized opaque placeholder (no reconstructed layout), so the WaitOn
            // is DEFERRED here -- it folds in with the job-layout pass. FLAG.
            mCommandPoster.End();
            mbPosterActive = false;
        }
        mbSerialiseActive = false;
    }

}
