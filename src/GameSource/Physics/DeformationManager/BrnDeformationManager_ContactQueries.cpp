// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager_ContactQueries.cpp
//
// ⭐ MOUNTED SLICE (walls leg 1, 2026-08-14): the three per-car collision-QUERY methods
// VehicleManager::DoRaceCarWorldContactGeneration @0x825EB140 calls per live race car per
// frame. Sliced out of the still-unmounted BrnDeformationManager_Contacts.cpp exactly as the
// _ContactFixups slice was (2026-08-06 precedent, "fold back when it mounts"):
//   IsUsingSweptSpheres   @0x825C2338  MOVED (body verbatim from _Contacts.cpp)
//   GetSweptSpheresForCar @0x825C22D0  MOVED (body verbatim from _Contacts.cpp)
//   GetSpheresForCar      @0x825C2260  NEW — an EXPORT-SET HOLE (no JSON among the 30,084;
//     the name sits in the caller's own xrefs_from at exactly this address). All 28
//     instructions were lifted from the image with the proven ppcdis/x360rd pair.
//     ⚠️ IT IS NOT THE SWEPT SIBLING'S SHAPE: on a missing model it returns -1 GRACEFULLY
//     (blt -> li r3, -1) with NO assert and WITHOUT writing the out pointer — which is why
//     the caller zero-initialises its out slot and asserts "lpSpheres" itself (:952).
//     The body inlines DeformableObject::GetWorldSpaceSpheres (DWARF :434, now a header
//     inline): *out = &maWorldSensorSpheres[0] (model+0), count = spec sensors + 4.
// ============================================================================

#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject

namespace BrnPhysics
{
namespace Deformation
{
    // ==========================================================================================
    // GetSpheresForCar @ 0x825C2260 (export hole, lifted 2026-08-14)
    //
    // The (non-swept) world-space collision spheres for the car with entity id lEntityId.
    //   0x825C227C  bl FindModelIndexByEntityID
    //   0x825C2284  blt -> return -1              (graceful; no assert, out NOT written)
    //   0x825C2290  mulli 26496 + mpaModels       (&mpaModels[index])
    //   0x825C229C  stw model, 0(out)             (*out = &maWorldSensorSpheres[0], model+0)
    //   0x825C22A0  lwz spec, 6368(model) ; lbz 1618(spec) ; addi +4   (GetNumSensors)
    // ==========================================================================================
    s32 DeformationManager::GetSpheresForCar(EntityId lEntityId,
                                             const CgsGeometric::Sphere** lppSpheresOut)
    {
        const s32 liIndex = FindModelIndexByEntityID(lEntityId);
        if (liIndex < 0)
        {
            return -1;
        }
        return mpaModels[liIndex].GetWorldSpaceSpheres(lppSpheresOut);
    }

    // ==========================================================================================
    // GetSweptSpheresForCar @ 0x825C22D0  (MOVED from _Contacts.cpp, body verbatim)
    //
    // The continuous (swept) collision spheres for the car with entity id lEntityId. Looks up the
    // model slot, asserts it is live, and forwards to DeformableObject::GetSweptSpheres.
    // ==========================================================================================
    s32 DeformationManager::GetSweptSpheresForCar(EntityId lEntityId,
                                                  const CgsGeometric::SweptSphere** lppSpheresOut)
    {
        const s32 liIndex = FindModelIndexByEntityID(lEntityId);
        CGS_ASSERT(liIndex != -1, "liIndex != -1");
        return mpaModels[liIndex].GetSweptSpheres(lppSpheresOut);
    }

    // ==========================================================================================
    // IsUsingSweptSpheres @ 0x825C2338  (MOVED from _Contacts.cpp, body verbatim)
    //
    // Whether the car with entity id lEntityId currently runs continuous (swept-sphere) collision.
    // ==========================================================================================
    bool DeformationManager::IsUsingSweptSpheres(EntityId lEntityId)
    {
        const s32 liIndex = FindModelIndexByEntityID(lEntityId);
        CGS_ASSERT(liIndex != -1, "liIndex != -1");
        return mpaModels[liIndex].IsUsingSweptSpheres();
    }
}
}
