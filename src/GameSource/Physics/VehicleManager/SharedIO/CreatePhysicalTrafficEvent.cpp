#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include <cstring>

// ARTIST825B7AE8 copies the named fields and vector blocks, leaving alignment bytes untouched.
namespace BrnPhysics
{
namespace Vehicle
{
    CreatePhysicalTrafficEvent&
    CreatePhysicalTrafficEvent::operator=(const CreatePhysicalTrafficEvent& lrOther)
    {
        mVolumeInstanceID = lrOther.mVolumeInstanceID;
        mCrasherID = lrOther.mCrasherID;
        std::memcpy(&mInitialTransform, &lrOther.mInitialTransform, sizeof(mInitialTransform));
        std::memcpy(&mInitialVelocity, &lrOther.mInitialVelocity, sizeof(mInitialVelocity));
        std::memcpy(&mAngularVelocity, &lrOther.mAngularVelocity, sizeof(mAngularVelocity));
        mCarAssetAttribKey = lrOther.mCarAssetAttribKey;
        mModelHandle = lrOther.mModelHandle;
        meTrafficType = lrOther.meTrafficType;
        mbIsCab = lrOther.mbIsCab;
        mCgsID = lrOther.mCgsID;
        return *this;
    }
}
}
