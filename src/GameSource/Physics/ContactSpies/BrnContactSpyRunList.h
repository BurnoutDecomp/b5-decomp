#pragma once

#include "BrnCommonTypes.h"

namespace BrnPhysics
{
    namespace ContactSpy
    {
        // One per-entity run of accumulated contacts. Recovered from
        // BrnContactSpyRunList.h (DecFIGS DWARF). The Vector3 members make it
        // 16-byte aligned, which places the owning queue's inline buffer at +16.
        struct alignas(16) ContactSpyRunData
        {
            EntityId mEntityId;
            Vector3  mTotalFrictionStress;
            Vector3  mAverageStress;
            Vector3  mAverageContactPoint;
            s32      miStartIndex;
            s32      miRunLength;
        };
    }
}
