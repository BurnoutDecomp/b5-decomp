#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (lock-state guards)

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

} // namespace Io
} // namespace Module
} // namespace BrnSound
