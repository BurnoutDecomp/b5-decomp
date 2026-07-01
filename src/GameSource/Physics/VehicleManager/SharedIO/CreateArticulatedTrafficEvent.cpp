#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnPhysics::Vehicle::CreateArticulatedTrafficEvent copy assignment  @ 0x8270BF70
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 body is a pure bitwise copy of the whole
// object: VMX (lvx128/stvx128) 16-byte block copies across the cab/trailer Matrix44Affine
// transforms + Vector3 velocity region [0..0xC0), then the scalar tail copied with ld/std and
// lwz/stw -- the two VolumeInstanceIds, the two Attribute::Keys, the two ResourceHandles, the two
// CgsIDs and meTrafficType (last store at +256). CreateArticulatedTrafficEvent is trivially
// copyable, so `= default` reproduces the X360 memberwise copy exactly; kept out-of-line so this
// ledger func has a definition site.
namespace BrnPhysics
{
namespace Vehicle
{
    CreateArticulatedTrafficEvent&
    CreateArticulatedTrafficEvent::operator=( const CreateArticulatedTrafficEvent& ) = default;

    // BrnPhysics::Vehicle::CreateArticulatedTrafficEvent::GetCreateCabEvent  @ 0x825B3030
    // Projects the CAB half of an articulated-traffic event onto a CreatePhysicalTrafficEvent.
    // Field-for-field copy (the X360 body coalesces adjacent fields into ld/std and VMX lvx128/
    // stvx128 16-byte block copies): the cab VolumeInstanceId, the cab transform/velocity/angular-
    // velocity, the cab asset-attrib key and model handle, the shared traffic type, mbIsCab=true and
    // the cab CgsID. mCrasherID (out+0x8) is deliberately NOT written -- left as the caller supplied it.
    void CreateArticulatedTrafficEvent::GetCreateCabEvent( CreatePhysicalTrafficEvent* lpCreateCabEvent ) const
    {
        CGS_ASSERT( lpCreateCabEvent != nullptr, "lpCreateCabEvent != NULL" );

        lpCreateCabEvent->mVolumeInstanceID  = mVolumeInstanceID_Cab;
        lpCreateCabEvent->mInitialTransform  = mInitialTransform_Cab;
        lpCreateCabEvent->mInitialVelocity   = mInitialVelocity_Cab;
        lpCreateCabEvent->mAngularVelocity   = mAngularVelocity_Cab;
        lpCreateCabEvent->mCarAssetAttribKey = mAssetAttribKey_Cab;
        lpCreateCabEvent->mModelHandle       = mModelHandle_Cab;
        lpCreateCabEvent->meTrafficType      = meTrafficType;
        lpCreateCabEvent->mbIsCab            = true;
        lpCreateCabEvent->mCgsID             = mCgsId_Cab;
    }

    // BrnPhysics::Vehicle::CreateArticulatedTrafficEvent::GetCreateTrailerEvent  @ 0x825B3108
    // As GetCreateCabEvent, but projects the TRAILER half: the trailer VolumeInstanceId/transform/
    // velocities/attrib-key/model-handle and CgsID, the shared traffic type, and mbIsCab=false.
    // mCrasherID (out+0x8) is again left unwritten.
    void CreateArticulatedTrafficEvent::GetCreateTrailerEvent( CreatePhysicalTrafficEvent* lpCreateTrailerEvent ) const
    {
        CGS_ASSERT( lpCreateTrailerEvent != nullptr, "lpCreateTrailerEvent != NULL" );

        lpCreateTrailerEvent->mVolumeInstanceID  = mVolumeInstanceID_Trailer;
        lpCreateTrailerEvent->mInitialTransform  = mInitialTransform_Trailer;
        lpCreateTrailerEvent->mInitialVelocity   = mInitialVelocity_Trailer;
        lpCreateTrailerEvent->mAngularVelocity   = mAngularVelocity_Trailer;
        lpCreateTrailerEvent->mCarAssetAttribKey = mAssetAttribKey_Trailer;
        lpCreateTrailerEvent->mModelHandle       = mModelHandle_Trailer;
        lpCreateTrailerEvent->meTrafficType      = meTrafficType;
        lpCreateTrailerEvent->mbIsCab            = false;
        lpCreateTrailerEvent->mCgsID             = mCgsId_Trailer;
    }
}
}
