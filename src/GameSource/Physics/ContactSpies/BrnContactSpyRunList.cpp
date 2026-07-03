#include "GameSource/Physics/ContactSpies/BrnContactSpyRunList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert Begin/Fire/End triad

// BrnPhysics::ContactSpy::ContactSpyRunData::Construct  @ X360 0x8259BA80
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (store-for-store). Static factory that stamps a
// run-data record: entity id + the three accumulated stress/point vectors + the [start,length)
// slice into the owning contact queue. Called by ContactSpyQueue<T,N>::SortAndCreateRunList
// (RaceCar/Traffic/PhysicalCarPart/Prop specialisations). Vector args arrive in v1/v2/v3.
namespace BrnPhysics
{
namespace ContactSpy
{
    ContactSpyRunData* ContactSpyRunData::Construct(ContactSpyRunData* lpResult,
                                                    EntityId lEntityId,
                                                    const Vector3& lrTotalFrictionStress,
                                                    const Vector3& lrAverageStress,
                                                    const Vector3& lrAverageContactPoint,
                                                    s32 liStartIndex,
                                                    s32 liRunLength)
    {
        CGS_ASSERT(liStartIndex >= 0, "liStartIndex >= 0");
        CGS_ASSERT(liRunLength > 0, "liRunLength > 0");

        lpResult->mEntityId            = lEntityId;             // @0
        lpResult->mTotalFrictionStress = lrTotalFrictionStress; // @0x10 (v1)
        lpResult->mAverageStress       = lrAverageStress;       // @0x20 (v2)
        lpResult->mAverageContactPoint = lrAverageContactPoint; // @0x30 (v3)
        lpResult->miStartIndex         = liStartIndex;          // @0x40
        lpResult->miRunLength          = liRunLength;           // @0x44

        return lpResult;
    }
}
}
