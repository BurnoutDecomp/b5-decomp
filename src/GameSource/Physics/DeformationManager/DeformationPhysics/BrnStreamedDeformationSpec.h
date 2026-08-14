#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"          // CgsGeometric::AxisAlignedBox (GetBoundingBox out-param)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"   // EBodyParts (committed home)
#include "SharedClasses/Physics/Deformation/BrnSensorSpec.h"                       // SensorSpec (canonical home; embedded by value below)
#include "GameShared/GameClasses/Graphics/CgsSerialisedPtr.h"                      // CgsGraphics::Ptr32<T> (the 32-bit serialised slot)
#include <cstddef>                                                                 // offsetof (the layout tripwires)

// BrnPhysics::Deformation::StreamedDeformationSpec and its inline spec sub-structs.
// Reconstructed from BURNOUT_X360_ARTIST.XEX with member names/types from the DecFIGS
// DWARF (BrnStreamedDeformationSpec.h / BrnGlassPaneSpec.h / BrnSensorSpec.h). This is
// the streamed (resource-baked) description of a deformable vehicle: tag/driven/IK part
// tables, glass panes, locator lists, the four wheel specs and the deformation sensor grid.
//
// ⛔⛔ LAYOUT NOTE CORRECTED (wheel-render wave, 2026-08-03). This banner used to say the
// X360 offsets "do not (and need not) reproduce byte-for-byte on a 64-bit host" because the
// accessors are written by member name. THAT IS WRONG, and it was wrong in the way this
// project has been bitten by five times: StreamedDeformationSpec is NOT a host object, it
// IS the streamed resource image. The bytes arrive off disc in the console's 32-bit-pointer
// shape and the resource type relocates them in place, so a member name only computes the
// right address if the STRUCT reproduces that shape. With host-width pointers the eight
// embedded pointer slots (four tables + one per locator list) each grew four bytes and
// pushed everything after them along.
//
// MEASURED, which is how it was caught: reading the four authored WheelSpecs out of the
// shipped VEH_PUSMC01 deformation spec through this struct returned
//   pos(0.259968, 0.662665, 0.662665) scale(0, 0, 0)   -- twice, identically --
// i.e. two duplicate "wheels" with a zero scale and y == z. Those are not wheel placements;
// that is a read starting 28-odd bytes early, inside mHandlingBodyDimensions.
//
// The pointer slots are now CgsGraphics::Ptr32<T> and the console offsets the asm attests
// (maGlassPaneData @ +28, miNumGlassPanes @ +32, maWheelSpecs @ +80, the sensor grid @ +272,
// mCarModelSpaceToHandlingBodySpaceTransform @ +1552, mu8NumDeformationSensors @ +1618,
// mCurrentCOMOffset @ +1632, mMeshOffset @ +1648) are pinned by static_assert below.
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

        // Batch (all bounds-assert luTag < muNumLocators, non-gating tripwires):
        LocatorPointSpec       CreateLo(u32 luTag) const;      // X360 0x825E32F0 return-by-value copy (BrnStreamedDeformationSpec.cpp:481)
        ETagPointType          GetLocatorTy(u32 luTag) const;  // X360 0x82704930 meTagPointType at element +64 (.h:94)
        const Matrix44Affine*  GetLocatorXf(u32 luTag) const;  // X360 0x825B31E0 &element.mLocatorMatrix (.h:98)

        // Streamed record: count then a 32-bit array slot (asm count @ +0, ptr @ +4 -- eight
        // bytes per list, which is what puts mHandlingBodyDimensions at +64 and the wheel
        // specs at +80). Public because this IS the on-disc image and its owner rebases the
        // slot in place, exactly as SensorSpec's members are public for the same reason.
        u32                                    muNumLocators;
        CgsGraphics::Ptr32<LocatorPointSpec>   mpaLocatorPoints;

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

        // BrnStreamedDeformationSpec.h:222 -- checked IK/driven-part accessor.
        // X360 @ 0x825B3258: asserts liIndex < miNumberOfIKParts and liIndex >= 0 (non-gating
        // tripwires), then returns &maIKPartData[liIndex] (asm: 480 * liIndex + maIKPartData).
        // (Identified as GetDrivenPartSpec via the caller DeformableObject::PrepareIKPart:833; the
        // misnomer GetIKPart belongs to DeformableObject and returns a different type.)
        const IKBodyPartSpec* GetDrivenPartSpec(s32 liIndex) const;

        // ⭐ ADDED 2026-08-14 (walls wave): the per-index POINT-spec accessors, siblings of
        // GetDrivenPartSpec (same checked shape; strides 80/32 are the TU's banner-attested table
        // strides). They un-block ResetDeformation's tag/driven rebuild loops -- the _Lifecycle
        // "spec accessors not exposed" FLAGs were stale against this header.
        const TagPointSpec*      GetTagPointSpec(s32 liIndex) const;
        const IKDrivenPointSpec* GetDrivenPointSpec(s32 liIndex) const;

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

        // ---- THE STREAMED RECORD (public: this struct IS the on-disc image) -------------
        // Every pointer is a 32-bit slot the resource FixUp rebases in place -- see the
        // banner. Widening any of them moves every member after it.
        s32                              miVersionNumber;
        CgsGraphics::Ptr32<TagPointSpec>      maTagPointData;
        s32                              miNumberOfTagPoints;
        CgsGraphics::Ptr32<IKDrivenPointSpec> maDrivenPointData;
        s32                              miNumberOfDrivenPoints;
        CgsGraphics::Ptr32<IKBodyPartSpec>    maIKPartData;
        s32                              miNumberOfIKParts;
        CgsGraphics::Ptr32<GlassPaneSpec>     maGlassPaneData;
        s32                              miNumGlassPanes;
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

    // ---- THE LAYOUT TRIPWIRES ---------------------------------------------------------
    // Every one of these offsets is quoted from the X360 asm of a function that reads it
    // (the accessors' element arithmetic, GetBoundingBox's sensor walk, TransformToNewCOMSpace's
    // COM/mesh offsets, and ActiveRaceCar::OnResourcesLoaded @0x822EB2FC reading
    // `spec + 96 + 48*i` for wheel i's scale == maWheelSpecs[i].mScale). If one of these
    // fails, a pointer slot has been widened back to host width.
    static_assert(sizeof(LocatorPointSpecList) == 8, "LocatorPointSpecList is {u32 count, u32 slot}");
    static_assert(offsetof(StreamedDeformationSpec, maGlassPaneData) == 28, "maGlassPaneData @ +28");
    static_assert(offsetof(StreamedDeformationSpec, miNumGlassPanes) == 32, "miNumGlassPanes @ +32");
    static_assert(offsetof(StreamedDeformationSpec, mHandlingBodyDimensions) == 64, "mHandlingBodyDimensions @ +64");
    static_assert(offsetof(StreamedDeformationSpec, maWheelSpecs) == 80, "maWheelSpecs @ +80");
    static_assert(sizeof(WheelSpec) == 48, "WheelSpec stride 48 (spec + 96 + 48*i is its mScale)");
    static_assert(offsetof(StreamedDeformationSpec, maDeformationSensorSpecs) == 272, "sensor grid @ +272");
    static_assert(offsetof(StreamedDeformationSpec, mCarModelSpaceToHandlingBodySpaceTransform) == 1552,
                  "COM transform @ +1552 (ActiveRaceCar::OnResourcesLoaded reads Def + 1552)");
    static_assert(offsetof(StreamedDeformationSpec, mu8NumDeformationSensors) == 1618, "sensor count @ +1618");
    static_assert(offsetof(StreamedDeformationSpec, mCurrentCOMOffset) == 1632, "mCurrentCOMOffset @ +1632");
    static_assert(offsetof(StreamedDeformationSpec, mMeshOffset) == 1648, "mMeshOffset @ +1648");
}
}
