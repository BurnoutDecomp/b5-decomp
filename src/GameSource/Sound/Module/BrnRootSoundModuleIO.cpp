#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (lock-state guards)

#include <cstring>   // std::memcpy (PreUpdateOutput POD-span copies; models the Xbox block copy)

// =============================================================================
// BrnSound::Module::Io buffer accessors (this group's two TU functions).
//
// Each body asserts the buffer's lock state (read-lock bit 4 for the const getter,
// write-lock bit 3 for the mutable getter) then returns &member-at-X360-offset.
// The assert message strings ("Not locked for reading\n" / "Not locked for
// writing\n") match the X360 build's baked strings; see BrnRootSoundModuleIo.h.
// =============================================================================

namespace BrnSound
{
namespace Module
{
namespace Io
{

// X360 0x82694D30 (IDA-truncated "BrnSound::Module::I"). Read-lock accessor for the
// embedded vehicle (active-race-car output) interface at this+0x620.
const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
LogicInputBuffer::GetVehicleInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*>(
        &mVehicleInterfaceStorage);
}

// X360 0x823B8518 (IDA-truncated "BrnSound::Module::Io"). Write-lock accessor for the
// prop-update notification queue at this+0x10520 (the caller appends to it).
PropUpdateNotificationQueue* RootInputBuffer::GetPropUpdateNotificationQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<PropUpdateNotificationQueue*>(&mPropUpdateNotificationQueueStorage);
}

// X360 0x823B8BB8 ("BrnSound::Module::Io::RootPreUpdateOutputBuffer::Ge[tPreUpdateOutput]",
// IDA-truncated). Read-lock accessor: assert read-locked ("Not locked for reading\n",
// BrnRootSoundModuleIo.h:590 in the X360 baked string -> the DWARF method is :338), then
// return &mPreUpdateOutput (this+0x08).
const PreUpdateOutput& RootPreUpdateOutputBuffer::GetPreUpdateOutput() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mPreUpdateOutput;
}

// X360 0x826E0C10. Write-lock setter: assert write-locked ("Not locked for writing\n"),
// then copy the source PreUpdateOutput into mPreUpdateOutput exactly as the X360 body:
//   1. memcpy the leading GuiOut-queue region (0x110 bytes, dst+0x08 <- src+0x00),
//   2. reset the audio-car-loaded queue's live count then Append all of the source's
//      events (the queue owns a self-referential mpEvents pointer, so it is merged by
//      Clear()+Append rather than block-copied),
//   3. memcpy the trailing AudioEffects-queue region (0x90 bytes, dst+0x2A8 <- src+0x2A0).
void RootPreUpdateOutputBuffer::SetPreUpdateOutput(const PreUpdateOutput& lPreUpdateOutput)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

    // (1) leading GuiOut queue region: PreUpdateOutput +0x000 .. +0x110.
    std::memcpy(mPreUpdateOutput.maGuiOutEventQueueStorage,
                lPreUpdateOutput.maGuiOutEventQueueStorage,
                sizeof(mPreUpdateOutput.maGuiOutEventQueueStorage));

    // (2) audio-car-loaded queue: zero the destination count (X360 `stw 0, 8(queue)`),
    //     then merge all live source events.
    mPreUpdateOutput.mAudioCarDataLoadedQueue.Clear();
    mPreUpdateOutput.mAudioCarDataLoadedQueue.Append(lPreUpdateOutput.mAudioCarDataLoadedQueue);

    // (3) trailing AudioEffects queue region: PreUpdateOutput +0x2A0 .. +0x330.
    std::memcpy(mPreUpdateOutput.maAudioEffectsMessageQueueStorage,
                lPreUpdateOutput.maAudioEffectsMessageQueueStorage,
                sizeof(mPreUpdateOutput.maAudioEffectsMessageQueueStorage));
}

} // namespace Io
} // namespace Module
} // namespace BrnSound
