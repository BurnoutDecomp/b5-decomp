#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ARTIST8270BF70 copies through meTrafficType; trailing alignment bytes are untouched.
namespace BrnPhysics
{
namespace Vehicle
{
    static const char KAC_ARTIST_EVENT_FILE[] = "..\\..\\..\\GameSource\\Physics/VehicleManager/SharedIO/BrnVehicleEvents.h";

    CreateArticulatedTrafficEvent&
    CreateArticulatedTrafficEvent::operator=(const CreateArticulatedTrafficEvent& lrOther)
    {
        mInitialTransform_Cab = lrOther.mInitialTransform_Cab;
        mInitialTransform_Trailer = lrOther.mInitialTransform_Trailer;
        mInitialVelocity_Cab = lrOther.mInitialVelocity_Cab;
        mInitialVelocity_Trailer = lrOther.mInitialVelocity_Trailer;
        mAngularVelocity_Cab = lrOther.mAngularVelocity_Cab;
        mAngularVelocity_Trailer = lrOther.mAngularVelocity_Trailer;
        mVolumeInstanceID_Cab = lrOther.mVolumeInstanceID_Cab;
        mVolumeInstanceID_Trailer = lrOther.mVolumeInstanceID_Trailer;
        mAssetAttribKey_Cab = lrOther.mAssetAttribKey_Cab;
        mAssetAttribKey_Trailer = lrOther.mAssetAttribKey_Trailer;
        mModelHandle_Cab = lrOther.mModelHandle_Cab;
        mModelHandle_Trailer = lrOther.mModelHandle_Trailer;
        mCgsId_Cab = lrOther.mCgsId_Cab;
        mCgsId_Trailer = lrOther.mCgsId_Trailer;
        meTrafficType = lrOther.meTrafficType;
        return *this;
    }

    // BrnPhysics::Vehicle::CreateArticulatedTrafficEvent::GetCreateCabEvent  @ 0x825B3030
    // Projects the CAB half of an articulated-traffic event onto a CreatePhysicalTrafficEvent.
    // Field-for-field copy (the X360 body coalesces adjacent fields into ld/std and VMX lvx128/
    // stvx128 16-byte block copies): the cab VolumeInstanceId, the cab transform/velocity/angular-
    // velocity, the cab asset-attrib key and model handle, the shared traffic type, mbIsCab=true and
    // the cab CgsID. mCrasherID (out+0x8) is deliberately NOT written -- left as the caller supplied it.
    void CreateArticulatedTrafficEvent::GetCreateCabEvent( CreatePhysicalTrafficEvent* lpCreateCabEvent ) const
    {
        CGS_ASSERT_AT( lpCreateCabEvent != 0, "lpCreateCabEvent != NULL", KAC_ARTIST_EVENT_FILE, 477 );

        lpCreateCabEvent->mVolumeInstanceID  = mVolumeInstanceID_Cab;
        lpCreateCabEvent->mInitialTransform  = mInitialTransform_Cab;
        lpCreateCabEvent->mInitialVelocity   = mInitialVelocity_Cab;
        lpCreateCabEvent->mAngularVelocity   = mAngularVelocity_Cab;
        lpCreateCabEvent->mCarAssetAttribKey = mAssetAttribKey_Cab;
        lpCreateCabEvent->mModelHandle       = mModelHandle_Cab;
        lpCreateCabEvent->meTrafficType      = meTrafficType;
        lpCreateCabEvent->mCgsID             = mCgsId_Cab;
        lpCreateCabEvent->mbIsCab            = true;
    }

    // BrnPhysics::Vehicle::CreateArticulatedTrafficEvent::GetCreateTrailerEvent  @ 0x825B3108
    // As GetCreateCabEvent, but projects the TRAILER half: the trailer VolumeInstanceId/transform/
    // velocities/attrib-key/model-handle and CgsID, the shared traffic type, and mbIsCab=false.
    // mCrasherID (out+0x8) is again left unwritten.
    void CreateArticulatedTrafficEvent::GetCreateTrailerEvent( CreatePhysicalTrafficEvent* lpCreateTrailerEvent ) const
    {
        CGS_ASSERT_AT( lpCreateTrailerEvent != 0, "lpCreateTrailerEvent != NULL", KAC_ARTIST_EVENT_FILE, 496 );

        lpCreateTrailerEvent->mVolumeInstanceID  = mVolumeInstanceID_Trailer;
        lpCreateTrailerEvent->mInitialTransform  = mInitialTransform_Trailer;
        lpCreateTrailerEvent->mInitialVelocity   = mInitialVelocity_Trailer;
        lpCreateTrailerEvent->mAngularVelocity   = mAngularVelocity_Trailer;
        lpCreateTrailerEvent->mCarAssetAttribKey = mAssetAttribKey_Trailer;
        lpCreateTrailerEvent->mModelHandle       = mModelHandle_Trailer;
        lpCreateTrailerEvent->meTrafficType      = meTrafficType;
        lpCreateTrailerEvent->mCgsID             = mCgsId_Trailer;
        lpCreateTrailerEvent->mbIsCab            = false;
    }
}
}
