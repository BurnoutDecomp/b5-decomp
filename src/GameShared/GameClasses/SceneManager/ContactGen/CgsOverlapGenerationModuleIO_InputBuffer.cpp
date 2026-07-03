// ===========================================================================
// CgsSceneManager::OverlapGenerationIO::InputBuffer -- producing (write) side.
//
// The SceneManagerModule pushes per-frame body-mutation requests onto this input
// buffer's event queues; the OverlapGenerationModule's Process*BodyQueue passes
// (CgsOverlapGenerationModule.cpp) then replay them into the SceneSweeper. This TU
// hosts the one producer body the X360 emits out-of-line (UpdateBody); the queue
// AddEvent/Construct instantiations live in their own sibling .cpp files.
//
// The InputBuffer / event layouts are the committed stand-in in
// CgsOverlapGenerationModule.h (grown to host this wave); this TU does NOT introduce a
// competing CgsOverlapGenerationModuleIO.h (that would duplicate OverlapGenerationIO::
// InputBuffer -- an ODR clash with the module header the Process passes compile against).
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModule.h"

#include <cstring>  // std::memcpy (models the Xbox block-copy of the 32-byte world AABB)

namespace CgsSceneManager
{
namespace OverlapGenerationIO
{

// ---------------------------------------------------------------------------------
// InputBuffer::UpdateBody @ 0x828BA430
//   Push a per-frame body-position update request onto the update-body queue. The X360
//   assembles the 64-byte InUpdateBodyEvent record on the stack -- the 32-byte world AABB
//   value-copied from lpAabb (four ld/std qword pairs off r5), the position lane
//   (stvx128 v1 @+0x20), the sweeper object index (stw r4 @+0x30) and the packed
//   volume-instance handle (std r6 @+0x38) -- then appends it via the update-body queue's
//   AddEvent. There is NO IOBuffer lock assert in this body (the asm never tests the
//   status byte's read/write bit).
// ---------------------------------------------------------------------------------
void InputBuffer::UpdateBody(u32 luIndex, const void* lpAabb, Vector3 lvPadding, VolumeInstanceId lVolumeInstanceID)
{
    InUpdateBodyEvent lEvent;

    // element+0x00: the world AABB, block-copied whole from the caller's box
    // (X360: four ld/std qword pairs off r5 -> the two 16-byte AABBox corner rows).
    std::memcpy(lEvent.maAABBox, lpAabb, sizeof(lEvent.maAABBox));

    // element+0x20: position lane (stvx128 v1); +0x30: object index (stw r4);
    // +0x38: packed volume-instance handle / body word (std r6).
    lEvent.mvPosition    = lvPadding;
    lEvent.muObjectIndex = luIndex;
    lEvent.mu64Body      = lVolumeInstanceID.muId;

    // sub_828B0230(this) resolves the update-body queue (== &mUpdateBodyQueue); AddEvent
    // appends the record.
    GetUpdateBodyQueue()->AddEvent(lEvent);
}

} // namespace OverlapGenerationIO
} // namespace CgsSceneManager
