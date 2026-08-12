// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO_InputBuffer_PreScene.cpp
//
// Out-of-line body for the one BrnWorld::PropEntityIO::InputBuffer_PreScene mutator the
// X360 ARTIST build emitted out-of-line. The rest of that buffer's producer surface
// (SetIsOnline / SetEasySmashProps / SetPropProgressionEnabled / SendingPropProgression /
// SetHitPropsBitArray / SetCurrentTimestep / ResetProps) is HEADER-INLINED by the console
// -- WorldModule::BridgeInputToEntityModules @0x827ADF88 has no `bl` for any of them, only
// direct stb/stw/stfs stores -- so those live as header inlines in BrnPropEntityModuleIO.h.
//
// Accessor shape (X360, authoritative, the same across every IO buffer in the project):
//   write-lock (status>>3 &1) => IsBufferLockedForWriting(), "Not locked for writing".
// CGS_ASSERT stamps __FILE__/__LINE__, so the X360-baked d:\p4 path/line is not reproduced.
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{
namespace PropEntityIO
{

// X360 0x827A1578 (W) -- latch the frame's replay status into the prop pre-scene input
// buffer. Sole caller: WorldModule::BridgeInputToEntityModules @0x827ADF88, whose asm
// reads `BrnWorldIO::UpdateInputBuffer::GetReplayStatusInter(worldInput)` a SECOND time
// (bl @0x827AE04C) and feeds it straight here (bl @0x827AE058) -- the line right after the
// identical hand-off into RaceCarEntityModuleIO::InputBuffer_PreScene::SetReplayStatusInterface.
//
// [EVIDENCE NOTE / PARK] 0x827A1578 is a HOLE in the .ida-exports dump: the bridge's xref
// table gives its address and its fully-qualified name, but no per-function json was
// emitted for it, and the PS3 DecFIGS DWARF for this buffer has neither this method nor a
// replay-status member (an X360-only merge-window addition). So the body below is NOT
// transcribed from its own disassembly. What IS pinned:
//   * the signature (from the X360 xref name + the call site's argument, a
//     const BrnReplays::ReplayIO::StatusInterface* straight off the world input buffer);
//   * the lock kind (every Set* on every IO buffer in the image asserts the WRITE lock);
//   * the effect (a snapshot copy of the source interface into the member) -- taken from
//     the two bodied siblings that take the very same pointer:
//       BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene::SetReplayStatusInterface
//         @0x8279D258  -> StatusInterface::operator= into a BY-VALUE member
//       BrnWorldIO::UpdateInputBuffer::SetReplayStatusInterface @0x823B4DF0 -> same.
// The member's console byte offset inside this buffer is unrecovered and is NOT claimed
// (see the member's [FLAG] note in the header); the copy is by name.
void
InputBuffer_PreScene::SetReplayStatusInterface(const BrnReplays::ReplayIO::StatusInterface* lpReplayStatusInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mReplayStatusInterface = *lpReplayStatusInterface;
}

// ============================================================================
// ADDITIVE GROW 2026-08-12 (prop-spawn wave, agent B5) -- the four remaining
// out-of-line InputBuffer_PreScene bodies the X360 ARTIST build emitted.
// ============================================================================

// X360 0x822EFAA0 -- InputBuffer_PreScene::Construct (BrnPropEntityModuleIO.h:344).
// 61 instructions, store-for-store:
//     *(u8*)this = 1                          -> IOBuffer base: mark constructed
//     Construct the three queues              -> this+0x4C (loaded, 25), this+0x04
//                                                (instances-needed, 30), this+0x8C
//                                                (unloaded, 25); each Construct is
//                                                followed by `stw 0, queue+8` (miLength)
//     mpabHitPropBitArray = 0                 -> stw r30, 0x780
//     mbSendingPropProgression = false        -> stb r30, 0x790
//     mbReloadingProfile       = false        -> stb r30, 0x791
//     mbResetProps             = false        -> stb r30, 0x792
//     me*Status = 2 (E_CHANGESTATUS_NO_CHANGE)-> `li r11,2` + stb 0x78E/0x78D/0x78F
//     mbPlayerCrashing = false                -> stb r30, 0x793
//     mPlayerPos / maRaceCarVelocity[0..7]    -> nine `stvx128 v0(zero)` at 0x6F0 and
//                                                0x700..0x770
// and then the unrecovered-payload initialisation (a word at +0xCC, six bytes at
// +0x1D0 + i*0x101, and {-1,-1,<float>} at +0x6D8/+0x6DC/+0x6E0) which this slice does
// NOT reproduce -- those members are the opaque span (see the header's FLAG), so there
// is nothing to write them BY NAME and inventing offset pokes into the span is exactly
// the fabrication the project forbids. Recorded here so the gap is explicit.
// (mbPlayerWrecked is NOT written by the console's Construct either -- also faithful.)
void
InputBuffer_PreScene::Construct()
{
    CgsModule::IOBuffer::Construct();

    mPropGraphicsLoadedQueue.Construct();
    mPropInstancesNeededForZoneQueue.Construct();
    mPropGraphicsUnloadedQueue.Construct();

    mpabHitPropBitArray      = 0;
    mbSendingPropProgression = false;
    mbReloadingProfile       = false;
    mbResetProps             = false;

    meEasySmashPropsStatus   = E_CHANGESTATUS_NO_CHANGE;
    meOnlineStatus           = E_CHANGESTATUS_NO_CHANGE;
    mePropProgressionStatus  = E_CHANGESTATUS_NO_CHANGE;

    mbPlayerCrashing         = false;

    mPlayerPos.SetZero();
    for (s32 liCar = 0; liCar < 8; ++liCar)
    {
        maRaceCarVelocity[liCar].SetZero();
    }
}

// X360 0x822B90A8 (`sub_822B90A8`, unnamed in IDA): read-lock handle, returns this + 0x4C.
// Its baked tripwire is "Not locked for reading\n" at BrnPropEntityModuleIO.h:452, which
// is DecFIGS :449 GetPropGraphicsLoadedQueue() const (the X360 file is 3 lines longer).
// Sole caller: PropEntityModule::PreSceneUpdate's graphics-LOADED queue drain.
const InputBuffer_PreScene::PropGraphicsLoadedQueue*
InputBuffer_PreScene::GetPropGraphicsLoadedQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mPropGraphicsLoadedQueue;
}

// :450 -- the non-const twin (no separate X360 symbol; the producer bridge inlines it).
InputBuffer_PreScene::PropGraphicsLoadedQueue*
InputBuffer_PreScene::GetPropGraphicsLoadedQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mPropGraphicsLoadedQueue;
}

// X360 0x822B9150 (`sub_822B9150`): read-lock handle, returns this + 0x8C; baked
// tripwire line 455 == DecFIGS :452 GetPropGraphicsUnloadedQueue() const.
const InputBuffer_PreScene::PropGraphicsUnloadedQueue*
InputBuffer_PreScene::GetPropGraphicsUnloadedQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mPropGraphicsUnloadedQueue;
}

// :453 -- the non-const twin.
InputBuffer_PreScene::PropGraphicsUnloadedQueue*
InputBuffer_PreScene::GetPropGraphicsUnloadedQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mPropGraphicsUnloadedQueue;
}

// X360 0x822B91F8 (`sub_822B91F8`): read-lock handle, returns this + 4; baked tripwire
// line 458 == DecFIGS :455 GetPropInstancesNeededForZoneQueue() const. This is the queue
// PropEntityModule::UpdateInstanceStreaming walks to decide which zones to stream.
const InputBuffer_PreScene::PropInstancesNeededForZoneQueue*
InputBuffer_PreScene::GetPropInstancesNeededForZoneQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mPropInstancesNeededForZoneQueue;
}

// :456 -- the non-const twin.
InputBuffer_PreScene::PropInstancesNeededForZoneQueue*
InputBuffer_PreScene::GetPropInstancesNeededForZoneQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mPropInstancesNeededForZoneQueue;
}

// ----------------------------------------------------------------------------
// Never called. Pins the SHAPES this buffer's consumer relies on. Deliberately NOT a
// set of console byte offsets: the three queues carry an 8-byte mpEvents on the host,
// so every member past +0x004 moves, and that is correct -- the console offsets in the
// header are provenance for WHICH member is which, nothing more.
// ----------------------------------------------------------------------------
void
InputBuffer_PreScene::_AssertLayout()
{
    // The 2-byte notification events + the capacities the X360 Construct passes.
    static_assert(sizeof(PropInstancesNeededForZoneEvent) == 2, "2-byte zone-id event");
    static_assert(sizeof(PropGraphicsLoadedEvent) == 2,         "2-byte graphics-id event");
    static_assert(sizeof(PropGraphicsUnloadedEvent) == 2,       "2-byte graphics-id event");
    static_assert(PropInstancesNeededForZoneQueue::KI_LENGTH == 30, "0x4C-0x04 == 12 + 30*2");
    static_assert(PropGraphicsLoadedQueue::KI_LENGTH == 25,         "0x8C-0x4C == 12 + 25*2 (round 4)");
    static_assert(PropGraphicsUnloadedQueue::KI_LENGTH == 25,       "sibling of the loaded queue");

    // The nine `stvx128` slots Construct zeroes: one position + eight velocities, 16B each.
    static_assert(sizeof(Vector3) == 16, "one SIMD lane per vector");
    static_assert(sizeof(maRaceCarVelocity) == 8 * 16,
                  "maRaceCarVelocity[8] -- the 0x700..0x770 stvx128 run");

    // 300000 bits == 37504 bytes == the memcpy size PreSceneUpdate uses for the profile's
    // hit-props copy. If this ever stops holding, that copy is copying the wrong thing.
    static_assert(sizeof(HitPropsBitArray) == 37504, "BitArray<300000> == the 37504-byte memcpy");
}

}
}
