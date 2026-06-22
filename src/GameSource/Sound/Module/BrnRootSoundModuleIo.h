#ifndef BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_IO_H
#define BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_IO_H

#include <cstddef>   // offsetof (buffer layout asserts)
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (base; lock state machine)

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

} // namespace Io
} // namespace Module
} // namespace BrnSound

#endif // BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_IO_H
