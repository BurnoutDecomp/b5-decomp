#include "GameShared/GameClasses/System/Resource/CgsLiveUpdateModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::LiveUpdateIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the buffer's lifecycle + the four lock-guarded
// accessors that view the +4 request queue and the +0x214 in-flight status byte:
//
//   Construct()             @ 0x828EEF98  status=constructed + queue Construct + status byte = 0
//   GetOutputQueue() const  @ 0x826642D8  read-lock  (bit 4) -> &mOutputQueue (this + 4)
//   Get()                   @ 0x828E64A0  write-lock (bit 3) -> &mOutputQueue (this + 4)
//   GetSt()  const          @ 0x82664230  read-lock  (bit 4) -> &mStatus     (this + 0x214)
//   SetStatus(const u8*)    @ 0x828E63F0  write-lock (bit 3) -> mStatus = *lpStatus
//
// Each accessor loads the 1-byte FlagSet status (lbz 0(this)) and tests its lock bit (read:
// extrwi ...,27 == (status>>4)&1; write: extrwi ...,28 == (status>>3)&1), asserts when clear,
// then returns/writes the named member. Assert strings are verbatim X360 rodata (the "reading"/
// "writing" strings carry a trailing \n).

namespace CgsResource
{
namespace LiveUpdateIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mOutputQueue) == 0x0004, "mOutputQueue @0x0004");
    }

    // X360 0x828EEF98: set the IOBuffer constructed bit (this+0), Construct the +4 request
    // queue (VariableEventQueue<512,16>, 528 bytes), then zero the +0x214 status byte.
    void OutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();   // stb 1,0(this): status = eStatusConstructed
        mOutputQueue.Construct();           // bl VariableEventQueue<512,16>::Construct(this+4)
        mStatus = 0;                        // stb 0,0x214(this)
    }

    // X360 0x826642D8: read-lock (bit 4); return this + 4 (the request queue), const.
    const OutputBuffer::OutputQueue* OutputBuffer::GetOutputQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mOutputQueue;
    }

    // X360 0x828E64A0: write-lock (bit 3); return this + 4 (the request queue). This is the
    // write-locked handle LiveUpdateModule::Update obtains to AddEvent its LoadBundleRequest.
    OutputBuffer::OutputQueue* OutputBuffer::Get()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mOutputQueue;
    }

    // X360 0x82664230: read-lock (bit 4); return this + 0x214 (the 1-byte status). Read-locked
    // const accessor to the live-update in-progress status byte. DWARF CgsLiveUpdateModuleIO.h:108.
    const u8* OutputBuffer::GetSt() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mStatus;
    }

    // X360 0x828E63F0: write-lock (bit 3); copy *lpStatus into the +0x214 status byte. Called by
    // LiveUpdateModule::Update at end-of-frame with the address of (miUpdateState != 0) to publish
    // whether a live-update is still in flight. DWARF CgsLiveUpdateModuleIO.h:109.
    void OutputBuffer::SetStatus(const u8* lpStatus)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mStatus = *lpStatus;   // lbz r11,0(a2) ; stb r11,0x214(this)
    }
}
}
