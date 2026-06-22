#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"   // EBodyParts (committed home)

// BrnPhysics::Deformation::StreamedDeformationSpec and its inline spec sub-structs.
// Reconstructed from BURNOUT_X360_ARTIST.XEX with member names/types from the DecFIGS
// DWARF (BrnStreamedDeformationSpec.h / BrnGlassPaneSpec.h / BrnSensorSpec.h). This is
// the streamed (resource-baked) description of a deformable vehicle: tag/driven/IK part
// tables, glass panes, locator lists, the four wheel specs and the deformation sensor grid.
//
// Layout faithfulness: the X360 member offsets used by the asm accessors (maGlassPaneData @
// +28, miNumGlassPanes @ +32, maWheelSpecs @ +80) are 32-bit-pointer offsets that do not
// (and need not) reproduce byte-for-byte on a 64-bit host. The accessors here are bodied
// BY MEMBER NAME, so the host compiler recomputes the correct element addresses; semantic
// parity (which member, which stride multiple, which bounds assert) is preserved exactly.
namespace BrnPhysics
{
namespace Deformation
{
    // Forward-declared sibling spec tables only ever touched through pointers in this TU's
    // accessors are HONEST forward declarations -- their full layout lives in their own
    // DWARF homes (BrnTagPoint.h / BrnIKDrivenPoint.h / BrnIKBodyPart.h) and is not needed
    // to body GetGlassPaneSpec / GetWheelSpec.
    struct TagPointSpec;
    struct IKDrivenPointSpec;
    struct IKBodyPartSpec;

    enum ETagPointType : s32 { E_TAG_POINT_TYPE_INVALID = -1 };   // BrnStreamedDeformationSpec.h (DWARF); placeholder enumerator

    // BrnStreamedDeformationSpec.h:49 -- one suspension wheel's streamed placement.
    // sizeof == 48 (0x30): two 16-byte Vector3 lanes + a 4-byte index, 16-byte aligned.
    struct WheelSpec
    {
        Vector3 mPosition;
        Vector3 mScale;
        s32     liTagPointIndex;
    };

    // BrnGlassPaneSpec.h:49 -- one streamed breakable glass pane.
    // sizeof == 112 (0x70): mNormal(16) + maCornerPositionOffsets[4](64) + maiPointIndex[4](8)
    // + mabSkinToControlPoint[4](4) + 3x i16(6) + mePartType(4), 16-byte aligned.
    struct alignas(16) GlassPaneSpec
    {
        static const s8 KI_NUM_POINTS_PER_GLASS_PANE = 4;

        Vector3    mNormal;
        Vector3    maCornerPositionOffsets[4];
        s16        maiPointIndex[4];
        bool       mabSkinToControlPoint[4];
        s16        miParentBodyPart;
        s16        miCrackSensor;
        s16        miSmashSensor;
        EBodyParts mePartType;
    };

    // BrnSensorSpec.h:48 -- one deformation sensor's streamed parameters.
    // sizeof == 64: mInitialOffset(16) + maDirectionParams[6](24) + mfRadius(4) +
    // maNextSensor[6](6) + mu8SceneIndex(1) + mu8AbsorbtionLevel(1) +
    // mau8NextBoundarySensor[2](2), 16-byte aligned.
    struct alignas(16) SensorSpec
    {
        struct PerDirectionParams { f32 mCompressionLimits; };

        Vector3            mInitialOffset;
        PerDirectionParams maDirectionParams[6];
        f32                mfRadius;
        u8                 maNextSensor[6];
        u8                 mu8SceneIndex;
        u8                 mu8AbsorbtionLevel;
        u8                 mau8NextBoundarySensor[2];
    };

    // BrnStreamedDeformationSpec.h:69 -- one streamed locator point (generic/camera/light tag).
    struct LocatorPointSpec
    {
        Matrix44Affine mLocatorMatrix;
        ETagPointType  meTagPointType;
        s16            miIkPartIndex;
        u8             mu8SkinPoint;
    };

    // BrnStreamedDeformationSpec.h:87 -- a streamed list of locator points (count + fixed-up ptr).
    struct LocatorPointSpecList
    {
    private:
        u32               muNumLocators;
        LocatorPointSpec* mpaLocatorPoints;
    };

    // BrnStreamedDeformationSpec.h:160 -- the full streamed deformation spec record.
    struct StreamedDeformationSpec
    {
    public:
        // BrnStreamedDeformationSpec.h:230 -- checked glass-pane accessor.
        // X360 @ 0x825B32D8: asserts liIndex < miNumGlassPanes and liIndex >= 0 (non-gating
        // tripwires), then returns &maGlassPaneData[liIndex] (asm: 112 * liIndex + maGlassPaneData).
        const GlassPaneSpec* GetGlassPaneSpec(s32 liIndex) const;

        // BrnStreamedDeformationSpec.h:256 -- checked wheel-spec accessor.
        // X360 @ 0x822A0328: asserts liWheel < eNumWheels (4) (non-gating tripwire), then returns
        // &maWheelSpecs[liWheel] (asm: 48 * liWheel + this + 80).
        const WheelSpec* GetWheelSpec(s32 liWheel) const;

    private:
        s32                  miVersionNumber;
        TagPointSpec*        maTagPointData;
        s32                  miNumberOfTagPoints;
        IKDrivenPointSpec*   maDrivenPointData;
        s32                  miNumberOfDrivenPoints;
        IKBodyPartSpec*      maIKPartData;
        s32                  miNumberOfIKParts;
        GlassPaneSpec*       maGlassPaneData;
        s32                  miNumGlassPanes;
        LocatorPointSpecList mGenericTags;
        LocatorPointSpecList mCameraTags;
        LocatorPointSpecList mLightTags;
        Vector3              mHandlingBodyDimensions;
        WheelSpec            maWheelSpecs[4];
        SensorSpec           maDeformationSensorSpecs[20];
        Matrix44Affine       mCarModelSpaceToHandlingBodySpaceTransform;
        u8                   mu8SpecID;
        u8                   mu8NumVehicleBodies;
        u8                   mu8NumDeformationSensors;
        u8                   mu8NumGraphicsParts;
        Vector3              mCurrentCOMOffset;
        Vector3              mMeshOffset;
        Vector3              mRigidBodyOffset;
        Vector3              mCollisionOffset;
        Vector3              mInertiaTensor;
    };
}
}
