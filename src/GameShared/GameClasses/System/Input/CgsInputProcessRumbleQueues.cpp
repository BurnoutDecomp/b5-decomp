// ============================================================================
// CgsInputProcessRumbleQueues.cpp
//
// The three CgsInput::InputIO leaf functions the input module's rumble pass
// (CgsInput::InputModule::ProcessRumbleRequests) calls. Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//
//   BaseEventQueue<PlayJoltEffectEvent>::GetEvent(s32) const   @ 0x828DFEB0
//   BaseEventQueue<StopRumbleEffectEvent>::GetEvent(s32) const @ 0x828DFF58
//   PreWorldInputBuffer::GetPlayRumbleEffectEventQueue() const @ 0x828E67E8
//
// The two GetEvent bodies are the per-instantiation out-of-line const element
// accessors of CgsModule::BaseEventQueue<T> (generic body inline in
// CgsBaseEventQueue.h): assert the buffer + the index bounds (CgsBaseEventQueue.h
// :272/:274/:275) then return &mpEvents[liIndex]. The X360 renders the return as
// `sizeof(T)*liIndex + *mpEvents`, i.e. stride 60 for PlayJoltEffectEvent and
// stride 12 for StopRumbleEffectEvent (the only 12-byte member of the rumble-event
// family: BaseInputEvent[8] + miRumbleId[4]). These const overloads are distinct
// X360 functions from the non-const GetEvent instances in the per-type
// EventQueue_*_4.cpp files (which only instantiate the non-const overload), so the
// const instantiations are emitted here, this TU's own home, without disturbing
// those sibling files.
//
// The third is the PreWorldInputBuffer read-lock accessor for the play-rumble
// queue at this+256 (the queue immediately after the jolt queue) -- same shape as
// the buffer's other read-lock accessors (assert IsBufferLockedForReading, return
// the typed queue member).
// ============================================================================

#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// ---- per-instantiation const element accessors (bodies inline in CgsBaseEventQueue.h) ----

// @ 0x828DFEB0 - BaseEventQueue<PlayJoltEffectEvent>::GetEvent(s32) const (stride 60).
//   asm tail: mulli r11,r29,0x3C; add r3,r11,*mpEvents  (== &mpEvents[liIndex]).
template const CgsInput::InputIO::PlayJoltEffectEvent&
    CgsModule::BaseEventQueue<CgsInput::InputIO::PlayJoltEffectEvent>::GetEvent(s32) const;

// @ 0x828DFF58 - BaseEventQueue<StopRumbleEffectEvent>::GetEvent(s32) const (stride 12).
//   asm tail: slwi r11,r30,1; add r11,r30,r11; slwi r11,r11,2 (== 12*liIndex);
//   add r3,r11,*mpEvents  (== &mpEvents[liIndex]).
template const CgsInput::InputIO::StopRumbleEffectEvent&
    CgsModule::BaseEventQueue<CgsInput::InputIO::StopRumbleEffectEvent>::GetEvent(s32) const;

// ---- PreWorldInputBuffer read-lock accessor ----

namespace CgsInput
{
namespace InputIO
{

// @ 0x828E67E8 - read-lock accessor returning the play-rumble-effect event queue (this+256).
// asm: lbz r11,0(this); extrwi r11,r11,1,27 (bit 4 = read-lock); bne skip;
//      <Begin/Fire("Not locked for reading\n", CgsInputModuleIO.h, 927)/End>;
//      addi r3,this,0x100 (== this+256). Mirrors GetTimerStatusInt @0x828E69E0 (this+868,
//      line 969). Caller: CgsInput::InputModule::ProcessRumbleRequests.
const PreWorldInputBuffer::PlayRumbleEffectEventQueue*
PreWorldInputBuffer::GetPlayRumbleEffectEventQueue() const
{
    // Member scope grants access to pin the modelled PC offset of the queue immediately
    // after the jolt queue (matches the X360 this+256 placement; the timer-snapshot member
    // it precedes is held at the X360 +868 stride -- see the header note on pointer width).
    static_assert(offsetof(PreWorldInputBuffer, mTimerStatusInterface) == 868,
                  "mTimerStatusInterface kept at the X360 +868 stride");
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mPlayRumbleEffectEventQueue;
}

}
}
