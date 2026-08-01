// BrnPhysics::Vehicle::CreateRaceCarEvent ledger TU.
//
// Home of the one ledger function for CreateRaceCarEvent: operator= @0x822AE040. The event
// struct itself is reconstructed in BrnVehicleEvents.h; this TU provides the out-of-line
// assignment-operator body (called by CgsModule::EventQueue<CreateRaceCarEvent,8>::AddEvent).
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

namespace BrnPhysics
{
namespace Vehicle
{
    // @0x822AE040  BrnPhysics::Vehicle::CreateRaceCarEvent::operator=
    //
    // The X360 body is a flat, bitwise, field-for-field copy of the entire event in declaration
    // order -- there is no per-field logic. The asm shape (which is purely how MSVC chose to copy
    // a trivially-copyable POD of this size) is:
    //   std r9,0(r3)              <- copy the 8-byte head (mVolumeInstanceID @0x00)
    //   lvx128/stvx128 @+0x10,+0x20,+0x30,+0x40  <- four 16-byte VMX rows (mInitialTransform)
    //   lvx128/stvx128 @+0x50,+0x60              <- two 16-byte vectors (mInitialVelocity,
    //                                               mAngularVelocity)
    //   ld/std @+0x70                            <- mCarAssetAttribKey, ALL EIGHT BYTES
    //                                               (CORRECTED 2026-08-01: this line used to
    //                                                read "mCarAssetAttribKey + mModelHandle
    //                                                head", i.e. a 4-byte key. It is not --
    //                                                ProcessCreateEvents @0x82616770 `ld`s the
    //                                                same +0x70 and hands the whole doubleword
    //                                                to Attrib::FindCollection as the collection
    //                                                key. See BrnVehicleEvents.h's banner.)
    //   lwz/stw @+0x78,+0x7C                     <- mModelHandle     (two console pointers)
    //   lwz/stw @+0x80,+0x84                     <- mGraphicsHandle  (two console pointers)
    //   lwz/stw @+0x88                           <- meRaceCarType
    //   lfs/stfs @+0x8C                          <- mfDeformAmount (copied as a float lane)
    //   lwz/stw @+0x90                           <- meBaseDeformationType
    //   lbz/stb @+0x94                           <- mbDisablePhysicsStateReset
    //   lwz/stw @+0x98                           <- miCarStrengthStat
    // CreateRaceCarEvent is trivially copyable, so the defaulted memberwise assignment reproduces
    // exactly this whole-object bitwise copy (the compiler emits the same field copies).
    CreateRaceCarEvent& CreateRaceCarEvent::operator=(const CreateRaceCarEvent&) = default;
}
}
