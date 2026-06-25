#ifndef BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H
#define BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"

namespace BrnTraffic
{
    class VehicleTypeRuntime
    {
    public:
        static const u32 KU_NUM_PAINT_COLOURS_PER_VEHICLE = 20;

        Vector4 PickPaintColourForVehicle(u32 luSeed,
                                          s32 liNumAvailableColours,
                                          const Vector4* lpaPaintColours) const;

        f32 GetCabPivotDistance() const { return mCabPivot_TrailerPivot_BackAxle_FwdAxle.x; }
        f32 GetTrailerPivotDistance() const { return mCabPivot_TrailerPivot_BackAxle_FwdAxle.y; }
        f32 GetBackAxleOffset() const { return mCabPivot_TrailerPivot_BackAxle_FwdAxle.z; }
        f32 GetForwardAxleOffset() const { return mCabPivot_TrailerPivot_BackAxle_FwdAxle.w; }

    private:
        Vector3 mBBoxOffset;
        Vector3 mBBoxHalfSize;
        Vector4 mCabPivot_TrailerPivot_BackAxle_FwdAxle;
        Vector4 mMass_WheelRadius_Z_W;
        Attribute::Key mAttribKey;
        s8 maiPaintColours[KU_NUM_PAINT_COLOURS_PER_VEHICLE];
        s8 miNumPaintColours;
    };
}

#endif // BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H
