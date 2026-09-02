#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_InputBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstring>                                    // std::memcpy (the console's XMemCpy / memcpy setters)

// ============================================================================
// BrnEffects::EffectsIO::InputBuffer -- lifecycle + accessors, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Every accessor shares the recurring CgsModule::IOBuffer
// lock-guard prologue: read the status byte, test one lock bit -- const getters test
// bit 4 (eStatusLockedForRead, "Not locked for reading\n"), the write-side getter
// and every setter test bit 3 (eStatusLockedForWrite, "Not locked for writing\n") --
// then touch one named member. The Effects lock strings carry the trailing \n per
// the X360 rodata.
//
// 2026-09-02 (tyre-mark wave): the members are REAL TYPES now (see the header), so
// each setter is the type's own copy: operator= where the console calls one
// (Camera / DeformationOutputInterface / ReplayIO::StatusInterface), a bitwise copy
// where the console XMemCpy's / memcpy's (the active-race-car interface, the timer
// status, the audio message queue), one-word stores for the two pointer-sized
// interfaces, and Clear + Append for the two fixed-capacity event queues.
// ============================================================================

namespace BrnEffects
{
namespace EffectsIO
{

// ---- Construct @ 0x82293618 ----------------------------------------------------------
// The console order, store for store:
//   *this = 1 (IOBuffer::Construct)                 mInEventQueue.Construct()     [+292]
//   mContactSpyInterface.Construct()      [+31632]  mDeformationInterface.Construct() [+31648]
//   mGameActionQueue.Construct()          [+42640]
//   mReplayStatusInterface: mxStatusFlags = 0 [+55984]; the six reels' macName[0] = 0
//     (the 257-byte stride loop from +56244); mfDebugHudAlpha = 0.0 [+57540];
//     miCurrentRecordReel = miCurrentPlaybackReel = -1 [+57532 / +57536]
//     (the inlined StatusInterface clear -- there is no out-of-line body for it)
//   mPropVFXLocatorQueue.Construct()      [+57552]  mVehiclePhysicalStateQueue.Construct() [+14896]
//   mActiveRaceCarInterface.Clear()       [+4416]   mCameraInput.Clear()           [+31232]
//   mTimerStatusInterface.Clear()         [+31584]  mbSuspendEffects = 0           [+58516]
// maBoostInfos / mEffectsEnvironmentInterface / mTriangleCacheInterface /
// mAudioEffectsMessageQueue are NOT initialised by the console's Construct (the bridge
// writes every one of them before Update reads them); reproduced.
void InputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();
    mInEventQueue.Construct();
    mContactSpyInterface.Construct();
    mDeformationInterface.Construct();
    mGameActionQueue.Construct();

    mReplayStatusInterface.mxStatusFlags = 0;
    for (s32 liReel = 0; liReel < 6; ++liReel)
    {
        mReplayStatusInterface.maReels[liReel].macName[0] = '\0';
    }
    mReplayStatusInterface.mfDebugHudAlpha        = 0.0f;
    mReplayStatusInterface.miCurrentRecordReel    = -1;
    mReplayStatusInterface.miCurrentPlaybackReel  = -1;

    mPropVFXLocatorQueue.Construct();
    mVehiclePhysicalStateQueue.Construct();
    mActiveRaceCarInterface.Clear();
    mCameraInput.Clear();
    mTimerStatusInterface.Clear();
    mbSuspendEffects = false;
}

// ============================== read-lock getters ==============================

const InputBuffer::GameActionQueue* InputBuffer::GetGameActionQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mGameActionQueue;
}

const InputBuffer::InTriangleCacheInterface* InputBuffer::GetTriangleCacheInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mTriangleCacheInterface;
}

const BrnDirector::Camera::Camera* InputBuffer::GetCameraInput() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mCameraInput;
}

const CgsSystem::TimerStatusInterface* InputBuffer::GetTimerStatusInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mTimerStatusInterface;
}

const BrnPhysics::ContactSpy::ContactSpyInterface* InputBuffer::GetContactSpyInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mContactSpyInterface;
}

const BrnPhysics::Deformation::DeformationOutputInterface* InputBuffer::GetDeformationInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mDeformationInterface;
}

const InputBuffer::AudioEffectsMessageQueue* InputBuffer::GetAudioEffectsMessageQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mAudioEffectsMessageQueue;
}

const InputBuffer::RCEntityActiveRaceCarOutputInterface* InputBuffer::GetActiveRaceCarInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mActiveRaceCarInterface;
}

const InputBuffer::PhysicalTrafficStateQueue* InputBuffer::GetVehiclePhysicalStateQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mVehiclePhysicalStateQueue;
}

const InputBuffer::PropVFXLocatorQueue* InputBuffer::GetPropVFXLocatorQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mPropVFXLocatorQueue;
}

const InputBuffer::ReplayStatusInterface* InputBuffer::GetReplayStatusInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mReplayStatusInterface;
}

const EffectsEnvironmentInterface* InputBuffer::GetEffectsEnvironmentInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mEffectsEnvironmentInterface;
}

// ============================== write-lock getter ==============================

// X360 0x823BA708 (:142) -- the game-action queue for the bridge's Append.
InputBuffer::GameActionQueue* InputBuffer::GetGameActionQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mGameActionQueue;
}

// ============================== setters (write-lock) ==============================

// X360 0x823BA490 (:82) -- XMemCpy(&mActiveRaceCarInterface, lpInterface, 0x28F0): the
// console copies the interface bitwise; the host copy is the same object bitwise.
void InputBuffer::SetActiveRaceCarInterface(const RCEntityActiveRaceCarOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mActiveRaceCarInterface, lpInterface, sizeof(mActiveRaceCarInterface));
}

// X360 0x823C96B8 (:91) -- `stw 0, 8(queue)` (Clear) then BaseEventQueue::Append(source).
void InputBuffer::SetVehiclePhysicalStateQueue(const PhysicalTrafficStateQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mVehiclePhysicalStateQueue.Clear();
    mVehiclePhysicalStateQueue.Append(*lpQueue);
}

// X360 0x823C9770 (:100) -- mCameraInput = *lpCameraInput (Camera::operator=).
void InputBuffer::SetCameraInput(const BrnDirector::Camera::Camera* lpCameraInput)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mCameraInput = *lpCameraInput;
}

// X360 0x823BA548 (:109) -- the 48-byte (two 24-byte TimerStatus) copy.
void InputBuffer::SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpTimer)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mTimerStatusInterface, lpTimer, sizeof(mTimerStatusInterface));
}

// X360 0x823BA658 (:118) -- one word: the interface IS its ContactSpyData pointer.
void InputBuffer::SetContactSpyInterface(const BrnPhysics::ContactSpy::ContactSpyInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mContactSpyInterface = *lpInterface;
}

// X360 0x823C9820 (:127) -- mDeformationInterface = *lpInterface (its operator=).
void InputBuffer::SetDeformationInterface(const BrnPhysics::Deformation::DeformationOutputInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mDeformationInterface = *lpInterface;
}

// DWARF :136 -- one BoostOutputInfo slot (the bridge copies all eight inline instead).
void InputBuffer::SetBoostInfoN(s32 liIndex, const BoostOutputInfo* lpBoostInfo)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    CGS_ASSERT(liIndex >= 0 && liIndex < static_cast<s32>(KU_NUM_BOOST_INFOS), "liIndex < KU_NUM_BOOST_INFOS");
    maBoostInfos[liIndex] = *lpBoostInfo;
}

// X360 0x823BA7B0 (:151) -- mReplayStatusInterface = *lpStatus (its operator=).
void InputBuffer::SetReplayStatusInterface(const ReplayStatusInterface* lpStatus)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mReplayStatusInterface = *lpStatus;
}

// X360 0x823BA868 (:154) -- the 16-byte (one Vector2) copy.
void InputBuffer::SetEffectsEnvironmentInterface(const EffectsEnvironmentInterface* lpEnv)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mEffectsEnvironmentInterface = *lpEnv;
}

// X360 0x823C98D0 (:163) -- `stw 0, 8(queue)` (Clear) then BaseEventQueue::Append(source).
void InputBuffer::SetPropVFXLocatorQueue(const PropVFXLocatorQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mPropVFXLocatorQueue.Clear();
    mPropVFXLocatorQueue.Append(*lpQueue);
}

// X360 0x823BA928 (:172) -- one word: the interface IS its TriangleCacheManager pointer.
void InputBuffer::SetTriangleCacheInterface(const InTriangleCacheInterface* lpInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mTriangleCacheInterface = *lpInterface;
}

// X360 0x823BA9E0 (:178) -- memcpy(&mAudioEffectsMessageQueue, lpQueue, 0x90): a bitwise
// copy of the whole VariableEventQueue<128,16> (its buffer is inline -- no pointer to alias).
void InputBuffer::SetAudioEffectsMessageQueue(const AudioEffectsMessageQueue* lpQueue)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    std::memcpy(&mAudioEffectsMessageQueue, lpQueue, sizeof(mAudioEffectsMessageQueue));
}

}   // namespace EffectsIO
}   // namespace BrnEffects
