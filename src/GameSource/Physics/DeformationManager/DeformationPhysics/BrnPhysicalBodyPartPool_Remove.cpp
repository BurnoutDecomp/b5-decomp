#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"  // InputBuffer (write-side GetRemoveRigidBodyQueue)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h" // InRemoveRigidBody

// BrnPhysics::Deformation::PhysicalBodyPartPool -- the slot-release slice.
// ⭐ TU CREATED 2026-08-14 (deformation-mount wave), the established slice pattern
// (BrnPhysicalBodyPartPool_{Construct,Accessors}.cpp are the siblings): the home TU still
// carries its own open closure (CreatePart/UpdateRWBodies/UpdateJoinedParts), and the
// deformation-manager mount needs exactly this one body (ResetDeformation ->
// RemovePhysicalPartsAndJoints -> HERE). Fold back into the home TU when it mounts.
//
// NOTE: this path is DEAD AT RUNTIME this wave (no part ever becomes physical until contact
// generation lands); it exists for LINK closure and for the day parts detach.

namespace BrnPhysics
{
namespace Deformation
{
    // =============================================================================================
    // RemovePart @ 0x8260CA30 (54 instr) -- release slot lu8Index back to the pool.
    //
    // Call-for-call (part == &maParts[lu8Index], console stride 0x1F0):
    //   * if !part->mbJoinedToVehicle (lbz +0x1E4): the part owns a live rigid body -- post an
    //     InRemoveRigidBody{ mID = the packed part handle (ld +0x1D0) } on the sim input buffer's
    //     remove queue (the inlined write-side GetRemoveRigidBodyQueue @0x825BCF58 + the
    //     BaseEventQueue<InRemoveRigidBody>::AddEvent instantiation, both in mounted TUs).
    //     ⚠️ The console's 16-byte stack record only ever writes the leading 8 bytes -- the
    //     mbFailIfRigidBodyNotFound byte at +8 is UNINITIALISED stack on the X360 (a real console
    //     quirk, not a transcription gap). The host sets it FALSE == the tolerant remove, which is
    //     what ProcessRemoveRigidBodyQueue's not-found-tolerant arm does with a clear byte.
    //   * if part->mbAddedToScene (lbz +0x1E5): PhysicalBodyPart::RemoveFromScene(lpSceneInput).
    //   * clear the used bit (the inlined CgsBitArray UnSetBit with its "luIndex < NUMBITS"
    //     bounds tripwire, CgsBitArray.h:241), decrement mu8NumDetachedParts (stb +0x60EC), and
    //     tear down the slot's bindings + re-seed its handle with K_INVALID_RIGID_BODY_ID
    //     (qword_82F2A3A8) -- the named ClearPoolSlotBindings on the part.
    // =============================================================================================
    void PhysicalBodyPartPool::RemovePart(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                          CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                                          u8 lu8Index)
    {
        PhysicalBodyPart& lrPart = maParts[lu8Index];

        if (!lrPart.IsJoinedToVehicle())
        {
            CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody lRemoveEvent;
            lRemoveEvent.mID = lrPart.GetContactVolumeInstanceId().muId;   // ld part+0x1D0, std -> record+0
            lRemoveEvent.mbFailIfRigidBodyNotFound = false;                // ⚠️ console leaves this byte as stack garbage; see banner
            lpSimInput->GetRemoveRigidBodyQueue()->AddEvent(lRemoveEvent);
        }

        if (lrPart.IsAddedToScene())
        {
            lrPart.RemoveFromScene(lpSceneInput);
        }

        // The inlined CgsBitArray bounds tripwire (CgsBitArray.h:241), then the un-set + count.
        CGS_ASSERT(static_cast<u32>(lu8Index) < KU_MAX_DETACHED_PARTS, "luIndex < NUMBITS");
        mUsedParts.UnSetBit(static_cast<u32>(lu8Index));
        mu8NumDetachedParts = static_cast<u8>(mu8NumDetachedParts - 1u);   // addi +0xFF / stb

        lrPart.ClearPoolSlotBindings();   // the four teardown stores (see the part-header method)
    }
}
}
