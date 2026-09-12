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
// BrnReplays::ReplayModule -- the constructor + the two off-path module methods.
//
//   * ReplayModule::ReplayModule         (runs on every boot)
//   * ReplayModule::Update_Dispatch
//   * ReplayModule::WaitForSerialiseJobs
//
// CONSTRUCTOR, store for store as the console performs it:
//   the ModuleSingleBuffered base vtable, then the derived ReplayModule vtable
//   RWMutex construction on the base input buffer's mutex   (+0x10)
//   RWMutex construction on the base output buffer's mutex  (+0x118)
//   mbFlag23C             = 0                               (+0x23C)
//   Job construction on the embedded jobs at +0x2B10, at +0x4940 (four of them,
//     stride +0x350) and at +0x5680
//   critical-section init on mLockA (+0x3B00) and on mLockB (+0x3F80)
//   mpTailInterfaceVTable = the trailing contained-interface vtable (+0x6264)
//
// In human C++ the two vtable writes + the two RWMutex constructions are the
// ModuleSingleBuffered base sub-object's own construction, and the two
// critical-section inits are the EA::Thread::Futex members' own ctors (the committed
// Futex is exactly that CRITICAL_SECTION-backed lock, reused by name, so its ctor
// performs the init -- same pattern as CgsGraphics::MoviePlayer). This body therefore
// reproduces only the trailing scalar stores past the base.
//
// FLAG -- DEFERRED sub-construction. The console ctor chains EA::Jobs::Job::Job(.,0) on
// each embedded job (+0x2B10, the four at +0x4940, and the +0x5680 serialise job).
// Those jobs have no complete reconstructed layout (size-only opaque placeholders in
// the header), so their sub-constructors are NOT chained here -- chaining them would
// require fabricating the embedded layout/ctor-overload, which the rules forbid. The
// GPUDiskWriteStream / DataStreamCommandPoster / Futex embeds construct trivially via
// their real types. Every scalar store the ctor makes IS reproduced by name.
// ============================================================================

// The trailing contained-interface vtable is not reconstructed, so the ctor leaves
// mpTailInterfaceVTable null (FLAGGED above).

namespace BrnReplays
{
    ReplayModule::ReplayModule()
    {
        mbPrepared     = false;   // the module has not prepared (base +0x228)
        mpLinearMalloc = 0;   // ReplayModule::Prepare acquires it (+0x878)

        // Base (ModuleSingleBuffered: both vtables + the two RWMutexes) and the
        // embedded mGpuWriteStream / mLockA / mLockB / mCommandPoster construct
        // automatically before this body. mLockA / mLockB's Futex ctors perform the
        // two critical-section inits the console ctor makes at +0x3B00 / +0x3F80.

        mbFlag23C = false;            // the console stores 0 at +0x23C

        // Embedded jobs (+0x2B10, the four at +0x4940, +0x5680) sub-construction DEFERRED.

        mpTailInterfaceVTable = nullptr;  // the console stamps the vtable here (FLAGGED).
    }

    // Tail-call the GPU disk write stream's Dispatch: the console branches straight into
    // GPUDiskWriteStream::Dispatch with this advanced to the embedded stream at +0x980.
    void ReplayModule::Update_Dispatch()
    {
        mGpuWriteStream.Dispatch();
    }

    // If a serialise job is in flight, wait for it to finish, end the
    // command poster, and clear the poster-active flag; then always clear the
    // serialise-active flag.
    void ReplayModule::WaitForSerialiseJobs()
    {
        if (mbSerialiseActive)
        {
            // The console waits on mSerialiseJob here. The serialise job is a
            // size-only opaque placeholder (no reconstructed layout), so the WaitOn
            // is DEFERRED here -- it folds in with the job-layout pass. FLAG.
            mCommandPoster.End();
            mbPosterActive = false;
        }
        mbSerialiseActive = false;
    }

}
