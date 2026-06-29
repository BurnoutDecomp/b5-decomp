#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"          // CgsGeometric::AxisAlignedBox (GetBoundingBox out-param)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"   // EBodyParts (committed home)
#include "SharedClasses/Physics/Deformation/BrnSensorSpec.h"                       // SensorSpec (canonical home; embedded by value below)

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

    // ETagPointType -- the full streamed tag-point taxonomy (DWARF home
    // SharedClasses/Physics/Deformation/BrnTagPointTypes.h:35). The boost-effect anchors
    // E_TAGPOINT_FXBOOSTPOINT1..4 (41..44) are read by BrnEffects::BoostStateMachine::OnTick
    // to position the four boost-nozzle particle effects. E_TAG_POINT_TYPE_INVALID(-1) is the
    // project's spelling for the no-tag sentinel.
    enum ETagPointType : s32
    {
        E_TAG_POINT_TYPE_INVALID                = -1,
        E_TAGPOINT_PHYSICS_CENTREOFMASS         = 0,
        E_TAGPOINT_LIGHTS_FRONTRUNNINGLEFT      = 1,
        E_TAGPOINT_LIGHTS_FRONTRUNNINGRIGHT     = 2,
        E_TAGPOINT_LIGHTS_REARRUNNINGLEFT       = 3,
        E_TAGPOINT_LIGHTS_REARRUNNINGRIGHT      = 4,
        E_TAGPOINT_LIGHTS_FRONTSPOTLEFT         = 5,
        E_TAGPOINT_LIGHTS_FRONTSPOTRIGHT        = 6,
        E_TAGPOINT_LIGHTS_INDICATORFRONTLEFT    = 7,
        E_TAGPOINT_LIGHTS_INDICATORFRONTRIGHT   = 8,
        E_TAGPOINT_LIGHTS_INDICATORREARLEFT     = 9,
        E_TAGPOINT_LIGHTS_INDICATORREARRIGHT    = 10,
        E_TAGPOINT_LIGHTS_BRAKELEFT             = 11,
        E_TAGPOINT_LIGHTS_BRAKERIGHT            = 12,
        E_TAGPOINT_LIGHTS_BRAKECENTRE           = 13,
        E_TAGPOINT_LIGHTS_REVERSELEFT           = 14,
        E_TAGPOINT_LIGHTS_REVERSERIGHT          = 15,
        E_TAGPOINT_LIGHTS_SPOTLIGHT1            = 16,
        E_TAGPOINT_LIGHTS_SPOTLIGHT2            = 17,
        E_TAGPOINT_LIGHTS_BLUESTWOS1            = 18,
        E_TAGPOINT_LIGHTS_BLUESTWOS2            = 19,
        E_TAGPOINT_TYREWELL_FRONTLEFT           = 20,
        E_TAGPOINT_TYREWELL_FRONTRIGHT          = 21,
        E_TAGPOINT_TYREWELL_REARLEFT            = 22,
        E_TAGPOINT_TYREWELL_REARRIGHT           = 23,
        E_TAGPOINT_TYREWELL_ADDITIONALLEFT      = 24,
        E_TAGPOINT_TYREWELL_ADDITIONALRIGHT     = 25,
        E_TAGPOINT_AXLEPOINT_FRONT              = 26,
        E_TAGPOINT_AXLEPOINT_REAR               = 27,
        E_TAGPOINT_ARTICULATIONPOINT_FRONT      = 28,
        E_TAGPOINT_ARTICULATIONPOINT_REAR       = 29,
        E_TAGPOINT_ATTACHPOINT                  = 30,
        E_TAGPOINT_FXGLASSSMASHPOINT1           = 31,
        E_TAGPOINT_FXGLASSSMASHPOINT2           = 32,
        E_TAGPOINT_FXGLASSSMASHPOINT3           = 33,
        E_TAGPOINT_FXGLASSSMASHPOINT4           = 34,
        E_TAGPOINT_FXGLASSSMASHPOINT5           = 35,
        E_TAGPOINT_FXGLASSSMASHPOINT6           = 36,
        E_TAGPOINT_FXGLASSSMASHPOINT7           = 37,
        E_TAGPOINT_FXGLASSSMASHPOINT8           = 38,
        E_TAGPOINT_FXGLASSSMASHPOINT9           = 39,
        E_TAGPOINT_FXGLASSSMASHPOINT10          = 40,
        E_TAGPOINT_FXBOOSTPOINT1                = 41,
        E_TAGPOINT_FXBOOSTPOINT2                = 42,
        E_TAGPOINT_FXBOOSTPOINT3                = 43,
        E_TAGPOINT_FXBOOSTPOINT4                = 44,
        E_TAGPOINT_FXFIREPOINT                  = 45,
        E_TAGPOINT_FXSTEAMPOINT                 = 46,
        E_TAGPOINT_FXPOV_FRONTLEFT              = 47,
        E_TAGPOINT_FXPOV_FRONTRIGHT             = 48,
        E_TAGPOINT_FXPOV_REARLEFT               = 49,
        E_TAGPOINT_FXPOV_REARRIGHT              = 50,
        E_TAGPOINT_FXDASHBOARD                  = 51,
        E_TAGPOINT_FXENGINE                     = 52,
        E_TAGPOINT_FXTRUNK                      = 53,
        E_TAGPOINT_FXPETROL_TANK                = 54,
        E_TAGPOINT_FXPELVIS_FRONTLEFT           = 55,
        E_TAGPOINT_PAYLOAD                      = 56,
        E_TAGPOINT_COUNT                        = 57,
    };   // DWARF BrnTagPointTypes.h:35

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

    // SensorSpec (one deformation sensor's streamed parameters) is now homed canonically in
    // SharedClasses/Physics/Deformation/BrnSensorSpec.h (included above) and embedded by value below as
    // maDeformationSensorSpecs[20]. Its local copy previously lived here; it was REMOVED to keep a single
    // ODR-correct definition. The committed BrnStreamedDeformationSpec.cpp's field reads
    // (.mInitialOffset @ +0, .mfRadius @ +40) are unaffected -- the canonical layout matches.

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
        // X360 reads the count then walks the array (stride sizeof(LocatorPointSpec) == 80) to
        // bounds-check each locator's miIkPartIndex during StreamedDeformationSpec::FixUp. Declared
        // here so that fix-up loop can spell the access by name (asm: count @ +0, array ptr @ +4,
        // miIkPartIndex @ element +68).
        u32 GetNumLocatorPoints() const { return muNumLocators; }
        const LocatorPointSpec* GetLocatorSpec(u32 luIndex) const { return &mpaLocatorPoints[luIndex]; }

    private:
        u32               muNumLocators;
        LocatorPointSpec* mpaLocatorPoints;

        // StreamedDeformationSpec::FixUp / FixDown rebase the embedded mpaLocatorPoints offset of
        // each of the three locator lists in place (add / subtract the stream base), exactly as the
        // X360 serialiser does inline.
        friend struct StreamedDeformationSpec;
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

        // Live deformation-sensor count (mu8NumDeformationSensors; asm spec+1618). The debug component's
        // selected-sensor slider range-checks against this.
        s32 GetNumDeformationSensors() const { return static_cast<s32>(mu8NumDeformationSensors); }

        // Live tag-point / driven-point / IK-part counts (the spec tables the rig debug walk bounds
        // against). asm spec+8 / spec+16 / spec+24.
        s32 GetNumberOfTagPoints() const { return miNumberOfTagPoints; }
        s32 GetNumberOfDrivenPoints() const { return miNumberOfDrivenPoints; }
        s32 GetNumberOfIKParts() const { return miNumberOfIKParts; }

        // The four streamed wheel specs the detached-wheel debug draw reads (asm DrawDetachedWheels:
        // *(spec+...) wheel spec table). Returns the wheel spec at liWheel, or the typed table accessor.
        const WheelSpec* GetW(s32 liWheel) const { return GetWheelSpec(liWheel); }

        // BrnStreamedDeformationSpec.h:281 -- fill an axis-aligned bounding box that contains every
        // deformation sensor sphere. X360 @ 0x825BA9E8: accumulate min/max of (sensorOffset +/- radius)
        // over the mu8NumDeformationSensors sensors, then write min->box.mMin, max->box.mMax.
        void GetBoundingBox(CgsGeometric::AxisAlignedBox& lBoxOut) const;

        // BrnStreamedDeformationSpec.h:234 -- re-express all streamed geometry in a new centre-of-mass
        // frame. X360 @ 0x825E3148: when the COM actually moves, shift every sensor/tag/driven/joint
        // point by (newCOM - oldCOM), pull mMeshOffset the other way, and record the new COM.
        void TransformToNewCOMSpace(Vector3 lCOMOffset);

        // BrnStreamedDeformationSpec.h:310/313 -- serialise-time pointer relocation. FixDown rebases
        // every embedded pointer to a base-relative offset (subtract lpBaseAddress); FixUp rebases
        // them back to absolute (add lpBaseAddress) and re-runs the per-body-part handedness fix-up.
        // X360 FixDown @ 0x82631118, FixUp @ 0x82630E18. Both called by the resource serialiser.
        void FixDown(void* lpBaseAddress);
        void FixUp(void* lpBaseAddress);

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
