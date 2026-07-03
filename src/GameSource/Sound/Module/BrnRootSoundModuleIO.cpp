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
    return &mPropUpdateNotificationQueue;
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
    //     (whole-object memcpy stays X360-faithful: the queue is pointer-free --
    //      inline buffer + s32 cursors -- so the byte copy relocates cleanly)
    std::memcpy(&mPreUpdateOutput.mAudioEffectsMessageQueue,
                &lPreUpdateOutput.mAudioEffectsMessageQueue,
                sizeof(mPreUpdateOutput.mAudioEffectsMessageQueue));
}

// =============================================================================
// BrnSound::Module::Io::RootInputBuffer accessors (wave 7).
// Read-lock const getters assert bit 4; write-lock setters/getters assert bit 3.
// Each getter returns &member-at-X360-offset; each setter memcpys the source
// interface over the embedded member (the X360 operator=/queue-Append semantics).
// =============================================================================

// X360 0x82694940 -- read-lock accessor for the game event queue @ +0x6494.
const RootInputBuffer::GameEventQueue* RootInputBuffer::GetGameEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mGameEventQueue;
}

// X360 0x823B83C8 -- write-lock accessor for the SAME game event queue @ +0x6494.
RootInputBuffer::GameEventQueue* RootInputBuffer::GetGameEventQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mGameEventQueue;
}

// X360 0x82695128 -- read-lock accessor for the gui-audio event results @ +0x13730.
const RootInputBuffer::GuiAudioEventResults* RootInputBuffer::GetGuiAudioEventResults() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mGuiAudioEventResults;
}

// X360 0x82695078 -- read-lock accessor for the gui event queue pointer @ +0xEBA8.
const RootInputBuffer::GuiEventQueue* RootInputBuffer::GetGuiEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mpGuiEventQueue;
}

// X360 0x82694F28 -- read-lock accessor for the player's active-race-car slot @ +0x2F10.
EActiveRaceCarIndex RootInputBuffer::GetPlayerActiveRaceCarIndex() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mePlayerActiveRaceCarIndex;
}

// X360 0x82694550 -- read-lock accessor for the per-frame update info @ +0xEBBC.
const RootInputBuffer::UpdateInfo* RootInputBuffer::GetUpdateInfo() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mUpdateInfo;
}

// X360 0x826947F0 (folded bare BrnSound::Module::Io::RootI) -- read-lock accessor for the
// online-scoring output interface @ +0xEA30.
const RootInputBuffer::OnlineScoringOutputInterface* RootInputBuffer::GetOnlineScoringInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mOnlineScoringInterface;
}

// X360 0x826945F8 (folded bare BrnSound::Module::Io::RootInputB) -- read-lock accessor for
// the game-mode output interface @ +0xEBAC.
const RootInputBuffer::GameModeOutputInterface* RootInputBuffer::GetGameModeInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mGameModeInterface;
}

// X360 0x82694748 (folded bare BrnSound::Module::Io::RootInputBu) -- read-lock accessor for
// the scoring output interface @ +0xDF80.
const RootInputBuffer::ScoringOutputInterface* RootInputBuffer::GetScoringInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mScoringInterface;
}

// X360 0x82694E80 (folded bare BrnSound::Module::Io::RootInputBuffer) -- read-lock accessor
// for the deformation output interface @ +0xB490 (returns by reference, DWARF :129).
const RootInputBuffer::DeformationInterface& RootInputBuffer::GetDeformationInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mDeformationInterface;
}

// X360 0x82694C88 (folded bare BrnSound::Module::Io::RootInp) -- read-lock accessor for the
// game-action queue @ +0x3084 (returns by reference, DWARF :90). Const overload.
const RootInputBuffer::GameActionQueue& RootInputBuffer::GetGameActionQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mGameActionQueue;
}

// X360 0x823B8668 (folded bare BrnSound::Module::Io::RootInputBuff) -- write-lock accessor
// for the SAME game-action queue @ +0x3084 (returns by reference, DWARF :87). Non-const.
RootInputBuffer::GameActionQueue& RootInputBuffer::GetGameActionQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return mGameActionQueue;
}

// --- write-lock setters --------------------------------------------------------

// X360 0x823B7EC0 @ +0x0004.
void RootInputBuffer::SetReplayStatusInterface(const ReplayStatusInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mReplayStatusInterface, lpInterface, sizeof(ReplayStatusInterface));
}

// X360 0x823C9140 @ +0x2F20.
void RootInputBuffer::SetCameraInput(const DirectorCamera* lpCamera)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mDirectorCamera, lpCamera, sizeof(DirectorCamera));
}

// X360 0x823B8908 @ +0x3080.
void RootInputBuffer::SetContactSpyQueueInterface(const InputContactSpyQueueInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mContactSpyQueueInterface, lpInterface, sizeof(InputContactSpyQueueInterface));
}

// X360 0x823B8710 @ +0x6AB0.
void RootInputBuffer::SetTrafficOutputInterface(const TrafficSoundOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mTrafficOutputInterface, lpInterface, sizeof(TrafficSoundOutputInterface));
}

// X360 0x823C9088 @ +0x74C0 (reset-count-then-Append modelled as a wholesale memcpy over
// pointer-free inline queue storage; stride not attested, start offset exact).
void RootInputBuffer::SetPhysicalTrafficStates(const PhysicalTrafficStateQueue* lpStates)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mPhysicalTrafficStates, lpStates, sizeof(PhysicalTrafficStateQueue));
}

// X360 0x823C91F0 @ +0xB490.
void RootInputBuffer::SetDeformationInterface(const DeformationInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mDeformationInterface, lpInterface, sizeof(DeformationInterface));
}

// X360 0x823B81A0 @ +0xDF80.
void RootInputBuffer::SetScoringInterface(const ScoringOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mScoringInterface, lpInterface, sizeof(ScoringOutputInterface));
}

// X360 0x823B8258 @ +0xEA30.
void RootInputBuffer::SetOnlineScoringInterface(const OnlineScoringOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mOnlineScoringInterface, lpInterface, sizeof(OnlineScoringOutputInterface));
}

// X360 0x823C92A8 @ +0xEAD4 (reset-count-then-Append modelled as memcpy over inline queue).
void RootInputBuffer::SetWorldLoadInterface(const SoundWorldLoadInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mWorldLoadInterface, lpInterface, sizeof(SoundWorldLoadInterface));
}

// X360 0x823B89B8 @ +0xEBA8 -- stores the caller's queue pointer (not a copy).
void RootInputBuffer::SetGuiEventQueue(const GuiEventQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mpGuiEventQueue = lpQueue;
}

// X360 0x823B8028 @ +0xEBAC.
void RootInputBuffer::SetGameModeInterface(const GameModeOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mGameModeInterface, lpInterface, sizeof(GameModeOutputInterface));
}

// X360 0x823B7F70 @ +0xEBBC.
void RootInputBuffer::SetUpdateInfo(const UpdateInfo* lpInfo)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mUpdateInfo, lpInfo, sizeof(UpdateInfo));
}

// X360 0x823B8310 @ +0xEEE0.
void RootInputBuffer::SetAICarOutputInterface(const AICarOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mAICarOutputInterface, lpInterface, sizeof(AICarOutputInterface));
}

// =============================================================================
// BrnSound::Module::Io::RootOutputBuffer accessors (wave 7).
// Read-lock const getters assert bit 4; write-lock non-const getters assert bit 3.
// Each returns &member-at-X360-offset (embedded request-interface storage).
// =============================================================================

// X360 0x823B8A68 -- read-lock accessor for the sound resource request interface @ +0x04.
const RootOutputBuffer::SoundResourceRequestInterface*
RootOutputBuffer::GetResourceRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const SoundResourceRequestInterface*>(&mResourceRequestInterfaceStorage);
}

// X360 0x826951D0 -- write-lock accessor for the SAME sound resource request interface @ +0x04.
RootOutputBuffer::SoundResourceRequestInterface*
RootOutputBuffer::GetResourceRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<SoundResourceRequestInterface*>(&mResourceRequestInterfaceStorage);
}

// X360 0x823B8B10 (folded bare BrnSound::Module::Io::RootOutp) -- read-lock accessor for the
// AttribSys request interface @ +0x1014.
const RootOutputBuffer::AttribSysRequestInterface* RootOutputBuffer::GetAttribSysRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const AttribSysRequestInterface*>(&mAttribSysRequestInterfaceStorage);
}

// X360 0x82695278 (folded bare BrnSound::Module::Io::RootOutputBuff) -- write-lock accessor
// for the SAME AttribSys request interface @ +0x1014.
RootOutputBuffer::AttribSysRequestInterface* RootOutputBuffer::GetAttribSysRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<AttribSysRequestInterface*>(&mAttribSysRequestInterfaceStorage);
}

// X360 0x823B85C0 -- read-lock accessor for the replay request interface @ +0x1824.
const RootOutputBuffer::ReplayRequestInterface*
RootOutputBuffer::GetReplayRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return reinterpret_cast<const ReplayRequestInterface*>(&mReplayRequestInterfaceStorage);
}

// X360 0x82694A90 -- write-lock accessor for the SAME replay request interface @ +0x1824
// (non-const overload; DWARF false-negative on non-const getters).
RootOutputBuffer::ReplayRequestInterface*
RootOutputBuffer::GetReplayRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return reinterpret_cast<ReplayRequestInterface*>(&mReplayRequestInterfaceStorage);
}

} // namespace Io
} // namespace Module
} // namespace BrnSound
