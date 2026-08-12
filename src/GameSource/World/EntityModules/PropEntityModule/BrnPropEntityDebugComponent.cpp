// ============================================================================
// Bodies for the prop-entity-module debug overlay, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct           @ 0x822A96A8
//   Draw                @ 0x822A9770
//   GetName             @ 0x822A9740
//   OnActivate          @ 0x822C52C8
//   RenderCellGrid      @ 0x822EFF48
//   RenderHUD           @ 0x822FC108
//   RenderInertiaBoxes  @ 0x822DC520
//   RenderModuleStats   @ 0x822DE3F8
//   RenderProps         @ 0x822DCBF8
//   RenderPropStats     @ 0x822DDAE8
//   RenderTrafficLights @ 0x822DD868
//   RenderWorld         @ 0x822EFEB8
//
// The X360 compiler inlines every helper these functions called -- the zone/prop walk
// (mZoneManager.GetProp + the per-zone start/count tables), the rwmath vector ops
// (RwMath::MagnitudeSquared / IsValid / vector subtract), the CgsDev::StrStream stat
// builders, and the rwcollision volume primitive draws -- and folds them into VMX
// intrinsics + flat asserts. This reconstruction RE-ROLLS them back into the clean
// idiomatic source: the X360 pseudocode/asm is authority for the control flow (which flag
// gates which pass, the zone/prop iteration, the distance + state culls, the colours, the
// stat fields, the line numbers/strings of the asserts) and the member offsets; the call
// SHAPES + de-inlined helper names follow the project idiom (cf. the sibling
// TriggerEntityModuleDebugComponent) and, for the overlapping functions, the Feb-2007
// partial source. Authority for declaration shape is the DecFIGS DWARF.
//
// DISTANCE CONSTANTS (X360 .rdata floats squared into the compare): the volume passes cull
// at MagnitudeSquared > 900 == 30^2 (KF_MAX_DIST_FOR_VOLUME_RENDERING == 30); the stat pass
// culls at > 400 == 20^2 (KF_MAX_DIST_FOR_STAT_RENDERING == 20).

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityDebugComponent.h"

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"   // PropEntityModule
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.h"     // PropZoneManager (GetProp / GetNumberOf*)
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"  // PropEntityInstance / EPropState
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"                   // PropPhysicsDataHeader / PropTypeData
#include "SDKs/EATech/rwcollision/volume_debug_access.h"                            // rw::collision::Volume (Draw)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h" // 3D draws
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // 2D draws
#include "GameShared/GameClasses/Development/CgsStrStream.h"                         // CgsDev::StrStream
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                             // CgsCore::SPrintf
#include "rw/rwcore_structs.h"                                                      // rw::RGBA
#include "rw/math/vpu/matrix44affine_operation.h"                                   // Matrix44Affine operator*

#include <cstring>                                                                  // strlen
#include <cmath>                                                                    // acosf (Lean angle stat)

namespace BrnWorld
{
    namespace
    {
        // World-space render distances (the squared form is what the X360 compares).
        const f32 KF_MAX_DIST_FOR_VOLUME_RENDERING = 30.0f;
        const f32 KF_MAX_DIST_FOR_STAT_RENDERING   = 20.0f;

        // --- de-inlined rwmath helpers (the RwMath:: free functions the X360 folded in) ---

        // RwMath::MagnitudeSquared(a - b): the squared distance between two world points.
        inline f32 MagnitudeSquared(const Vector3& lrA, const Vector3& lrB)
        {
            const f32 lfDx = lrA.x - lrB.x;
            const f32 lfDy = lrA.y - lrB.y;
            const f32 lfDz = lrA.z - lrB.z;
            return (lfDx * lfDx) + (lfDy * lfDy) + (lfDz * lfDz);
        }

        // RwMath::IsValid(transform): every lane of all four rows is finite (the X360
        // per-row, per-lane vcmpeqfp self-compare == "not NaN", ANDed over the four rows).
        // Used only to fire a debug assert; resolved by ADL to rw::math::vpu::IsValid
        // (matrix44affine_operation.h), the exact four-row form the asm emits.
        using ::rw::math::vpu::IsValid;

        inline Vector3 MakeVector3(f32 lfX, f32 lfY, f32 lfZ)
        {
            Vector3 lVector;
            lVector.x = lfX;
            lVector.y = lfY;
            lVector.z = lfZ;
            lVector.w = 0.0f;
            return lVector;
        }

        // Broadcast a scalar into all four lanes (the VecFloat the inertia helper takes; the
        // X360 splats the volume radius across the register before the call).
        inline VecFloat lBroadcast(f32 lfValue)
        {
            VecFloat lValue;
            lValue.x = lfValue;
            lValue.y = lfValue;
            lValue.z = lfValue;
            lValue.w = lfValue;
            return lValue;
        }
    }

    // @ 0x822A96A8. Run the base DebugComponent ctor, stash the owning module, clear every
    // render flag, default the cell-grid zoom to 1.0 and the slow-refresh frame counter to 0.
    // (The leading "BaseCollisionGenerator::Destruct" call in the pseudocode is the
    // COMDAT-folded DebugComponent::Construct.)
    void PropEntityDebugComponent::Construct(PropEntityModule* lpPropEntityModule)
    {
        DebugComponent::Construct();

        CGS_ASSERT(lpPropEntityModule != nullptr, "lpPropEntityModule != NULL");
        mpPropEntityModule = lpPropEntityModule;

        mbRenderProps         = false;   // +0x38
        mbRenderPropStats     = false;   // +0x39
        mbRenderPartStats     = false;   // +0x3A
        mbRenderModuleStats   = false;   // +0x3B
        mbRenderCellGrid      = false;   // +0x3C
        mbRenderInertiaBoxes  = false;   // +0x3D
        mbRenderTrafficLights = false;   // +0x3E

        mfCellGridZoom        = 1.0f;    // +0x40
        muFrameCountForUpdate = 0;       // +0x50
    }

    // @ 0x822A9740.
    const char* PropEntityDebugComponent::GetName() const
    {
        return "Prop Entity Module";
    }

    // @ 0x822C52C8. Register the render toggles, the cell-grid zoom (with a 0.1 step), the
    // "Reset props" action, and the owning module's override / online / progression tuning
    // flags with the debug menu.
    void PropEntityDebugComponent::OnActivate()
    {
        DebugComponent::OnActivate();

        RegisterFunction(&PropEntityDebugComponent::ResetProps, this, "Reset props");

        RegisterVariable(&mbRenderProps,         "Render prop collision volumes");
        RegisterVariable(&mbRenderPropStats,     "Render the stats for each prop");
        RegisterVariable(&mbRenderPartStats,     "Render the stats for each part");
        RegisterVariable(&mbRenderModuleStats,   "Render PropEntityModule stats");
        RegisterVariable(&mbRenderCellGrid,      "Render cell grid");

        RegisterVariable(&mfCellGridZoom,        "Cell grid zoom");
        SetStep(&mfCellGridZoom, 0.1f);

        RegisterVariable(&mbRenderTrafficLights, "Render props marked as Traffic Lights");

        CGS_ASSERT(mpPropEntityModule != nullptr, "mpPropEntityModule != NULL");

        RegisterVariable(&mpPropEntityModule->mbUseOverrides,          "Overrides...", "Use Overrides");
        RegisterVariable(&mpPropEntityModule->mfOverrideLeanThreshold, "Overrides...", "Override Lean Threshold");
        RegisterVariable(&mpPropEntityModule->mfOverrideMoveThreshold, "Overrides...", "Override Move Threshold");
        RegisterVariable(&mpPropEntityModule->mfOverrideSmashThreshold,"Overrides...", "Override Smash Threshold");

        RegisterVariable(&mpPropEntityModule->mbCurrentlyOnline,       "Currently online");
        RegisterVariable(&mbRenderInertiaBoxes,                        "Render Inertia Boxes");
        RegisterVariable(&mpPropEntityModule->mbAllowPropProgression,  "Allow prop progression");
    }

    // @ 0x822EFEB8. World-space (3D) debug pass -- each sub-pass gated on its own flag.
    void PropEntityDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        if (mbRenderProps)
        {
            RenderProps(lpDisplay);
        }
        if (mbRenderPropStats)
        {
            RenderPropStats(lpDisplay);
        }
        if (mbRenderInertiaBoxes)
        {
            RenderInertiaBoxes(lpDisplay);
        }
        if (mbRenderTrafficLights)
        {
            RenderTrafficLights(lpDisplay);
        }
    }

    // @ 0x822FC108. Screen-space (2D) debug pass -- module stats + the cell-grid map.
    void PropEntityDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        if (mbRenderModuleStats)
        {
            RenderModuleStats(lpDisplay);
        }
        if (mbRenderCellGrid)
        {
            RenderCellGrid(lpDisplay);
        }
    }

    // @ 0x822A9770. Draw a single collision volume under a world transform, in the debug
    // colour for its primitive type. The volume's relative transform is composed with the
    // prop world transform before drawing (folded into the VMX matrix multiply in the asm).
    void PropEntityDebugComponent::Draw(rw::collision::Volume* lpVolume,
                                        CgsDev::Debug3DImmediateRender* lpDisplay,
                                        Matrix44Affine lTransform) const
    {
        switch (lpVolume->GetType())
        {
            case rw::collision::E_VOLUMETYPE_SPHERE:
            {
                const Vector3& lrCentre = lTransform.Pos();
                const f32 lfRadius = lpVolume->GetRadius();
                lpDisplay->DrawSphere(lrCentre, lfRadius, rw::RGBA(255, 255, 255, 255));
                lpDisplay->DrawHollowSphere(lrCentre, lfRadius, rw::RGBA(255, 255, 255, 255));
            }
            break;

            case rw::collision::E_VOLUMETYPE_CAPSULE:
            {
                const f32 lfRadius     = lpVolume->GetRadius();
                const f32 lfHalfHeight = lpVolume->GetExtent().x;
                const Vector3 lMin = MakeVector3(-lfRadius, -lfRadius, -lfHalfHeight);
                const Vector3 lMax = MakeVector3( lfRadius,  lfRadius,  lfHalfHeight);
                lpDisplay->DrawSolidBox(lMin, lMax, lTransform, rw::RGBA(255, 0, 0, 255));     // -65536  == ARGB 0xFFFF0000
                lpDisplay->DrawBox(lMin, lMax, lTransform, rw::RGBA(0, 0, 0, 255));            // -16777216 == ARGB 0xFF000000
            }
            break;

            case rw::collision::E_VOLUMETYPE_BBOX:
            {
                const Vector3& lrExtent = lpVolume->GetExtent();
                const Vector3 lMin = MakeVector3(-lrExtent.x, -lrExtent.y, -lrExtent.z);
                const Vector3 lMax = MakeVector3( lrExtent.x,  lrExtent.y,  lrExtent.z);
                lpDisplay->DrawSolidBox(lMin, lMax, lTransform, rw::RGBA(255, 255, 255, 255)); // -1 == ARGB 0xFFFFFFFF
                lpDisplay->DrawBox(lMin, lMax, lTransform, rw::RGBA(0, 0, 0, 255));
            }
            break;

            case rw::collision::E_VOLUMETYPE_CYLINDER:
            {
                const f32 lfRadius     = lpVolume->GetRadius();
                const f32 lfHalfHeight = lpVolume->GetExtent().x;
                const Vector3 lMin = MakeVector3(-lfRadius, -lfRadius, -lfHalfHeight);
                const Vector3 lMax = MakeVector3( lfRadius,  lfRadius,  lfHalfHeight);
                lpDisplay->DrawSolidBox(lMin, lMax, lTransform, rw::RGBA(0, 255, 0, 255));     // -16711936 == ARGB 0xFF00FF00
                lpDisplay->DrawBox(lMin, lMax, lTransform, rw::RGBA(0, 0, 0, 255));
            }
            break;

            default:
            {
                CGS_ASSERT(false, "Collision volume very confused about what type it is... corrupted?");
            }
            break;
        }
    }

    // @ 0x822DCBF8. For every prop in every loaded zone, within the volume-render distance:
    // draw its per-instance label, a "Mass:" (+conditional "Lean angle:") text stat, each of
    // its whole-prop collision volumes (or, when the prop has broken into parts, the per-part
    // volumes) composed with the volume's own relative transform, and a bounding hollow sphere.
    void PropEntityDebugComponent::RenderProps(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        const Vector3& lrCamera = lpDisplay->GetCameraPosition();
        const f32 lfMaxDistSq = KF_MAX_DIST_FOR_VOLUME_RENDERING * KF_MAX_DIST_FOR_VOLUME_RENDERING;
        const BrnPhysics::Props::PropPhysicsDataHeader* lpTypes = mpPropEntityModule->GetPropPhysicsDataHeader();
        PropZoneManager& lrZoneManager = mpPropEntityModule->mZoneManager;

        for (u16 lu16ZoneId = 0; lu16ZoneId < KU_MAX_ZONES; ++lu16ZoneId)
        {
            CGS_ASSERT(lu16ZoneId < KU_MAX_ZONES, "luZoneIndex < BrnPhysics::Props::KU_MAX_ZONES");

            const u32 luPropsInZone = lrZoneManager.GetNumberOfPropsInZone(lu16ZoneId);
            for (u32 luPropIndex = 0; luPropIndex < luPropsInZone; ++luPropIndex)
            {
                PropEntityInstance* lpProp = lrZoneManager.GetProp(lu16ZoneId, luPropIndex);

                const Matrix44Affine& lrTransform = lpProp->GetWorldTransform();
                if (MagnitudeSquared(lrCamera, lrTransform.Pos()) > lfMaxDistSq)
                {
                    continue;
                }

                // The prop's per-instance label: its content id, or "generated_instance".
                const u32 luInstanceId = lpProp->GetInstanceID();
                char lacLabel[128];
                if (luInstanceId != 0)
                {
                    CgsCore::SPrintf(lacLabel, sizeof(lacLabel), "ID: %u", luInstanceId);
                }
                const Vector3 lLabelPos = lrTransform.Pos();
                lpDisplay->DrawText(MakeVector3(lLabelPos.x, lLabelPos.y + 2.0f, lLabelPos.z),
                                    (luInstanceId != 0) ? lacLabel : "generated_instance", 16.0f);

                CGS_ASSERT(lpProp->GetState() < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
                const bool lbBrokenIntoParts = (lpProp->GetState() >= E_SMASHED);

                const u32 luTypeId = lpProp->GetTypeId();
                CGS_ASSERT(luTypeId < BrnPhysics::Props::KU_MAX_PROP_TYPES, "liTypeId < KU_MAX_PROP_TYPES");
                CGS_ASSERT(luTypeId < lpTypes->GetNumberOfPropTypes(), "liTypeId < muNumberOfPropTypes");
                const BrnPhysics::Props::PropTypeData* lpType = lpTypes->GetType(luTypeId);

                if (lbBrokenIntoParts)
                {
                    // The prop has broken up: draw the per-part collision volumes (under each
                    // part's world transform) and a per-part "Mass:" stat + bounding sphere.
                    const u32 luNumberOfParts = lpType->GetNumberOfParts();
                    for (u32 luPartIndex = 0; luPartIndex < luNumberOfParts; ++luPartIndex)
                    {
                        const BrnPhysics::Props::PropPartVolumeGroup& lrPartGroup =
                            lpType->GetPartVolumeGroups()[luPartIndex];

                        PropPartEntityInstance* lpPart =
                            lrZoneManager.GetPart(lu16ZoneId, static_cast<u16>(luPropIndex), static_cast<u16>(luPartIndex));
                        CGS_ASSERT(IsValid(lpPart->mWorldTransform),
                                   "RwMath::IsValid(lpPartInstance->mWorldTransform)");
                        const Matrix44Affine& lrPartTransform = lpPart->mWorldTransform;

                        // Per-part mass stat, drawn at text size 10.0.
                        CgsDev::SimpleStrStream lStatStream;
                        lStatStream.Reset();
                        lStatStream << "Mass: " << lrPartGroup.GetMass();
                        lpDisplay->DrawText(lrPartTransform.Pos(), lStatStream.GetBuffer(), 10.0f);

                        // Each part volume, composed with its own relative transform.
                        const u32 luPartVolumes = lrPartGroup.GetNumberOfVolumes();
                        for (u32 luVolumeIndex = 0; luVolumeIndex < luPartVolumes; ++luVolumeIndex)
                        {
                            rw::collision::Volume* lpVolume = lrPartGroup.GetCollisionVolume(luVolumeIndex);
                            Draw(lpVolume, lpDisplay, lpVolume->GetRelativeTransform() * lrPartTransform);
                        }

                        lpDisplay->DrawHollowSphere(lrPartTransform.Pos(), lrPartGroup.GetBoundingRadius(),
                                                    rw::RGBA(255, 255, 255, 255));
                    }
                }
                else
                {
                    // Intact prop: a "Mass:" (+conditional "Lean angle:") stat, then the whole-
                    // prop volumes composed with their relative transforms, then a bounding sphere.
                    CGS_ASSERT(IsValid(lrTransform), "RwMath::IsValid(lpPropInstance->GetWorldTransform())");

                    CgsDev::SimpleStrStream lStatStream;
                    lStatStream.Reset();
                    lStatStream << "Mass: " << lpType->GetMass();
                    lStatStream << "\n";

                    // The lean-angle stat is emitted only for the leaning states (1 / 2).
                    const u8 lu8LeanState = lpType->GetLeanState();
                    if (lu8LeanState == 1 || lu8LeanState == 2)
                    {
                        lStatStream << "Lean angle: " << acosf(lpType->GetLeanCosAngle());
                        lpDisplay->DrawHollowSphere(lrTransform.Pos(), 1.0f, rw::RGBA(255, 255, 255, 255));
                    }
                    lpDisplay->DrawText(lrTransform.Pos(), lStatStream.GetBuffer(), 10.0f);

                    const u32 luNumberOfVolumes = lpType->GetNumberOfVolumes();
                    for (u32 luVolumeIndex = 0; luVolumeIndex < luNumberOfVolumes; ++luVolumeIndex)
                    {
                        rw::collision::Volume* lpVolume = lpType->GetCollisionVolume(luVolumeIndex);
                        Draw(lpVolume, lpDisplay, lpVolume->GetRelativeTransform() * lrTransform);
                    }

                    lpDisplay->DrawHollowSphere(lrTransform.Pos(), lpType->GetBoundingRadius(),
                                                rw::RGBA(255, 255, 255, 255));
                }
            }
        }
    }

    // @ 0x822DDAE8. For every prop within the (closer) stat-render distance, draw a multi-line
    // stat read-out at the prop origin. An intact prop draws one block (entity id / index /
    // instance id / type); a broken prop (state >= E_SMASHED) draws one "entity id / index"
    // block per part instead. All read-outs are drawn at text size 12.0.
    void PropEntityDebugComponent::RenderPropStats(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        static const u32 KU_STRING_BUFFER_SIZE = 1000;

        const Vector3& lrCamera = lpDisplay->GetCameraPosition();
        const f32 lfMaxDistSq = KF_MAX_DIST_FOR_STAT_RENDERING * KF_MAX_DIST_FOR_STAT_RENDERING;
        const BrnPhysics::Props::PropPhysicsDataHeader* lpTypes = mpPropEntityModule->GetPropPhysicsDataHeader();
        PropZoneManager& lrZoneManager = mpPropEntityModule->mZoneManager;

        for (u16 lu16ZoneId = 0; lu16ZoneId < KU_MAX_ZONES; ++lu16ZoneId)
        {
            CGS_ASSERT(lu16ZoneId < KU_MAX_ZONES, "luZoneIndex < BrnPhysics::Props::KU_MAX_ZONES");

            const u32 luPropsInZone = lrZoneManager.GetNumberOfPropsInZone(lu16ZoneId);
            for (u32 luPropIndex = 0; luPropIndex < luPropsInZone; ++luPropIndex)
            {
                PropEntityInstance* lpProp = lrZoneManager.GetProp(lu16ZoneId, luPropIndex);
                if (lpProp == nullptr)
                {
                    continue;
                }

                const Matrix44Affine& lrTransform = lpProp->GetWorldTransform();
                if (MagnitudeSquared(lrCamera, lrTransform.Pos()) > lfMaxDistSq)
                {
                    continue;
                }

                const u32 luTypeId = lpProp->GetTypeId();
                CGS_ASSERT(luTypeId < BrnPhysics::Props::KU_MAX_PROP_TYPES, "liTypeId < KU_MAX_PROP_TYPES");
                CGS_ASSERT(luTypeId < lpTypes->GetNumberOfPropTypes(), "liTypeId < muNumberOfPropTypes");
                const BrnPhysics::Props::PropTypeData* lpType = lpTypes->GetType(luTypeId);

                CGS_ASSERT(IsValid(lrTransform), "RwMath::IsValid(lpPropInstance->GetWorldTransform())");
                CGS_ASSERT(lpProp->GetState() < E_STATE_COUNT, "mu8State < E_STATE_COUNT");

                // The prop's global instance index (zone start slot + index-within-zone); the
                // packed entity id is index<<10 | owner<<24 (| part for the broken path).
                const u32 luGlobalPropIndex = lrZoneManager.mauStartIndexOfZone[lu16ZoneId] + luPropIndex;

                if (lpProp->GetState() >= E_SMASHED)
                {
                    // Broken prop: one "Entity id / Index" stat per part.
                    const u32 luNumberOfParts = lpType->GetNumberOfParts();
                    for (u32 luPartIndex = 0; luPartIndex < luNumberOfParts; ++luPartIndex)
                    {
                        PropPartEntityInstance* lpPart =
                            lrZoneManager.GetPart(lu16ZoneId, static_cast<u16>(luPropIndex), static_cast<u16>(luPartIndex));

                        CGS_ASSERT(luGlobalPropIndex < (1u << PropEntityID::KU_NUM_BITS_FOR_ENTITY_NUM),
                                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
                        CGS_ASSERT(luPartIndex < (1u << PropEntityID::KU_NUM_BITS_FOR_PART_NUM),
                                   "luPartIndex < (1U << KU_NUM_BITS_FOR_PART_NUM)");
                        const u32 luEntityId =
                            (luGlobalPropIndex << PropEntityID::KU_ENTITY_INDEX_BASE)
                          | (static_cast<u32>(E_ENTITYTYPE_PROP) << PropEntityID::KU_OWNER_BASE)
                          | luPartIndex;
                        CGS_ASSERT(((luEntityId >> 24) & 0xFFu) == E_ENTITYTYPE_PROP,
                                   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");

                        char lacStringBuffer[KU_STRING_BUFFER_SIZE];
                        CgsDev::StrStream lOutputStream(lacStringBuffer, KU_STRING_BUFFER_SIZE);
                        lOutputStream.Reset();
                        lOutputStream << "Entity id: " << CgsDev::E_PRINTMODE_HEXONCE << luEntityId;
                        lOutputStream << "\n";
                        CGS_ASSERT(strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE,
                                   "strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE");

                        lpDisplay->DrawText(lpPart->mWorldTransform.Pos(), lOutputStream.GetBuffer(), 12.0f);
                    }
                }
                else
                {
                    // Intact prop: one block of entity id / index / instance id / type.
                    CGS_ASSERT(luGlobalPropIndex < (1u << PropEntityID::KU_NUM_BITS_FOR_ENTITY_NUM),
                               "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
                    const u32 luEntityId =
                        (luGlobalPropIndex << PropEntityID::KU_ENTITY_INDEX_BASE)
                      | (static_cast<u32>(E_ENTITYTYPE_PROP) << PropEntityID::KU_OWNER_BASE);
                    CGS_ASSERT(((luEntityId >> 24) & 0xFFu) == E_ENTITYTYPE_PROP,
                               "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");

                    char lacStringBuffer[KU_STRING_BUFFER_SIZE];
                    CgsDev::StrStream lOutputStream(lacStringBuffer, KU_STRING_BUFFER_SIZE);
                    lOutputStream.Reset();
                    const u32 luEntityIndex =
                        (luEntityId >> PropEntityID::KU_ENTITY_INDEX_BASE)
                      & ((1u << PropEntityID::KU_NUM_BITS_FOR_ENTITY_NUM) - 1u);   // (id>>10)&0x3FFF
                    lOutputStream << "Entity id: "    << CgsDev::E_PRINTMODE_HEXONCE << luEntityId;
                    lOutputStream << "\nIndex: "       << luEntityIndex;
                    lOutputStream << "\nInstance id: " << lpProp->GetInstanceID();
                    lOutputStream << "\nType: "        << luTypeId;
                    CGS_ASSERT(strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE,
                               "strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE");

                    lpDisplay->DrawText(lrTransform.Pos(), lOutputStream.GetBuffer(), 12.0f);
                }
            }
        }
    }

    // @ 0x822DC520. For every prop within the volume-render distance and not yet smashed,
    // draw the inertia box of each of its (or its parts') collision volumes.
    void PropEntityDebugComponent::RenderInertiaBoxes(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        const Vector3& lrCamera = lpDisplay->GetCameraPosition();
        const f32 lfMaxDistSq = KF_MAX_DIST_FOR_VOLUME_RENDERING * KF_MAX_DIST_FOR_VOLUME_RENDERING;
        const BrnPhysics::Props::PropPhysicsDataHeader* lpTypes = mpPropEntityModule->GetPropPhysicsDataHeader();

        for (u16 lu16ZoneId = 0; lu16ZoneId < KU_MAX_ZONES; ++lu16ZoneId)
        {
            CGS_ASSERT(lu16ZoneId < KU_MAX_ZONES, "luZoneIndex < BrnPhysics::Props::KU_MAX_ZONES");

            const u32 luPropsInZone = mpPropEntityModule->mZoneManager.GetNumberOfPropsInZone(lu16ZoneId);
            for (u32 luPropIndex = 0; luPropIndex < luPropsInZone; ++luPropIndex)
            {
                PropEntityInstance* lpProp = mpPropEntityModule->mZoneManager.GetProp(lu16ZoneId, luPropIndex);

                const Matrix44Affine& lrTransform = lpProp->GetWorldTransform();
                if (MagnitudeSquared(lrCamera, lrTransform.Pos()) > lfMaxDistSq)
                {
                    continue;
                }

                CGS_ASSERT(lpProp->GetState() < E_STATE_COUNT, "mu8State < E_STATE_COUNT");
                const bool lbBrokenIntoParts = (lpProp->GetState() >= E_SMASHED);

                const u32 luTypeId = lpProp->GetTypeId();
                CGS_ASSERT(luTypeId < BrnPhysics::Props::KU_MAX_PROP_TYPES, "liTypeId < KU_MAX_PROP_TYPES");
                CGS_ASSERT(luTypeId < lpTypes->GetNumberOfPropTypes(), "liTypeId < muNumberOfPropTypes");
                const BrnPhysics::Props::PropTypeData* lpType = lpTypes->GetType(luTypeId);

                if (lbBrokenIntoParts)
                {
                    const u32 luNumberOfParts = lpType->GetNumberOfParts();
                    for (u32 luPartIndex = 0; luPartIndex < luNumberOfParts; ++luPartIndex)
                    {
                        PropPartEntityInstance* lpPart =
                            mpPropEntityModule->mZoneManager.GetPart(lu16ZoneId, static_cast<u16>(luPropIndex), static_cast<u16>(luPartIndex));
                        CGS_ASSERT(IsValid(lpPart->mWorldTransform),
                                   "RwMath::IsValid(lpPartInstance->mWorldTransform)");

                        const u32 luPartVolumes = lpType->GetPartVolumeGroups()[luPartIndex].GetNumberOfVolumes();
                        for (u32 luVolumeIndex = 0; luVolumeIndex < luPartVolumes; ++luVolumeIndex)
                        {
                            rw::collision::Volume* lpVolume = lpType->GetCollisionVolume(luVolumeIndex);
                            RenderInertiaBox(lpDisplay, lpPart->mWorldTransform,
                                             lpVolume->GetExtent(), lBroadcast(lpVolume->GetRadius()));
                        }
                    }
                }
                else
                {
                    const u32 luNumberOfVolumes = lpType->GetNumberOfVolumes();
                    for (u32 luVolumeIndex = 0; luVolumeIndex < luNumberOfVolumes; ++luVolumeIndex)
                    {
                        rw::collision::Volume* lpVolume = lpType->GetCollisionVolume(luVolumeIndex);
                        RenderInertiaBox(lpDisplay, lrTransform, lpVolume->GetExtent(), lBroadcast(lpVolume->GetRadius()));
                    }
                }
            }
        }
    }

    // @ 0x822C54C0. Draw ONE inertia box for a collision volume: from the volume's box extent
    // (lCentre) and a broadcast scale (lfExtent = the volume radius/mass), compute the three
    // principal half-extents of the equivalent solid-box inertia tensor and draw that box (white)
    // under the prop/part world transform. Called per-volume by RenderInertiaBoxes.
    //
    // The X360 body is a pure VMX cascade: it forms a Newton-refined reciprocal of the extent,
    // scales it by lfExtent, then for each axis takes sqrt(K * (sum of the other two lane terms -
    // this lane term)) -- the standard box inertia-diagonal shape (Ixx ~ y^2+z^2, etc.) inverted
    // back into an equivalent-box half-extent -- zero-guarding each sqrt, assembles the three into
    // the box max corner, negates it (sign-bit XOR) for the min corner, and calls
    // DrawBox(min, max, lTransform, white).
    //
    // FLAG (rodata): the per-axis scale/normalisation vector unk_82FAD600 (asm 0x822C54F4:
    // `lvx128 v6, r0, r11` from unk_82FAD600) is an un-homed .rdata constant absent from the
    // exports. The reciprocal/normalize/sqrt STRUCTURE, the zero guards, the min = -max, and the
    // white DrawBox call are reconstructed EXACTLY; the numeric scale (KF_INERTIA_SCALE) stays 0
    // until the constant is recovered, so the drawn extents are honest-inert. NEVER fabricated.
    void PropEntityDebugComponent::RenderInertiaBox(CgsDev::Debug3DImmediateRender* lpDisplay,
                                                    Matrix44Affine lTransform, Vector3 lCentre,
                                                    VecFloat lfExtent)
    {
        // FLAG: un-homed .rdata scale vector unk_82FAD600 @ 0x822C54F4 -> honest flagged-0 placeholder.
        static const f32 KF_INERTIA_SCALE = 0.0f;

        // Newton-refined reciprocal of the box extent (vrefp v11 + two vnmsubfp/vmaddfp steps),
        // scaled by the broadcast lfExtent (vmulfp128 v11, v11, v1).
        const f32 lfScale = lfExtent.x;
        const f32 lfRx = (lCentre.x != 0.0f) ? (lfScale / lCentre.x) : 0.0f;
        const f32 lfRy = (lCentre.y != 0.0f) ? (lfScale / lCentre.y) : 0.0f;
        const f32 lfRz = (lCentre.z != 0.0f) ? (lfScale / lCentre.z) : 0.0f;

        // Per-axis inertia-diagonal combination (sum of the OTHER two lanes minus this lane),
        // each scaled by the un-homed constant, then sqrt (vrsqrtefp + Newton), zero-guarded.
        const f32 lfTermX = (lfRy + lfRz - lfRx) * KF_INERTIA_SCALE;   // v11 = y+z-x
        const f32 lfTermY = (lfRz + lfRx - lfRy) * KF_INERTIA_SCALE;   // v10 = z+x-y
        const f32 lfTermZ = (lfRx + lfRy - lfRz) * KF_INERTIA_SCALE;   // v9  = x+y-z

        const f32 lfHx = (lfTermX > 0.0f) ? std::sqrt(lfTermX) : 0.0f;  // vcmpeqfp/vsel zero-guard
        const f32 lfHy = (lfTermY > 0.0f) ? std::sqrt(lfTermY) : 0.0f;
        const f32 lfHz = (lfTermZ > 0.0f) ? std::sqrt(lfTermZ) : 0.0f;

        // Box max = the three half-extents (vrlimi128 assembly), min = -max (vslw sign-mask XOR).
        const Vector3 lMax = MakeVector3( lfHx,  lfHy,  lfHz);
        const Vector3 lMin = MakeVector3(-lfHx, -lfHy, -lfHz);

        // DrawBox(lpDisplay, min, max, lTransform, white). Colour arg == -1 == ARGB 0xFFFFFFFF.
        lpDisplay->DrawBox(lMin, lMax, lTransform, rw::RGBA(255, 255, 255, 255));
    }

    // @ 0x822DD868. For every prop whose type is flagged as a traffic light, within the
    // render distance, draw an "ID: %u" marker at the prop.
    void PropEntityDebugComponent::RenderTrafficLights(CgsDev::Debug3DImmediateRender* lpDisplay)
    {
        const Vector3& lrCamera = lpDisplay->GetCameraPosition();
        const f32 lfMaxDistSq = KF_MAX_DIST_FOR_VOLUME_RENDERING * KF_MAX_DIST_FOR_VOLUME_RENDERING;
        const BrnPhysics::Props::PropPhysicsDataHeader* lpTypes = mpPropEntityModule->GetPropPhysicsDataHeader();

        for (u16 lu16ZoneId = 0; lu16ZoneId < KU_MAX_ZONES; ++lu16ZoneId)
        {
            CGS_ASSERT(lu16ZoneId < KU_MAX_ZONES, "luZoneIndex < BrnPhysics::Props::KU_MAX_ZONES");

            const u32 luPropsInZone = mpPropEntityModule->mZoneManager.GetNumberOfPropsInZone(lu16ZoneId);
            for (u32 luPropIndex = 0; luPropIndex < luPropsInZone; ++luPropIndex)
            {
                PropEntityInstance* lpProp = mpPropEntityModule->mZoneManager.GetProp(lu16ZoneId, luPropIndex);

                const u32 luTypeId = lpProp->GetTypeId();
                CGS_ASSERT(luTypeId < BrnPhysics::Props::KU_MAX_PROP_TYPES, "liTypeId < KU_MAX_PROP_TYPES");
                CGS_ASSERT(luTypeId < lpTypes->GetNumberOfPropTypes(), "liTypeId < muNumberOfPropTypes");
                const BrnPhysics::Props::PropTypeData* lpType = lpTypes->GetType(luTypeId);

                if (!lpType->IsTrafficLight())
                {
                    continue;
                }

                const Matrix44Affine& lrTransform = lpProp->GetWorldTransform();
                if (MagnitudeSquared(lrCamera, lrTransform.Pos()) > lfMaxDistSq)
                {
                    continue;
                }

                // The traffic-light marker: its content id, or "generated_instance" when the
                // prop has no instance id (X360 8547-8579: the SPrintf is taken only when
                // *(prop+64) != 0, else the literal label is drawn).
                const u32 luInstanceId = lpProp->GetInstanceID();
                char lacLabel[128];
                if (luInstanceId != 0)
                {
                    CgsCore::SPrintf(lacLabel, sizeof(lacLabel), "ID: %u", luInstanceId);
                }
                const Vector3 lLabelPos = lrTransform.Pos();
                lpDisplay->DrawText(MakeVector3(lLabelPos.x, lLabelPos.y + 1.0f, lLabelPos.z),
                                    (luInstanceId != 0) ? lacLabel : "generated_instance", 16.0f);
            }
        }
    }

    // @ 0x822DE3F8. The 2D module-wide stat panel: the used-prop / used-part pool counts, the
    // loaded-zone count, and a per-zone table of (zone id, #props, #parts). Refreshed on a slow
    // 30-frame cadence so the read-out stays legible -- the X360 recomputes muUsedProps (a1[18])
    // on frame 10 by POPCOUNTing the maUsedProps slot bitset, muUsedParts (a1[19]) on frame 20
    // from the maUsedParts bitset, and wraps the frame counter at 30; every frame it prints the
    // cached counts. (There is NO "used instances" field in the binary.)
    void PropEntityDebugComponent::RenderModuleStats(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        static const u32 KU_STRING_BUFFER_SIZE = 1000;
        static const f32 KF_TEXT_SIZE = 15.0f;

        typedef CgsContainers::BitArray<KU_NUM_ZONE_SLOTS> ZoneSlotBitArray;

        PropZoneManager& lrZoneManager = mpPropEntityModule->mZoneManager;

        // Slow-cadence refresh (asm 0x822DE440-0x822DE9A8): only one counter is recomputed per
        // cadence tick, and each is the popcount of its slot-usage bitset, NOT a sum of the
        // per-zone counts. The frame counter is bumped first, then keyed: 10 -> used props,
        // 20 -> used parts, 30 -> wrap to 0.
        const u32 luFrame = ++muFrameCountForUpdate;
        if (luFrame == 10)
        {
            muUsedProps = 0;
            for (s32 liSlot = lrZoneManager.maUsedProps.GetFirstNonZeroBit();
                 liSlot != ZoneSlotBitArray::KI_INVALID_BITINDEX;
                 liSlot = lrZoneManager.maUsedProps.GetNextNonZeroBit(liSlot))
            {
                ++muUsedProps;
            }
        }
        else if (luFrame == 20)
        {
            muUsedParts = 0;
            for (s32 liSlot = lrZoneManager.maUsedParts.GetFirstNonZeroBit();
                 liSlot != ZoneSlotBitArray::KI_INVALID_BITINDEX;
                 liSlot = lrZoneManager.maUsedParts.GetNextNonZeroBit(liSlot))
            {
                ++muUsedParts;
            }
        }
        if (muFrameCountForUpdate == 30)
        {
            muFrameCountForUpdate = 0;
        }

        char lacStringBuffer[KU_STRING_BUFFER_SIZE];
        CgsDev::StrStream lOutputStream(lacStringBuffer, KU_STRING_BUFFER_SIZE);

        // --- the pool-usage / loaded-zone summary, drawn at (500, 80) ---
        lOutputStream << " Used props: "   << muUsedProps;
        lOutputStream << " Used parts: "   << muUsedParts;
        lOutputStream << " Zones loaded: " << mpPropEntityModule->muNumberOfLoadedZones;
        lpDisplay->DrawText(lOutputStream.GetBuffer(), 500.0f, 80.0f, KF_TEXT_SIZE, 0xFFFFFFFFu);
        CGS_ASSERT(strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE,
                   "strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE");
        lOutputStream.Reset();
        lpDisplay->DrawText(lOutputStream.GetBuffer(), 500.0f, 80.0f, KF_TEXT_SIZE, 0xFFFFFFFFu);

        // --- the per-zone table header, drawn at (500, 100) ---
        lOutputStream.Reset();
        lOutputStream << "              Zone ID    Number of Props    Number of Parts \n";
        lOutputStream << "Loaded Zones:    ";
        lpDisplay->DrawText(lOutputStream.GetBuffer(), 500.0f, 100.0f, KF_TEXT_SIZE, 0xFFFFFFFFu);
        CGS_ASSERT(strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE,
                   "strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE");
        lOutputStream.Reset();

        // --- one row per zone that has props or parts; player-heavy zones (>300 props) in
        //     blue, all others white. Drawn one line per zone starting at (500, 90). ---
        f32 lfRowY = 90.0f;
        for (u32 luZoneId = 0; luZoneId < KU_MAX_ZONES; ++luZoneId)
        {
            CGS_ASSERT(luZoneId < KU_MAX_ZONES, "luZoneIndex < BrnPhysics::Props::KU_MAX_ZONES");

            const u32 luProps = lrZoneManager.GetNumberOfPropsInZone(static_cast<u16>(luZoneId));
            const u32 luParts = lrZoneManager.mauNumberOfPartsInZone[luZoneId];
            if (luProps != 0 || luParts != 0)
            {
                static_cast<CgsDev::StrStreamBase&>(lOutputStream)
                    << luZoneId
                    << "             "
                    << luProps
                    << "                    "
                    << luParts
                    << "\n"
                    << "                 ";

                const u32 luColour = (luProps > 300u) ? 0xFF0000FFu : 0xFFFFFFFFu;
                lpDisplay->DrawText(lOutputStream.GetBuffer(), 500.0f, lfRowY, 16.0f, luColour);
                CGS_ASSERT(strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE,
                           "strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE");
                lOutputStream.Reset();
                lfRowY += 16.0f;
            }
        }
        CGS_ASSERT(strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE,
                   "strlen(lOutputStream.GetBuffer()) < KU_STRING_BUFFER_SIZE");
    }

    // @ 0x822EFF48. The 2D top-down cell-grid map centred on the player's prop zone.
    //
    // SCOPE NOTE: the X360 body additionally walks the player zone's loaded-cell bitset
    // (PropCellManager / PropZoneData internals at deep manager offsets) drawing each cell as
    // a quad -- solid via DrawSolidConvexPolygon when the cell is active, wire via
    // DrawWirePolygon otherwise -- and rings every occupied prop cell with a marker. That walk
    // depends on the (not-yet-reconstructed) PropCellManager cell-grid layout and the
    // out-of-line ToCellGridScreenCoords projector, so it is summarised here. What IS pinned
    // from the asm is reconstructed faithfully: the projection-cache setup (map scale =
    // 1000/mfCellGridZoom, written to mMapDimensions / mScreenSize / mfAspect) the projector
    // reads, and the player-cell origin marker (blue circle) drawn last.
    void PropEntityDebugComponent::RenderCellGrid(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        // map scale = 1000 / zoom (X360 fdivs flt_82009E10(=1000.0) / mfCellGridZoom @ +0x40).
        const f32 lfMapScale = 1000.0f / mfCellGridZoom;
        mMapDimensions.x = lfMapScale;
        mMapDimensions.y = lfMapScale;
        mScreenSize.x = mMapDimensions.x;
        mScreenSize.y = mMapDimensions.y;
        mfAspect = (mScreenSize.y != 0.0f) ? (mScreenSize.x / mScreenSize.y) : 1.0f;

        // The player's current cell origin is ringed with a blue marker (X360: the final
        // DrawCircle, 5 segments, ARGB 0xFF0000FF, after projecting the player cell world
        // position through ToCellGridScreenCoords). The player cell position is read from the
        // owning module's player-tracking state (manager +864800), reached out-of-line.
        //
        // FLAG: the marker radius is the .rdata float flt_820149B4 (asm 0x822F0550:
        // `lfs f1, flt_820149B4@l(r10)`). That constant is not present in the IDA exports
        // (only the symbol reference is dumped, not its value), so the radius below is a
        // PLACEHOLDER -- see still_open. Modelled as a named const at the asm address.
        const f32 KF_PLAYER_CELL_MARKER_RADIUS = 5.0f;  // FLAG: flt_820149B4 @ 0x822F0550 (value un-dumped)
        const Vector2 lOrigin = ToCellGridScreenCoords(MakeVector3(0.0f, 0.0f, 0.0f));
        lpDisplay->DrawCircle(lOrigin, KF_PLAYER_CELL_MARKER_RADIUS, 5, 0xFF0000FFu);  // ARGB 0xFF0000FF (blue)
    }

    // @ 0x822A9758 -- the "Reset props" debug-menu action callback (registered in OnActivate
    // as RegisterFunction(&ResetProps, this, ...); the DebugUI invokes it with lpUserData == the
    // component `this`). It flips the owning module's streaming state to E_RESET_UNLOADING so the
    // module unloads + reloads every prop zone next frame.
    //
    // X360 body (single store, no branch):
    //   lwz   r11, 0x34(r3)          ; lpUserData->mpPropEntityModule
    //   li    r9, 2                  ; E_RESET_UNLOADING
    //   stwx  r9, r11, 0xD3200       ; module->meStreamingMode = E_RESET_UNLOADING
    void PropEntityDebugComponent::ResetProps(void* lpUserData)
    {
        PropEntityDebugComponent* lpComponent = static_cast<PropEntityDebugComponent*>(lpUserData);
        lpComponent->mpPropEntityModule->meStreamingMode = PropEntityModule::E_RESET_UNLOADING;
    }
}
