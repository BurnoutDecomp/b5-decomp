// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnCollidableBody.cpp
//
// ⭐ NEW TU 2026-08-14 (walls leg 4) -- the CollidableBody direction-vector table + its accessor.
//
// KA_IMPULSE_DIRECTIONS is THE per-direction unit-axis table of the whole impulse system: every
// ENextSensorDirection consumer (VehicleRigidBody::ApplyLocalImpulse / RecievePassedOnImpulse,
// DeformationSensor::ApplyLocalImpulse / RecievePassedOnImpulse, DeformableObject::
// ApplySensorImpulse's six-direction projection loop) resolves its direction index through this
// table. On the consoles it is a DYNAMIC-INIT global (zero in both images); this wave RECOVERED it:
//   * name + accessor: the PS3 exports name both (`BrnPhysics::Deformation::KA_IMPULSE_DIRECTIONS`;
//     CollidableBody::GetDirectionVector @0x6B4D7C is a 6-insn indexed 16-byte load from it);
//   * rows: the PS3 static initializer (__static_initialization_and_destruction_0ii_22 @0x6C2CCC,
//     the KA_IMPULSE block @0x6C56E4; row offsets r22/r21/r20/r19 = 0x10/0x20/0x30/0x40 read from
//     the raw words) writes the six rows as the SIGNED UNIT BODY AXES in enum order;
//   * cross-witness: the DWARF ENextSensorDirection names (E_NSD_POS_XAXIS..E_NSD_NEG_ZAXIS,
//     BrnSharedDeformationEnums.h) are exactly the +X,-X,+Y,-Y,+Z,-Z sequence the rows spell.
//
// This TU retires two flagged-zero placeholder tables (the silent-drop shape -- with zero rows the
// entire impulse chain multiplied to zero): VehicleRigidBody.cpp's mpDirectionVectorTable and
// BrnDeformableObject_Update.cpp's KsaApplyDirection.
// ============================================================================

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnCollidableBody.h"

namespace BrnPhysics
{
namespace Deformation
{
    // The six signed unit body axes, indexed by ENextSensorDirection (16-byte rows, w = 0).
    // PS3 initializer values verbatim (0x3F800000 / 0xBF800000 lanes; all other lanes zero).
    extern const Vector3 KA_IMPULSE_DIRECTIONS[6];
    const Vector3 KA_IMPULSE_DIRECTIONS[6] =
    {
        {  1.0f,  0.0f,  0.0f, 0.0f },   // E_NSD_POS_XAXIS (+0x00)
        { -1.0f,  0.0f,  0.0f, 0.0f },   // E_NSD_NEG_XAXIS (+0x10)
        {  0.0f,  1.0f,  0.0f, 0.0f },   // E_NSD_POS_YAXIS (+0x20)
        {  0.0f, -1.0f,  0.0f, 0.0f },   // E_NSD_NEG_YAXIS (+0x30)
        {  0.0f,  0.0f,  1.0f, 0.0f },   // E_NSD_POS_ZAXIS (+0x40)
        {  0.0f,  0.0f, -1.0f, 0.0f },   // E_NSD_NEG_ZAXIS (+0x50)
    };

    // ---------------------------------------------------------------------------------------------
    // GetDirectionVector -- PS3 @0x6B4D7C (6 insns, the whole body):
    //   slwi r4, direction, 4 ; lwz r9, KA_IMPULSE_DIRECTIONS ; lvx v0, r9, r4 ; stvx -> sret
    // ---------------------------------------------------------------------------------------------
    Vector3 CollidableBody::GetDirectionVector(ENextSensorDirection leDirection)
    {
        return KA_IMPULSE_DIRECTIONS[static_cast<s32>(leDirection)];
    }
}
}
