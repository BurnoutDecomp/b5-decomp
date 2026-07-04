#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_InputBuffer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // std::memcpy

// BrnEffects::EffectsIO::InputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The per-frame input aggregate the game bridges fill before the
// effects module updates. OPAQUE-IMAGE idiom (committed BrnAIModuleIO_OutputBuffer /
// BrnCrashModuleIO precedent): each accessor reinterprets its member at the X360-attested
// byte offset over the buffer image. Every accessor shares the recurring CgsModule::IOBuffer
// lock-guard prologue: getters test bit 4 (eStatusLockedForRead, "Not locked for reading\n"),
// setters test bit 3 (eStatusLockedForWrite, "Not locked for writing\n"). (These Effects lock
// strings carry the trailing \n per the X360 rodata.)

namespace BrnEffects
{
namespace EffectsIO
{

// ---- InputBufferBaseEventQueueView::Append -------------------------------------------------
// FLAG (foreign home): the concrete queue element types (BrnPhysics::Vehicle::
// PhysicalTrafficState / BrnWorld::PropEntityIO::PropVFXLocatorEvent) and the packed-byte
// merge are owned by the queue's own ledger TU (CgsModule::VariableEventQueue<>::Append).
// Only the destination-offset + clear-count facts are attested by the two effects setters, so
// this view models the "merge the source event count onto our tail" tail of that Append; the
// byte-image copy of the packed records is deferred to the real queue type.
void InputBufferBaseEventQueueView::Append(const InputBufferBaseEventQueueView& lrSource)
{
    miLength += lrSource.miLength;
}

// ============================== getters (read-lock, status bit 4) ==============================

// X360 0x8227DBE0 (R, :138) -- read-lock handle to the game-action queue (this+0xA690).
// [EffectsIO-bare accessor folded into the opaque-image idiom.]
const InputBuffer::GameActionQueue* InputBuffer::GetGameActionQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const GameActionQueue*>(MemberImage() + KU_GAME_ACTION_QUEUE_OFFSET);
}

// X360 0x8227DD30 (R, :152) -- read-lock handle to the triangle-cache interface (this+0xE400).
// [EffectsIO-bare accessor folded into the opaque-image idiom.]
const InputBuffer::InTriangleCacheInterface* InputBuffer::GetTriangleCacheInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const InTriangleCacheInterface*>(MemberImage() + KU_TRIANGLE_CACHE_INTERFACE_OFFSET);
}

// X360 0x8227D940 (R, :119) -- read-lock; return &mCameraInput (this+0x7A00).
const void* InputBuffer::GetCameraInput() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return MemberImage() + KU_CAMERA_INPUT_OFFSET;
}

// X360 0x8227D9E8 (R, :123) -- read-lock; return &mTimerStatusInterface (this+0x7B60).
const void* InputBuffer::GetTimerStatusInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return MemberImage() + KU_TIMER_STATUS_INTERFACE_OFFSET;
}

// X360 0x8227DA90 (R, :127) -- read-lock; return &mContactSpyInterface (this+0x7B90).
const void* InputBuffer::GetContactSpyInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return MemberImage() + KU_CONTACT_SPY_INTERFACE_OFFSET;
}

// X360 0x8227DB38 (R, :131) -- read-lock; return &mDeformationInterface (this+0x7BA0).
const void* InputBuffer::G() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return MemberImage() + KU_DEFORMATION_INTERFACE_OFFSET;
}

// X360 0x8227DDD8 (R, :156) -- read-lock; return &mAudioEffectsMessageQueue (this+0xE404).
const void* InputBuffer::GetAud() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return MemberImage() + KU_AUDIO_EFFECTS_MESSAGE_QUEUE_OFFSET;
}

// X360 0x8227D7F0 (R, :111) -- read-lock; return &mActiveRaceCarInterface (this+0x1140).
// Const read-lock counterpart of SetActiveRaceCarInterface (0x823BA490 :109).
const void* InputBuffer::GetActiveRaceCarInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return MemberImage() + KU_ACTIVE_RACE_CAR_INTERFACE_OFFSET;
}

// X360 0x8227D898 (R, :115) -- read-lock; return &mVehiclePhysicalStateQueue (this+0x3A30).
// Const read-lock counterpart of SetVehiclePhysicalStateQueue (0x823C96B8 :113).
const void* InputBuffer::GetVehiclePhysicalStateQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return MemberImage() + KU_VEHICLE_PHYSICAL_STATE_QUEUE_OFFSET;
}

// X360 0x8227DC88 (R, :149) -- read-lock; return &mPropVFXLocatorQueue (this+0xE0D0).
// The asm forms the offset as addis+1/-0x1F30 (== +0xE0D0). Const read-lock counterpart of
// SetPropVFXLocatorQueue (0x823C98D0 :147).
const void* InputBuffer::GetPropVFXLocatorQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return MemberImage() + KU_PROP_VFX_LOCATOR_QUEUE_OFFSET;
}

// ============================== getters (write-lock, status bit 3) =============================
// The non-const overload faithfully tests the WRITE bit as the asm names it.

// X360 0x823BA708 (W, :137) -- write-lock handle to the game-action queue (this+0xA690).
// [EffectsIO-bare accessor folded into the opaque-image idiom.]
InputBuffer::GameActionQueue* InputBuffer::GetGameActionQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<GameActionQueue*>(MemberImage() + KU_GAME_ACTION_QUEUE_OFFSET);
}

// ============================== setters (write-lock, status bit 3) =============================

// X360 0x823BA490 (:109) -- assert write-lock, XMemCpy 10480 bytes into
// mActiveRaceCarInterface (this+0x1140).
void InputBuffer::SetActiveRaceCarInterface(const void* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(MemberImage() + KU_ACTIVE_RACE_CAR_INTERFACE_OFFSET, lpInterface,
                KU_ACTIVE_RACE_CAR_INTERFACE_SIZE);
}

// X360 0x823C96B8 (:113) -- assert write-lock, clear mVehiclePhysicalStateQueue's live count
// (miLength @ queue+8), then Append(source) onto its tail (queue @ this+0x3A30). The queue
// element type (BrnPhysics::Vehicle::PhysicalTrafficState) is owned by another TU, so the
// clear+append is modelled on the queue's BaseEventQueue view.
void InputBuffer::SetVehiclePhysicalStateQueue(const void* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

    InputBufferBaseEventQueueView& lDest = *reinterpret_cast<InputBufferBaseEventQueueView*>(
        MemberImage() + KU_VEHICLE_PHYSICAL_STATE_QUEUE_OFFSET);
    const InputBufferBaseEventQueueView& lSource =
        *static_cast<const InputBufferBaseEventQueueView*>(lpQueue);

    lDest.miLength = 0;   // X360: stw 0, 8(queue) -- reset live count before the merge
    lDest.Append(lSource);
}

// X360 0x823C9770 (:117) -- assert write-lock, then mCameraInput = *lpCameraInput
// (this+0x7A00). The X360 build calls BrnDirector::Camera::Camera::operator= out-of-line;
// modelled here as a byte-image copy of the CameraInput sub-object at its attested offset.
void InputBuffer::SetCameraInput(const void* lpCameraInput)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(MemberImage() + KU_CAMERA_INPUT_OFFSET, lpCameraInput,
                KU_CAMERA_INPUT_SIZE);
}

// X360 0x823BA548 (:121) -- assert write-lock, then mTimerStatusInterface = *lpTimer
// (this+0x7B60). The X360 build expands the TimerStatusInterface operator= as two 24-byte
// field runs (word/f32/f32/byte/word/f32); modelled here as a 48-byte struct-image copy.
void InputBuffer::SetTimerStatusInterface(const void* lpTimer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(MemberImage() + KU_TIMER_STATUS_INTERFACE_OFFSET, lpTimer,
                KU_TIMER_STATUS_INTERFACE_SIZE);
}

// X360 0x823BA658 (:125) -- assert write-lock, then copy one word (*lpInterface) into
// mContactSpyInterface (this+0x7B90). The interface holds a single pointer/word.
void InputBuffer::SetContactSpyInterface(const void* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(MemberImage() + KU_CONTACT_SPY_INTERFACE_OFFSET, lpInterface,
                sizeof(u32));
}

// X360 0x823C9820 (:129) -- assert write-lock, then mDeformationInterface = *lpInterface
// (this+0x7BA0). The X360 build calls DeformationOutputInterface::operator= out-of-line;
// modelled here as a byte-image copy of the sub-object at its attested offset.
void InputBuffer::SetDeformationInterface(const void* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(MemberImage() + KU_DEFORMATION_INTERFACE_OFFSET, lpInterface,
                KU_DEFORMATION_INTERFACE_SIZE);
}

// X360 0x823BA7B0 (:141) -- assert write-lock, then mReplayStatusInterface = *lpStatus
// (this+0xDAB0). The X360 build calls BrnReplays::ReplayIO::StatusInterface::operator=
// out-of-line; modelled here as a byte-image copy at the attested offset.
void InputBuffer::SetReplayStatusInterface(const void* lpStatus)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(MemberImage() + KU_REPLAY_STATUS_INTERFACE_OFFSET, lpStatus,
                KU_REPLAY_STATUS_INTERFACE_SIZE);
}

// X360 0x823BA868 (:143) -- assert write-lock, then mEffectsEnvironmentInterface =
// *lpEnv (this+0xDAA0), a 16-byte copy (two 8-byte runs). The interface is a single
// 16-byte-aligned Vector2 (BrnEffects::EffectsEnvironmentInterface, DWARF-attested 16 bytes).
void InputBuffer::SetEffectsEnvironmentInterface(const void* lpEnv)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(MemberImage() + KU_EFFECTS_ENVIRONMENT_INTERFACE_OFFSET, lpEnv,
                KU_EFFECTS_ENVIRONMENT_INTERFACE_SIZE);
}

// X360 0x823C98D0 (:147) -- assert write-lock, clear mPropVFXLocatorQueue's live count
// (miLength @ queue+8), then Append(source) onto its tail (queue @ this+0xE0D0). The queue
// element type (BrnWorld::PropEntityIO::PropVFXLocatorEvent) is owned by another TU, so the
// clear+append is modelled on the queue's BaseEventQueue view.
void InputBuffer::SetPropVFXLocatorQueue(const void* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

    InputBufferBaseEventQueueView& lDest = *reinterpret_cast<InputBufferBaseEventQueueView*>(
        MemberImage() + KU_PROP_VFX_LOCATOR_QUEUE_OFFSET);
    const InputBufferBaseEventQueueView& lSource =
        *static_cast<const InputBufferBaseEventQueueView*>(lpQueue);

    lDest.miLength = 0;   // X360: stw 0, 8(queue) -- reset live count before the merge
    lDest.Append(lSource);
}

// X360 0x823BA928 (:151) -- assert write-lock, then copy one word (*lpInterface) into
// mTriangleCacheInterface (this+0xE400). The interface holds a single pointer/word.
void InputBuffer::SetTriangleCacheInterface(const void* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(MemberImage() + KU_TRIANGLE_CACHE_INTERFACE_OFFSET, lpInterface,
                sizeof(u32));
}

// X360 0x823BA9E0 (:154) -- assert write-lock, memcpy 144 bytes into
// mAudioEffectsMessageQueue (this+0xE404).
void InputBuffer::SetAudioEffectsMessageQueue(const void* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(MemberImage() + KU_AUDIO_EFFECTS_MESSAGE_QUEUE_OFFSET, lpQueue,
                KU_AUDIO_EFFECTS_MESSAGE_QUEUE_SIZE);
}

}   // namespace EffectsIO
}   // namespace BrnEffects
