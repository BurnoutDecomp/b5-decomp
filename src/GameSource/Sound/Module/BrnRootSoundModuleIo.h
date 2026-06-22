#ifndef BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_IO_H
#define BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_IO_H

#include <cstddef>   // offsetof (buffer layout asserts)
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (base; lock state machine)
#include "GameShared/GameClasses/Module/CgsEventQueue.h" // CgsModule::EventQueue<T,N> (AudioCarLoadedDataQueue)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // AudioCarDataLoadedEvent (queue element)

// =============================================================================
// BrnSound::Module::Io buffer accessors
//   GameSource/Sound/Module/BrnRootSoundModuleIo.h (DWARF home) +
//   GameSource/Sound/Module/BrnRootSoundModuleIO.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The BrnSound::Module::Io buffers
// (LogicInputBuffer, RootInputBuffer, ...) derive from CgsModule::IOBuffer: the
// status flag byte sits at offset 0, and the lock-guarded Get* accessors assert
// the right lock (read-lock bit 4 for const getters, write-lock bit 3 for the
// mutable getters) then return &member-at-X360-offset. Same shape as the
// committed BrnGameState::GameStateModuleIO buffers.
//
// This group lands exactly two of those accessors (the two TU functions):
//   * LogicInputBuffer::GetVehicleInterface() const  (X360 0x82694D30, truncated
//     to "BrnSound::Module::I" in the IDA export) -- read-locked, returns the
//     embedded vehicle (active-race-car output) interface at this+0x620. Proven
//     by VehicleStateManager::OnAssetLoaded: it calls this on GetBrnInputStructure()
//     (a LogicInputBuffer*) and feeds the result to
//     RCEntityActiveRaceCarOutputInterface::GetPlay(...).
//   * RootInputBuffer::GetPropUpdateNotificationQueue()  (X360 0x823B8518,
//     truncated to "BrnSound::Module::Io") -- write-locked, returns the
//     prop-update notification queue at this+0x10520. Proven by
//     BrnGameModule::BridgeWorldToSound: the result is passed straight to
//     PropUpdateNotification_::Append(...).
//
// MINIMAL SLICE: each buffer models only its touched member, pinned to its exact
// X360 byte offset with explicit u8 storage for the gap (the BrnGameStateModuleIO
// precedent). The member types are forward-declared incomplete classes; the
// touched member is named opaque storage of the correct width at the correct
// offset, so the buffer's later full reconstruction can replace the storage with
// the real typed member without moving anything. Offsets ARE asserted here: only
// the IOBuffer base + u8 storage precede each touched member (no host-width
// pointers), so offsetof is byte-faithful on the 64-bit gate.
// FLAG: the IDA-truncated method names (GetVehicleInterface / GetPropUpdate-
// NotificationQueue) are inferred from the call-site usage + the DWARF method list
// (BrnRootSoundModuleIo.h lines 108 / 226); the X360 bodies (lock assert + return
// &member-at-offset) are exact.
// =============================================================================

// The vehicle interface returned by LogicInputBuffer::GetVehicleInterface is the
// race-car active-output interface; only a pointer/reference is handed out, so an
// incomplete forward declaration suffices (its full layout lives in its own TU).
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

namespace BrnSound
{
namespace Module
{
namespace Io
{
    // The prop-update notification queue handed out (by pointer) from
    // RootInputBuffer::GetPropUpdateNotificationQueue. Forward-declared; the caller
    // (PropUpdateNotification_::Append) supplies the queue operations in its own TU.
    class PropUpdateNotificationQueue;

    // =========================================================================
    // BrnSound::Module::Io::PreUpdateOutput -- MINIMAL SLICE (canonical home is
    // GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h:149; reproduced here
    // only as far as RootPreUpdateOutputBuffer::SetPreUpdateOutput touches it).
    //
    // DWARF member order (BrnPreUpdateSharedIo.h:212-214):
    //   +0x000  GuiOutEventQueue (GuiEventQueueBase<256,16>)  mGuiOutEventQueue
    //   +0x110  EventQueue<AudioCarDataLoadedEvent,16>        mAudioCarDataLoadedQueue
    //   +0x2A0  AudioEffectsMessageQueue (VariableEventQueue<128,16>) mAudioEffectsMessageQueue
    //   sizeof == 0x330 (816)
    //
    // The X360 SetPreUpdateOutput (0x826E0C10) copies this struct in three steps,
    // which fixes the offsets:
    //   memcpy(dst+0x000, src+0x000, 0x110)   -> the GuiOut queue region
    //   dst.mAudioCarDataLoadedQueue.miLength = 0; Append(src.mAudioCarDataLoadedQueue)
    //   memcpy(dst+0x2A0, src+0x2A0, 0x090)   -> the AudioEffects queue region
    // The middle queue is copied via Clear()+Append (NOT memcpy) because it owns a
    // self-referential mpEvents pointer into its own inline buffer.
    //
    // The GuiOut and AudioEffects queues are NOT touched member-wise by this group,
    // so they are modelled as named opaque byte storage of their exact X360 width;
    // the real typed members land with the BrnPreUpdateSharedIo.h TU without moving
    // anything. The AudioCarDataLoadedQueue IS modelled as its real committed type
    // (EventQueue<AudioCarDataLoadedEvent,16>) so the Clear()+Append the X360 body
    // performs are reconstructed by name against the committed queue generics.
    // FLAG: minimal slice -- the omitted GuiOut/AudioEffects queue internals are
    // reconstructed by the BrnPreUpdateSharedIo.h TU; do not duplicate them here.
    // =========================================================================
    typedef CgsModule::EventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent, 16>
        AudioCarLoadedDataQueue;   // BrnWorldModuleIO.h:1505 (DWARF typedef)

    struct PreUpdateOutput
    {
        // GetCarDataLoadedQueue() const (BrnPreUpdateSharedIo.h:193); referenced by
        // SetPreUpdateOutput to merge the source's audio-car-loaded events.
        const AudioCarLoadedDataQueue& GetCarDataLoadedQueue() const { return mAudioCarDataLoadedQueue; }
        AudioCarLoadedDataQueue&       GetCarDataLoadedQueue()       { return mAudioCarDataLoadedQueue; }

        u8                      maGuiOutEventQueueStorage[0x110];     // @ +0x000 mGuiOutEventQueue
        AudioCarLoadedDataQueue mAudioCarDataLoadedQueue;            // @ +0x110 (0x190 wide)
        u8                      maAudioEffectsMessageQueueStorage[0x90]; // @ +0x2A0 mAudioEffectsMessageQueue

        static void _AssertLayout()
        {
            // X360 byte-faithful up to the audio-car-loaded queue: the leading GuiOut
            // region is pure POD storage (no host-wider members), so the queue's start
            // is asserted at its exact X360 offset (load-bearing for the memcpy split).
            static_assert(offsetof(PreUpdateOutput, mAudioCarDataLoadedQueue) == 0x110,
                          "PreUpdateOutput.mAudioCarDataLoadedQueue @ +0x110");
            // X360-32bit vs host-64bit divergence: AudioCarDataLoadedEvent embeds a
            // host-wider pointer (mpVehicleListEntry: 4B on X360, 8B on host) and the
            // queue's mpEvents pointer is likewise host-wider, so the queue is larger on
            // the 64-bit gate than the X360 0x190. The trailing AudioEffects region and
            // total size therefore CANNOT be asserted at the X360 offsets 0x2A0 / 0x330;
            // members are pinned BY NAME and SEQUENCE only past the queue. The three-step
            // SetPreUpdateOutput copy stays correct because it copies each region by its
            // named bounds (sizeof storage / Clear()+Append), never by absolute offset.
        }
    };

    // BrnSound::Module::Io::LogicInputBuffer -- the per-frame logic input payload the
    // sound logic module reads. Derives from CgsModule::IOBuffer.
    struct LogicInputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x82694D30 (read-lock; "Not locked for reading", BrnRootSoundModuleIo.h:405)
        // -- the embedded vehicle (active-race-car output) interface at this+0x620.
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
            GetVehicleInterface() const;

    private:
        u8 maPadToVehicleInterface[0x620 - sizeof(CgsModule::IOBuffer)]; // base end -> +0x0620
        // RCEntityActiveRaceCarOutputInterface @ +0x620; named opaque storage (full
        // layout lives in its own TU). GetVehicleInterface returns &this member.
        u8 mVehicleInterfaceStorage[0x40];                               // @ +0x0620

        static void _AssertLayout()
        {
            static_assert(offsetof(LogicInputBuffer, mVehicleInterfaceStorage) == 0x620,
                          "RCEntityActiveRaceCarOutputInterface @ +0x620");
        }
    };

    // BrnSound::Module::Io::RootInputBuffer -- the root sound module input payload
    // the BridgeWorldToSound bridge fills. Derives from CgsModule::IOBuffer.
    struct RootInputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x823B8518 (write-lock; "Not locked for writing", BrnRootSoundModuleIo.h:226)
        // -- the prop-update notification queue at this+0x10520. Non-const (the queue
        // is appended to under a write lock).
        PropUpdateNotificationQueue* GetPropUpdateNotificationQueue();

    private:
        u8 maPadToPropUpdateQueue[0x10520 - sizeof(CgsModule::IOBuffer)]; // base end -> +0x10520
        // PropUpdateNotificationQueue @ +0x10520; named opaque storage (full layout
        // lives in its own TU). GetPropUpdateNotificationQueue returns &this member.
        u8 mPropUpdateNotificationQueueStorage[0x40];                    // @ +0x10520

        static void _AssertLayout()
        {
            static_assert(offsetof(RootInputBuffer, mPropUpdateNotificationQueueStorage) == 0x10520,
                          "PropUpdateNotificationQueue @ +0x10520");
        }
    };

    // BrnSound::Module::Io::RootPreUpdateOutputBuffer (DWARF BrnRootSoundModuleIo.h:322)
    // -- the root sound module's pre-update output payload. Derives from CgsModule::
    // IOBuffer and holds a single PreUpdateOutput. The X360 places mPreUpdateOutput at
    // this+0x08: the 1-byte IOBuffer status base pads up to the PreUpdateOutput's
    // 8-byte alignment (its leading queue member is 8-aligned). Confirmed by the two
    // accessor bodies returning/operating at this+0x08:
    //   * GetPreUpdateOutput() const  (X360 0x823B8BB8, read-lock "Not locked for
    //     reading", BrnRootSoundModuleIo.h:338) -- returns &mPreUpdateOutput.
    //   * SetPreUpdateOutput(const PreUpdateOutput&)  (X360 0x826E0C10, write-lock
    //     "Not locked for writing", BrnRootSoundModuleIo.h:346) -- copies the source
    //     PreUpdateOutput into mPreUpdateOutput (POD spans by memcpy, the
    //     audio-car-loaded queue by Clear()+Append).
    struct RootPreUpdateOutputBuffer : public CgsModule::IOBuffer
    {
        // BrnRootSoundModuleIo.h:331 (DWARF; own TU, declared-only here).
        void Construct();

        // BrnRootSoundModuleIo.h:338 (DWARF). X360 0x823B8BB8.
        const PreUpdateOutput& GetPreUpdateOutput() const;

        // BrnRootSoundModuleIo.h:346 (DWARF). X360 0x826E0C10.
        void SetPreUpdateOutput(const PreUpdateOutput& lPreUpdateOutput);

    private:
        PreUpdateOutput mPreUpdateOutput;   // @ +0x08 (X360)

        static void _AssertLayout()
        {
            static_assert(offsetof(RootPreUpdateOutputBuffer, mPreUpdateOutput) == 0x08,
                          "RootPreUpdateOutputBuffer.mPreUpdateOutput @ +0x08");
        }
    };

} // namespace Io
} // namespace Module
} // namespace BrnSound

#endif // BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_IO_H
