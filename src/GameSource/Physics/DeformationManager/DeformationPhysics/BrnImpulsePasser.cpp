#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnImpulsePasser.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnPhysics::Deformation::ImpulsePasser -- out-of-line bodies. ⭐ TU CREATED 2026-08-14
// (deformation-mount wave): the header has always said "bodies are owned by the BrnImpulsePasser
// TU", but no such TU existed -- every method was declare-only. The first in-tree caller
// (DeformableObject::ResetSensors @0x82623D60) landed this wave and needs SetCollidableBodyMap.
//
// SetCollidableBodyMap has NO out-of-line X360 export: the console INLINES it at the ResetSensors
// call site, where its body is fully attested (0x82623EB8..0x82623F00):
//   cmpwi liIndex, 0        ; blt -> assert
//   cmpwi liIndex, 0x19     ; bge -> assert   (25 == KI_MAX_COLLIDABLE_BODIES)
//   assert "liIndex >= 0 && liIndex < KI_MAX_NUM_COLLIDABLE_BODIES_PER_DEFORMABLE_OBJECT"
//          (file ../Physics/DeformationManager/DeformationPhysics/BrnImpulsePasser.cpp, line 135
//          == THIS function's authored home; non-gating tripwire, fire-and-continue)
//   stwx sensorPtr, 4*(liIndex + 0x639)(owner)  ; == mapCollidableBodies[liIndex] = lpBody
//     (owner+0x18E4 is DeformableObject::mImpulsePasser's map base; the console slot is 4 bytes,
//      the host slot is the widened pointer -- access is by member name, never raw offset).
//
// The remaining methods (Construct/Destruct/Prepare/Release/PassOnImpulse/ApplyLocalImpulse/
// ClearVariables) stay declare-only until a caller lands them with their own asm witness.

namespace BrnPhysics
{
namespace Deformation
{
    // Inline-attested at ResetSensors @0x82623EB8..0x82623F00 (see the TU banner).
    void ImpulsePasser::SetCollidableBodyMap(s32 liIndex, CollidableBody* lpBody)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < KI_MAX_COLLIDABLE_BODIES,
                   "liIndex >= 0 && liIndex < KI_MAX_NUM_COLLIDABLE_BODIES_PER_DEFORMABLE_OBJECT");   // BrnImpulsePasser.cpp:135
        mapCollidableBodies[liIndex] = lpBody;
    }

    // =============================================================================================
    // PassOnImpulse -- ⭐ ADDED 2026-08-14 (deformation-mount wave). X360 export HOLE; the PS3
    // twin SHIPS the body (@0x6B4FB8, 61 instr, DecFIGS) and is the authority:
    //   * assert lu8ReceivingBodyIndex < 25 ("lu8ReceivingBodyIndex < KI_MAX_NUM_COLLIDABLE_
    //     BODIES_PER_DEFORMABLE_OBJECT", BrnImpulsePasser.cpp:156, fire-and-continue);
    //   * assert the slot is wired ("mapCollidableBodies[lu8ReceivingBodyIndex] != NULL", :157,
    //     fire-and-continue -- the PS3 falls back through and CALLS anyway; kept);
    //   * virtual-dispatch the impulse into the slot's body: the PS3 calls vtable slot 1
    //     (`lwz r11, 4(vptr)`), which in CollidableBody's declared order is
    //     RecievePassedOnImpulse -- semantically the pass-on receipt.
    // Caller: DeformationSensor::RecievePassedOnImpulse (the chain forward).
    // =============================================================================================
    void ImpulsePasser::PassOnImpulse(u8 lu8Index, const ImpulseParams* lpImpulseParams,
                                      VecFloat lvfPassedMagnitude)
    {
        CGS_ASSERT(static_cast<s32>(lu8Index) < KI_MAX_COLLIDABLE_BODIES,
                   "lu8ReceivingBodyIndex < KI_MAX_NUM_COLLIDABLE_BODIES_PER_DEFORMABLE_OBJECT");   // :156
        CGS_ASSERT(mapCollidableBodies[lu8Index] != nullptr,
                   "mapCollidableBodies[lu8ReceivingBodyIndex] != NULL");                            // :157

        // The console calls through regardless (fire-and-continue past both asserts).
        mapCollidableBodies[lu8Index]->RecievePassedOnImpulse(lpImpulseParams, lvfPassedMagnitude);
    }
}
}
