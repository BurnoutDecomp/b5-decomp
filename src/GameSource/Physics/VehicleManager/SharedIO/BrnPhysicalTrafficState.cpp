#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include <cstring>

// ARTIST82284EE8 copies the vector blocks and named scalar fields; the alignment byte is untouched.
namespace BrnPhysics
{
namespace Vehicle
{
    PhysicalTrafficState::PhysicalTrafficState(const PhysicalTrafficState& lrOther)
    {
        std::memcpy(maWheels, lrOther.maWheels, sizeof(maWheels));
        std::memcpy(&mTransform, &lrOther.mTransform, sizeof(mTransform));
        std::memcpy(&mvRoadTestNormal_HeightAboveRoad, &lrOther.mvRoadTestNormal_HeightAboveRoad, sizeof(mvRoadTestNormal_HeightAboveRoad));
        std::memcpy(&mLinearVelocity, &lrOther.mLinearVelocity, sizeof(mLinearVelocity));
        std::memcpy(maWheelTransforms, lrOther.maWheelTransforms, sizeof(maWheelTransforms));
        mEntityID = lrOther.mEntityID;
        mfSpeed = lrOther.mfSpeed;
        mbFrozen = lrOther.mbFrozen;
        mbIsDeforming = lrOther.mbIsDeforming;
        mbIsFatallyCrashing = lrOther.mbIsFatallyCrashing;
        mfSteering = lrOther.mfSteering;
    }
}
}
