// BrnPhysics::Deformation::DeformationDebugComponent -- bodies for the deformation-rig debug
// visualiser/editor, reconstructed from BURNOUT_X360_ARTIST.XEX (offset + branch authority) with
// class shape / names from the DecFIGS DWARF (BrnDeformationDebugComponent.h:131/149).
//
//   GetExtent                       @ 0x825DEC50  (DeformationAxisAlignedBox)
//   Construct                       @ 0x82606480
//   Update                          @ 0x82623178
//   RenderWorld                     @ 0x82606548
//   OnActivate                      @ 0x82623198
//   CompressSelectedRig_MaxDrivetime@ 0x82607160
//   DrawDetachedWheels              @ 0x825DED20
//   DetachPart                      @ 0x825B97F8
//   OnCompressionChange             @ 0x825DF2E0
//   OnCompressionPresetChange       @ 0x826073A8
//   OnSelectedRigChange             @ 0x825DF408
//   OnSelectedSensorChange          @ 0x825DF310
//   OnSelectedPointChange           @ 0x825DF688
//   OnPointPositionChange           @ 0x825B98F8
//
// The component is the file-scope static the DeformationManager constructs/registers. It derives from
// CgsDev::DebugComponent; the trailing folded `BaseCollisionGenerator::Destruct(this)` the X360 emits
// in Construct/OnActivate is the base CgsDev::DebugComponent::Construct() (the X360 image folds trivial
// bodies onto one shared address -- same convention as CgsLanguageManagerDebugComponent.cpp).
//
// The change/select/function callbacks are registered with the debug menu as plain C function pointers
// (VariableCallbackFunction = void(*)(void*,void*) / DebugCallbackFunction = void(*)(void*)); the X360
// passes the component as the user-data argument, which each callback casts back to a component.

#include "GameSource/Physics/DeformationManager/BrnDeformationDebugComponent.h"
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"                          // DeformationManager
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"         // DeformableObject + debug accessors
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"        // DeformationSensor
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"  // StreamedDeformationSpec
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"               // IKBodyPart
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnTagPoint.h"                 // TagPoint
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKDrivenPoint.h"            // IKDrivenPoint
#include "SharedClasses/Physics/Deformation/BrnIKBodyPartSpec.h"                                  // IKBodyPartSpec
#include "SharedClasses/Physics/Deformation/BrnTagPointSpec.h"                                    // TagPointSpec (detach threshold / init pos)
#include "SharedClasses/Physics/Deformation/BrnDeformationJointSpec.h"                            // DeformationJointSpec (DrawAxis basis)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"                     // VehiclePhysics, Wheel, EVehicleDrivenWheel
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"                              // Wheel (mu8State / mPosition)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"      // Debug3DImmediateRender draw API
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"                       // CgsGeometric::AxisAlignedBox
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                               // CgsGeometric::Sphere (GetLocalSphereCentre)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                                // CGS_ASSERT
#include "rw/rwcore_structs.h"                                                                    // rw::RGBA
#include "rw/math/vpu/matrix44affine_operation.h"                                                 // OrthoNormalize3x3, TransformVector/Point
#include "rw/math/vpu/vector3_operation.h"                                                        // Cross, Subtract, MagnitudeSquared

#include <cmath>   // std::fabs

namespace BrnPhysics
{
namespace Deformation
{
    // Out-of-line bodies for two accessors declared (but not defined) in headers that keep their operand
    // types opaque/forward-declared. Bodied here, where the full layouts are in scope.

    // The sensor's local-space sphere is the opaque BrnPhysics::Deformation::Sphere (forward-declared in
    // BrnDeformationSensor.h -- its real home is the collision code). Its leading 16 bytes are the
    // centre.xyz + radius.w Vector4 (documented in CgsSphere.h / the sensor header). Read that leading
    // Vector4 via the homed CgsGeometric::Sphere shape -- the only allowed cross-type view of an
    // un-homed object whose leading layout is known (same pattern as the sensor's GetMaxSensorImpulse).
    const Vector4& DeformationSensor::GetLocalSphereCentre() const
    {
        return reinterpret_cast<const CgsGeometric::Sphere*>( GetLocalSpaceSphere() )->mPositionRadius;
    }

    // The manager's model-slot accessor (declared-only in BrnDeformationManager.h, which keeps
    // DeformableObject forward-declared). &mpaModels[liIndex] from the pool base.
    DeformableObject* DeformationManager::GetDeformableObject(s32 liIndex)
    {
        return &mpaModels[liIndex];
    }

    // =============================================================================================
    // Module-scope tuning globals (the debug menu registers / mutates these by &address). They are the
    // real game-tuning rodata, owned by the deformation-physics TUs (BrnDeformableObject_*.cpp). The
    // X360 OnActivate registers their addresses with the debug UI, so they MUST be the same objects the
    // physics reads -- declared extern here and resolved at link. NONE are fabricated values; the
    // component only registers/edits them.
    // =============================================================================================
    extern f32 kfNormalImpulseScale;
    extern f32 kfFrictionImpulseScale;
    extern f32 kfPartLinearDrag;
    extern f32 kfPartAngularDrag;
    extern f32 kfPartMaxLinearVelocity;
    extern f32 kfPartMaxAngularVelocity;
    extern f32 kfPartMass;
    extern f32 kfPartInertiaMultiplier;
    extern f32 kfPartExtraGravity;
    extern f32 kfPartDynamicFriction;
    extern f32 kfPartStaticFriction;
    extern f32 kfJointPenetrationMultiplier;
    extern f32 kfJointForceMultiplier;
    extern f32 kfAngularVelocityForDetachment;
    extern f32 kfAngularVelocityDecay;

    extern bool kbAllowDeformationDebug;
    extern bool kbAllowRandomPartDetachment;
    extern bool kbAllowDriveTimeDeformation;
    extern bool kbSlowMoDeformation;

    // The "render only this car part" selector (asm dword_82CDB49C); -1 == render all parts. Edited by
    // the "Render car part" debug variable; consulted by RenderWorld / DetachPart.
    extern s32 giDebugOnlyRenderThisCarPart;

    namespace
    {
        // ---- debug-menu group / variable names (X360 .rdata literals) ----------------------------
        const char* const KPC_GROUP_COMPRESS = "Compress..";
        const char* const KPC_GROUP_RENDER   = "Rendering..";
        const char* const KPC_GROUP_SENSORS  = "Sensors & points..";
        const char* const KPC_GROUP_IMPULSE  = "Impulse scale..";
        const char* const KPC_GROUP_PARTS    = "Parts..";
        const char* const KPC_GROUP_FLAGS    = "Flags..";
        const char* const KPC_GROUP_DETACH   = "Detachment..";

        // ---- compression-slider tuning (X360 .rdata) ---------------------------------------------
        const f32 KF_COMPRESS_STEP   = 0.025f;        // flt_8209AE88
        const f32 KF_COMPRESS_MIN    = 0.0f;          // flt_82001CC0
        const f32 KF_COMPRESS_MAX    = 1.0f;          // flt_82001C98
        const f32 KF_POINT_STEP      = 0.05f;         // flt_820047C8
        const f32 KF_SENSOR_STEP     = 0.1f;          // flt_82004014
        const f32 KF_IMPULSE_STEP    = 0.001f;        // flt_82013F90
        const f32 KF_DECAY_STEP      = 0.01f;         // flt_82002138

        // ---- selector ranges (X360 immediates) ---------------------------------------------------
        const s32 KI_MAX_RIG_INDEX     = 27;          // sub_8282F598(rig, 0, 27)
        const s32 KI_MAX_SENSOR_INDEX  = 19;          // (selected sensor, 0..19)
        const s32 KI_MAX_POINT_INDEX   = 126;         // (selected point, 0..126)
        const s32 KI_NUM_COMPRESS_PRESETS = 2;        // SetRange(0, 2) on the preset selector

        // ---- compression presets ----------------------------------------------------------------
        enum ECompressPreset { E_COMPRESS_NONE = 0, E_COMPRESS_MAX_DRIVETIME = 1, E_COMPRESS_MAX_TOTAL = 2 };

        // ---- CompressSelectedRig_MaxDrivetime tuning (X360 .rdata) -------------------------------
        const f32 KF_DRIVETIME_EXTENT_LIMIT  = 3.4028235e38f;  // flt_820224B0 (FLT_MAX seed)
        const f32 KF_DRIVETIME_COMPRESS_STEP = 0.05f;          // flt_82002138 (per-iteration scale add)
        const f32 KF_DRIVETIME_SCALE_LIMIT   = 0.99000001f;    // flt_8208F5EC (stop once scale >= ~0.99)

        // RGBA debug colours (X360 packed ARGB immediates, passed straight to the renderer).
        // rw::RGBA takes (R,G,B,A); the comment is the X360 packed 0xAARRGGBB immediate.
        const rw::RGBA KRGBA_WHITE       = rw::RGBA(0xFFu, 0xFFu, 0xFFu, 0xFFu);   // 0xFFFFFFFF (-1)        point highlight / arrow / drivetime box
        const rw::RGBA KRGBA_GREEN       = rw::RGBA(0x00u, 0xFFu, 0x00u, 0xFFu);   // 0xFF00FF00 (r24)       tag-point marker triangle
        const rw::RGBA KRGBA_RED         = rw::RGBA(0xFFu, 0x00u, 0x00u, 0xFFu);   // 0xFFFF0000 (r25)       driven-point marker triangle
        const rw::RGBA KRGBA_BLUE        = rw::RGBA(0x00u, 0x00u, 0xFFu, 0xFFu);   // 0xFF0000FF             deformed box
        const rw::RGBA KRGBA_OFFSET      = rw::RGBA(0x00u, 0x7Fu, 0x00u, 0xFFu);   // 0xFF007F00 (r21)       skinning-offset lines (dark green)
        const rw::RGBA KRGBA_CONNECTION  = rw::RGBA(0x7Fu, 0x00u, 0x00u, 0xFFu);   // 0xFF7F0000 (r23)       driven-point connection lines (dark red)

        const f32 KF_DEBUG_SPHERE_RADIUS   = 0.1f;    // flt_82004014 (highlight-sphere radius)
        const f32 KF_DEBUG_TRIANGLE_RADIUS = 0.05f;   // flt_820047C8 (marker-triangle half-extent)

        // Transform a model-local POINT / DIRECTION into world space by the car transform. These are
        // exactly the canonical rw::math::vpu::TransformPoint (lp.x*Right + lp.y*Up + lp.z*At + Pos)
        // and TransformVector (the same FMA chain WITHOUT the +Pos translation) -- the vspltw128 +
        // vmaddfp128 cascades the X360 inlines (v127=Right, v126=Up, v125=At, v123=Pos). Local
        // forwarders keep the call sites short and unambiguous (the bare names would otherwise also
        // resolve via ADL straight into rw::math::vpu).
        inline Vector3 TransformPointToWorld(const Matrix44Affine& lrXform, const Vector3& lvLocal)
        {
            return rw::math::vpu::TransformPoint(lrXform, lvLocal);
        }
        inline Vector3 TransformDirectionToWorld(const Matrix44Affine& lrXform, const Vector3& lvLocal)
        {
            return rw::math::vpu::TransformVector(lrXform, lvLocal);
        }
    }

    // =============================================================================================
    // DeformationAxisAlignedBox::GetExtent  @ 0x825DEC50
    //
    // Return the box's signed extent along one of the six ENextSensorDirection axes. The X360 body is a
    // 6-case jump table: the even (POS_*) cases read the min corner (this+0), the odd (NEG_*) cases read
    // the max corner (this+16); the lane (x/y/z) follows the axis pair. The default returns the min
    // corner's x lane (vspltisw 0 then the back_chain reload == mMin.x).
    // =============================================================================================
    f32 DeformationAxisAlignedBox::GetExtent(ENextSensorDirection leAxis) const
    {
        switch ( leAxis )
        {
            case E_NSD_POS_XAXIS: return mMin.x;
            case E_NSD_NEG_XAXIS: return mMax.x;
            case E_NSD_POS_YAXIS: return mMin.y;
            case E_NSD_NEG_YAXIS: return mMax.y;
            case E_NSD_POS_ZAXIS: return mMin.z;
            case E_NSD_NEG_ZAXIS: return mMax.z;
            default:              return mMin.x;
        }
    }

    // =============================================================================================
    // Construct  @ 0x82606480
    //
    // Two-phase init: base DebugComponent::Construct (the folded BaseCollisionGenerator::Destruct),
    // assert + cache the manager, zero every slider/selector/flag, then refresh the selected rig.
    // =============================================================================================
    void DeformationDebugComponent::Construct(DeformationManager* lpDeformationManager)
    {
        CgsDev::DebugComponent::Construct();   // base two-phase init (X360 folded body)

        CGS_ASSERT( lpDeformationManager != nullptr, "lpDeformationManager" );   // BrnDeformationDebugComponent.cpp:158
        mpDeformationManager = lpDeformationManager;

        mfCompressRightSide = 0.0f;
        mfCompressLeftSide  = 0.0f;
        mfCompressFloor     = 0.0f;
        mfCompressRoof      = 0.0f;
        mfCompressRear      = 0.0f;
        mfCompressFront     = 0.0f;

        mbRenderDeformationRig          = false;
        mbRenderTagPoints               = false;
        mbRenderDrivenPoints            = false;
        mbRenderOffsets                 = false;
        mbRenderDrivenPointConnections  = false;

        miSelectedSensor   = 0;
        mpSelectedSensor   = nullptr;
        miSelectedRig      = 0;
        mpSelectedRig      = nullptr;
        miCompressPreset   = 0;

        mbDrawDrivetimeLimitsBox = false;
        mbDrawDeformedBox        = false;
        mbDrawDetachedWheels     = false;

        OnSelectedRigChange( &miSelectedRig, this );
    }

    // =============================================================================================
    // Update  @ 0x82623178
    //
    // If a rig is selected and it has a streamed deformation spec, recompute its drive-time limits.
    // =============================================================================================
    void DeformationDebugComponent::Update()
    {
        if ( mpSelectedRig != nullptr && mpSelectedRig->GetDeformationSpec() != nullptr )
        {
            mpSelectedRig->CalculateDriveTimeLimitsDebug();
        }
    }

    // =============================================================================================
    // OnSelectedRigChange  @ 0x825DF408  (registered VariableCallbackFunction; static)
    //
    // The "Selected rig" slider changed. Validate the index against the active-rigs bit array, then:
    //  - if that rig is live: bind mpSelectedRig to it, enable the compression sliders, flip the global
    //    deformation-debug allow flag, and refresh the selected sensor;
    //  - otherwise: clear mpSelectedRig + the sensor/point selection and make every editable read-only.
    // =============================================================================================
    void DeformationDebugComponent::OnSelectedRigChange(void* /*lpValue*/, void* lpUserData)
    {
        DeformationDebugComponent* lpThis = static_cast<DeformationDebugComponent*>(lpUserData);

        const s32 liRig = lpThis->miSelectedRig;
        CGS_ASSERT( static_cast<u32>(liRig) < 28u, "invalid index : liIndex < 28" );   // CgsBitArray.h:203 (non-gating)

        DeformationManager* lpMgr = lpThis->mpDeformationManager;
        if ( lpMgr != nullptr && lpMgr->IsDeformableObjectActive(liRig) )
        {
            lpThis->mpSelectedRig = lpMgr->GetDeformableObject(liRig);

            lpThis->SetReadOnly( &lpThis->mfCompressRightSide, false );
            lpThis->SetReadOnly( &lpThis->mfCompressLeftSide,  false );
            lpThis->SetReadOnly( &lpThis->mfCompressFloor,     false );
            lpThis->SetReadOnly( &lpThis->mfCompressRoof,      false );
            lpThis->SetReadOnly( &lpThis->mfCompressRear,      false );
            lpThis->SetReadOnly( &lpThis->mfCompressFront,     false );

            kbAllowDeformationDebug = true;
            OnSelectedSensorChange( &lpThis->miSelectedSensor, lpThis );
        }
        else
        {
            lpThis->mpSelectedRig    = nullptr;
            lpThis->mpSelectedSensor = nullptr;
            lpThis->miSelectedSensor = 0;

            lpThis->SetReadOnly( &lpThis->miSelectedSensor, true );
            lpThis->SetReadOnly( &lpThis->mfSensorX,        true );
            lpThis->SetReadOnly( &lpThis->mfSensorX,        true );
            lpThis->SetReadOnly( &lpThis->mfSensorY,        true );
            lpThis->SetReadOnly( &lpThis->mfSensorZ,        true );
            lpThis->SetReadOnly( &lpThis->mfSensorScratch,  true );
            lpThis->SetReadOnly( &lpThis->mfCompressRightSide, true );
            lpThis->SetReadOnly( &lpThis->mfCompressLeftSide,  true );
            lpThis->SetReadOnly( &lpThis->mfCompressFloor,     true );
            lpThis->SetReadOnly( &lpThis->mfCompressRoof,      true );
            lpThis->SetReadOnly( &lpThis->mfCompressRear,      true );
            lpThis->SetReadOnly( &lpThis->mfCompressFront,     true );
        }
    }

    // =============================================================================================
    // OnSelectedSensorChange  @ 0x825DF310  (registered VariableCallbackFunction; static)
    //
    // The "Selected sensor" slider changed. If a rig is selected and the index is in range, bind the
    // selected sensor, copy its local-sphere centre + scratch amount into the X/Y/Z/scratch sliders, and
    // make those four editable.
    // =============================================================================================
    void DeformationDebugComponent::OnSelectedSensorChange(void* /*lpValue*/, void* lpUserData)
    {
        DeformationDebugComponent* lpThis = static_cast<DeformationDebugComponent*>(lpUserData);

        DeformableObject* lpRig = lpThis->mpSelectedRig;
        if ( lpRig == nullptr )
        {
            return;
        }

        const s32 liSensor = lpThis->miSelectedSensor;
        if ( liSensor >= lpRig->GetDeformationSpec()->GetNumDeformationSensors() )
        {
            return;
        }

        // The X360 seeds the sensor index at +15 (the reserved world/swept slots precede the live grid).
        DeformationSensor& lrSensor = lpRig->GetSensorDebug( liSensor + 15 );
        lpThis->mpSelectedSensor = &lrSensor;

        const Vector4& lvCentre = lrSensor.GetLocalSphereCentre();
        lpThis->mfSensorX       = lvCentre.x;
        lpThis->mfSensorY       = lvCentre.y;
        lpThis->mfSensorZ       = lvCentre.z;
        lpThis->mfSensorScratch = lrSensor.GetScratchAmount();

        lpThis->SetReadOnly( &lpThis->miSelectedSensor, false );
        lpThis->SetReadOnly( &lpThis->mfSensorX,        false );
        lpThis->SetReadOnly( &lpThis->mfSensorY,        false );
        lpThis->SetReadOnly( &lpThis->mfSensorZ,        false );
        lpThis->SetReadOnly( &lpThis->mfSensorScratch,  false );
    }

    // =============================================================================================
    // OnSelectedPointChange  @ 0x825DF688  (registered VariableCallbackFunction; static)
    //
    // The "Selected point" slider changed. Copy the selected point's position into the point sliders:
    // a tag point (index < live tag-point count) or, beyond that, a driven point.
    // =============================================================================================
    void DeformationDebugComponent::OnSelectedPointChange(void* /*lpValue*/, void* lpUserData)
    {
        DeformationDebugComponent* lpThis = static_cast<DeformationDebugComponent*>(lpUserData);

        DeformableObject* lpRig = lpThis->mpSelectedRig;
        if ( lpRig == nullptr )
        {
            return;
        }

        const s32 liPoint        = lpThis->miSelectedPoint;
        const s32 liNumTagPoints = lpRig->GetNumTagPointsDebug();
        if ( liPoint >= liNumTagPoints )
        {
            const Vector3 lvPos = lpRig->GetDrivenPointDebug( liPoint - liNumTagPoints ).GetPosition();
            lpThis->mfPointX = lvPos.x;
            lpThis->mfPointY = lvPos.y;
            lpThis->mfPointZ = lvPos.z;
        }
        else
        {
            const Vector3& lvPos = lpRig->GetTagPointDebug( liPoint ).GetPosition();
            lpThis->mfPointX = lvPos.x;
            lpThis->mfPointY = lvPos.y;
            lpThis->mfPointZ = lvPos.z;
        }
    }

    // =============================================================================================
    // OnPointPositionChange  @ 0x825B98F8  (registered VariableCallbackFunction; static)
    //
    // One of the point X/Y/Z sliders changed. Write the new position back into the selected point (tag
    // or driven point, preserving the point's w/distance lane), then mark the rig deformed-this-frame.
    // =============================================================================================
    void DeformationDebugComponent::OnPointPositionChange(void* /*lpValue*/, void* lpUserData)
    {
        DeformationDebugComponent* lpThis = static_cast<DeformationDebugComponent*>(lpUserData);

        DeformableObject* lpRig = lpThis->mpSelectedRig;
        const s32 liPoint        = lpThis->miSelectedPoint;
        const s32 liNumTagPoints = lpRig->GetNumTagPointsDebug();

        const Vector3 lvNewPos = { lpThis->mfPointX, lpThis->mfPointY, lpThis->mfPointZ, 0.0f };

        if ( liPoint >= liNumTagPoints )
        {
            // Driven point: keep the w lane (distance-to-A), overwrite xyz.
            IKDrivenPoint& lrPoint = lpRig->GetDrivenPointDebug( liPoint - liNumTagPoints );
            lrPoint.SetPosition( lvNewPos );
        }
        else
        {
            lpRig->GetTagPointDebug( liPoint ).SetPosition( lvNewPos );
        }

        lpRig->SetDeformedThisFrameDebug();
    }

    // =============================================================================================
    // OnCompressionChange  @ 0x825DF2E0  (registered VariableCallbackFunction; static)
    //
    // Any of the six compression sliders changed: re-apply the whole compression set to the selected rig
    // (a tail call -- the asm forwards straight to CompressSelectedRig with the six current slider values).
    // =============================================================================================
    void DeformationDebugComponent::OnCompressionChange(void* /*lpValue*/, void* lpUserData)
    {
        DeformationDebugComponent* lpThis = static_cast<DeformationDebugComponent*>(lpUserData);

        if ( lpThis->mpSelectedRig != nullptr )
        {
            lpThis->CompressSelectedRig( lpThis->mfCompressRightSide, lpThis->mfCompressLeftSide,
                                         lpThis->mfCompressFloor,     lpThis->mfCompressRoof,
                                         lpThis->mfCompressRear,      lpThis->mfCompressFront );
        }
    }

    // =============================================================================================
    // OnCompressionPresetChange  @ 0x826073A8  (registered VariableCallbackFunction; static)
    //
    // The "Preset" selector changed. None -> uncompress; Max drivetime -> compress fully then sweep each
    // axis to its drive-time limit; Max total -> compress fully; anything >= 3 -> assert (invalid preset).
    // =============================================================================================
    void DeformationDebugComponent::OnCompressionPresetChange(void* /*lpValue*/, void* lpUserData)
    {
        DeformationDebugComponent* lpThis = static_cast<DeformationDebugComponent*>(lpUserData);

        if ( lpThis->mpSelectedRig == nullptr )
        {
            return;
        }

        const s32 liPreset = lpThis->miCompressPreset;
        f32 lfScale;
        if ( liPreset == E_COMPRESS_NONE )
        {
            lfScale = 0.0f;
        }
        else if ( liPreset == E_COMPRESS_MAX_DRIVETIME )
        {
            lpThis->CompressSelectedRig( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
            lpThis->CompressSelectedRig_MaxDrivetime();
            return;
        }
        else if ( liPreset >= 3 )
        {
            CGS_ASSERT( false, "Invalid compression preset" );   // BrnDeformationDebugComponent.cpp:2009
            return;
        }
        else   // E_COMPRESS_MAX_TOTAL
        {
            lfScale = 1.0f;
        }

        lpThis->CompressSelectedRig( lfScale, lfScale, lfScale, lfScale, lfScale, lfScale );
    }

    // =============================================================================================
    // CompressSelectedRig_MaxDrivetime  @ 0x82607160
    //
    // For each of the six box directions, binary-narrow the compression scale toward the drive-time
    // limit: starting from the deformed box's per-axis extent, keep compressing (CompressSelectedRig +
    // UpdateDeformedBBox) while the |extent| is still shrinking and the scale is below ~0.99. The X360
    // reads the deformed box's two corners (model frame) and the drive-time limit box.
    // =============================================================================================
    void DeformationDebugComponent::CompressSelectedRig_MaxDrivetime()
    {
        DeformableObject* lpRig = mpSelectedRig;
        if ( lpRig == nullptr )
        {
            return;
        }

        // Helper: read the rig's current model-space deformed box into a DeformationAxisAlignedBox.
        CgsGeometric::AxisAlignedBox lScratch;

        for ( u32 luAxis = 0; luAxis < 6u; ++luAxis )
        {
            const ENextSensorDirection leAxis = static_cast<ENextSensorDirection>(luAxis);

            // The drive-time limit box (model space).
            DeformationAxisAlignedBox lLimitBox;
            lLimitBox.mMin = lpRig->GetDriveTimeLimitsMin();
            lLimitBox.mMax = lpRig->GetDriveTimeLimitsMax();

            // The current deformed box.
            lpRig->GetDeformedBoundingBox( &lScratch );
            DeformationAxisAlignedBox lDeformedBox;
            lDeformedBox.mMin = Vector3{ lScratch.mMin.x, lScratch.mMin.y, lScratch.mMin.z, 0.0f };
            lDeformedBox.mMax = Vector3{ lScratch.mMax.x, lScratch.mMax.y, lScratch.mMax.z, 0.0f };

            f32 lfBestExtent = KF_DRIVETIME_EXTENT_LIMIT;
            const f32 lfLimitExtent = std::fabs( lLimitBox.GetExtent( leAxis ) );

            // Only sweep this axis if the deformed box already reaches past the drive-time limit.
            if ( std::fabs( lDeformedBox.GetExtent( leAxis ) ) <= lfLimitExtent )
            {
                continue;
            }

            f32 lfScale = 0.0f;
            while ( lfScale < KF_DRIVETIME_SCALE_LIMIT )
            {
                lfScale += KF_DRIVETIME_COMPRESS_STEP;

                // Re-read the current slider values, bump the swept axis' lane, re-apply + rebuild bbox.
                f32 lafScales[6] = { mfCompressRightSide, mfCompressLeftSide, mfCompressFloor,
                                     mfCompressRoof,      mfCompressRear,     mfCompressFront };
                lafScales[luAxis] = lfScale;
                CompressSelectedRig( lafScales[0], lafScales[1], lafScales[2],
                                     lafScales[3], lafScales[4], lafScales[5] );
                lpRig->UpdateDeformedBBoxDebug();

                lpRig->GetDeformedBoundingBox( &lScratch );
                lDeformedBox.mMin = Vector3{ lScratch.mMin.x, lScratch.mMin.y, lScratch.mMin.z, 0.0f };
                lDeformedBox.mMax = Vector3{ lScratch.mMax.x, lScratch.mMax.y, lScratch.mMax.z, 0.0f };
                const f32 lfExtent = std::fabs( lDeformedBox.GetExtent( leAxis ) );
                if ( lfExtent >= lfBestExtent )
                {
                    break;   // stopped shrinking -- this axis is at its limit
                }
                lfBestExtent = lfExtent;
                if ( lfExtent <= lfLimitExtent )
                {
                    break;   // reached the drive-time limit
                }
            }
        }

        lpRig->UpdateDeformedBBoxDebug();
    }

    // =============================================================================================
    // DetachPart  @ 0x825B97F8
    //
    // Flag the selected rig's "render car part" detach state. The X360 scans the rig's IK parts for the
    // one matching giDebugOnlyRenderThisCarPart (and fires the legacy "No longer works" assert if found),
    // then sets the rig's detach flag. The scan is a no-op tripwire; the meaningful effect is the flag.
    // =============================================================================================
    void DeformationDebugComponent::DetachPart()
    {
        DeformableObject* lpRig = mpSelectedRig;
        if ( lpRig == nullptr )
        {
            return;
        }

        const s32 liNumParts = lpRig->GetNumIKPartsDebug();
        for ( s32 liPart = 0; liPart < liNumParts; ++liPart )
        {
            if ( lpRig->GetIKPartDebug( liPart ).GetMeshId() == giDebugOnlyRenderThisCarPart )
            {
                CGS_ASSERT( false, "No longer works" );   // BrnDeformationDebugComponent.cpp:2075
                break;
            }
        }

        lpRig->SetDeformedThisFrameDebug();
    }

    // Static thunk so DetachPart can be registered as a DebugCallbackFunction (void(*)(void*)).
    void DeformationDebugComponent::DetachPartCallback(void* lpUserData)
    {
        static_cast<DeformationDebugComponent*>(lpUserData)->DetachPart();
    }

    // =============================================================================================
    // OnActivate  @ 0x82623198
    //
    // Register every editable with the debug menu. The X360 opens with a folded
    // BaseCollisionGenerator::Destruct(this) -- the trivial base prep the image folds onto one address;
    // it is a no-op here. The body then registers, in order:
    //   * the six compression sliders (step 0.025, range [0,1], read-only, OnCompressionChange);
    //   * the compression preset selector (None/Max drivetime/Max total, OnCompressionPresetChange);
    //   * the render-flag toggles + the "Selected rig" picker + the Slow-Mo flag;
    //   * the render-car-part selector + the three action buttons (Reset Sensors / Detach part / Reset
    //     selected rig) + the three master allow-flags;
    //   * the selected-sensor / selected-point pickers + their position sliders;
    //   * the impulse-scale + part-tuning + detachment-tuning globals.
    // Finally it re-selects rig 0 and sensor 0.
    // =============================================================================================
    void DeformationDebugComponent::OnActivate()
    {
        // (folded BaseCollisionGenerator::Destruct(this) == empty base prep -- no-op)

        // ---- six compression sliders -----------------------------------------------------------
        f32* const lapCompress[6] = { &mfCompressFront, &mfCompressRear, &mfCompressRightSide,
                                      &mfCompressLeftSide, &mfCompressFloor, &mfCompressRoof };
        const char* const lapCompressNames[6] = { "Front", "Rear", "Right side", "Left side", "Floor", "Roof" };
        for ( s32 li = 0; li < 6; ++li )
        {
            RegisterVariable( lapCompress[li], KPC_GROUP_COMPRESS, lapCompressNames[li] );
        }
        // The X360 sets step/range/read-only/callback in the asm's order (Right side..Front).
        f32* const lapCompressOrder[6] = { &mfCompressRightSide, &mfCompressLeftSide, &mfCompressFloor,
                                           &mfCompressRoof, &mfCompressRear, &mfCompressFront };
        for ( s32 li = 0; li < 6; ++li ) { SetStep( lapCompressOrder[li], KF_COMPRESS_STEP ); }
        for ( s32 li = 0; li < 6; ++li ) { SetRange( lapCompressOrder[li], KF_COMPRESS_MIN, KF_COMPRESS_MAX ); }
        for ( s32 li = 0; li < 6; ++li ) { SetReadOnly( lapCompressOrder[li], true ); }
        for ( s32 li = 0; li < 6; ++li ) { SetChangeCallback( lapCompressOrder[li], &OnCompressionChange, this ); }

        // ---- compression preset selector -------------------------------------------------------
        miCompressPreset                   = E_COMPRESS_MAX_TOTAL;   // X360 *(this+104)=2 (the high option's index)
        maCompressPresetStrings[0].miValue = 0; maCompressPresetStrings[0].mpcName = "None";
        maCompressPresetStrings[1].miValue = 1; maCompressPresetStrings[1].mpcName = "Max drivetime";
        maCompressPresetStrings[2].miValue = 2; maCompressPresetStrings[2].mpcName = "Max total";
        miCompressPreset                   = 0;                      // selected = None
        RegisterVariable( &miCompressPreset, KPC_GROUP_COMPRESS, "Preset" );
        SetRange( &miCompressPreset, 0, KI_NUM_COMPRESS_PRESETS );
        SetOptions( &miCompressPreset, maCompressPresetStrings );
        SetChangeCallback( &miCompressPreset, &OnCompressionPresetChange, this );

        // ---- render-flag toggles + rig picker + slow-mo flag -----------------------------------
        RegisterVariable( &mbDrawDrivetimeLimitsBox, KPC_GROUP_COMPRESS, "Render drivetime limits" );
        RegisterVariable( &mbDrawDeformedBox,        KPC_GROUP_COMPRESS, "Render deformed box" );
        RegisterVariable( &mbDrawDetachedWheels,     KPC_GROUP_COMPRESS, "Render detached wheels" );
        RegisterVariable( &miSelectedRig, "Selected rig" );
        RegisterVariable( &kbSlowMoDeformation, "Slow-Mo Deformation" );

        RegisterVariable( &mbRenderDeformationRig,         KPC_GROUP_RENDER, "Render deformation rig" );
        RegisterVariable( &mbRenderTagPoints,              KPC_GROUP_RENDER, "Render tag points" );
        RegisterVariable( &mbRenderDrivenPoints,           KPC_GROUP_RENDER, "Render driven points" );
        RegisterVariable( &mbRenderOffsets,                KPC_GROUP_RENDER, "Render skinning offsets" );
        RegisterVariable( &mbRenderDrivenPointConnections, KPC_GROUP_RENDER, "Render driven point connections" );
        RegisterVariable( &giDebugOnlyRenderThisCarPart,   KPC_GROUP_RENDER, "Render car part" );

        RegisterFunction( &ResetSensors,     this, "Reset Sensors" );
        RegisterFunction( &DetachPartCallback, this, "Detach part" );
        RegisterFunction( &ResetSelectedRig, this, "Reset selected rig" );

        RegisterVariable( &kbAllowDeformationDebug,     KPC_GROUP_FLAGS, "Allow deformation debug" );
        RegisterVariable( &kbAllowRandomPartDetachment, KPC_GROUP_FLAGS, "Allow random part Detachment" );
        RegisterVariable( &kbAllowDriveTimeDeformation, KPC_GROUP_FLAGS, "Allow Drive Time Deformation" );

        SetChangeCallback( &miSelectedRig, &OnSelectedRigChange, this );
        SetRange( &miSelectedRig, 0, KI_MAX_RIG_INDEX );

        // ---- selected sensor / point pickers ---------------------------------------------------
        RegisterVariable( &miSelectedSensor, KPC_GROUP_SENSORS, "Selected sensor" );
        SetChangeCallback( &miSelectedSensor, &OnSelectedSensorChange, this );
        SetRange( &miSelectedSensor, 0, KI_MAX_SENSOR_INDEX );

        RegisterVariable( &miSelectedPoint, KPC_GROUP_SENSORS, "Selected point" );
        SetChangeCallback( &miSelectedPoint, &OnSelectedPointChange, this );
        SetRange( &miSelectedPoint, 0, KI_MAX_POINT_INDEX );

        RegisterVariable( &mfSensorX,       KPC_GROUP_SENSORS, "Sensor X Pos" );
        RegisterVariable( &mfSensorY,       KPC_GROUP_SENSORS, "Sensor Y Pos" );
        RegisterVariable( &mfSensorZ,       KPC_GROUP_SENSORS, "Sensor Z Pos" );
        RegisterVariable( &mfSensorScratch, KPC_GROUP_SENSORS, "Sensor Scratch" );
        RegisterVariable( &mfPointX,        KPC_GROUP_SENSORS, "Point X Pos" );
        RegisterVariable( &mfPointY,        KPC_GROUP_SENSORS, "Point Y Pos" );
        RegisterVariable( &mfPointZ,        KPC_GROUP_SENSORS, "Point Z Pos" );

        SetStep( &mfPointX, KF_POINT_STEP );
        SetStep( &mfPointY, KF_POINT_STEP );
        SetStep( &mfPointZ, KF_POINT_STEP );

        SetChangeCallback( &mfSensorX,       &OnSensorPositionChange, this );
        SetChangeCallback( &mfSensorY,       &OnSensorPositionChange, this );
        SetChangeCallback( &mfSensorZ,       &OnSensorPositionChange, this );
        SetChangeCallback( &mfSensorScratch, &OnSensorPositionChange, this );
        SetChangeCallback( &mfPointX,        &OnPointPositionChange, this );
        SetChangeCallback( &mfPointY,        &OnPointPositionChange, this );
        SetChangeCallback( &mfPointZ,        &OnPointPositionChange, this );

        SetStep( &mfSensorX,       KF_SENSOR_STEP );
        SetStep( &mfSensorY,       KF_SENSOR_STEP );
        SetStep( &mfSensorZ,       KF_SENSOR_STEP );
        SetStep( &mfSensorScratch, KF_SENSOR_STEP );

        SetReadOnly( &miSelectedSensor, true );
        SetReadOnly( &mfSensorX,        true );
        SetReadOnly( &mfSensorY,        true );
        SetReadOnly( &mfSensorZ,        true );
        SetReadOnly( &mfSensorScratch,  true );

        // ---- impulse-scale + part + detachment tuning globals ----------------------------------
        RegisterVariable( &kfNormalImpulseScale, KPC_GROUP_IMPULSE, "Normal Impulse Scale" );
        SetRange( &kfNormalImpulseScale, 0.0f, 1.0f );
        SetStep( &kfNormalImpulseScale, KF_IMPULSE_STEP );
        RegisterVariable( &kfFrictionImpulseScale, KPC_GROUP_IMPULSE, "Friction Impulse Scale" );
        SetRange( &kfFrictionImpulseScale, 0.0f, 1.0f );
        SetStep( &kfFrictionImpulseScale, KF_IMPULSE_STEP );

        RegisterVariable( &kfPartLinearDrag, KPC_GROUP_PARTS, "Part linear drag" );
        SetStep( &kfPartLinearDrag, KF_IMPULSE_STEP );
        RegisterVariable( &kfPartAngularDrag, KPC_GROUP_PARTS, "Part angular drag" );
        SetStep( &kfPartAngularDrag, KF_IMPULSE_STEP );
        RegisterVariable( &kfPartMaxLinearVelocity, KPC_GROUP_PARTS, "Part max linear velocity" );
        SetStep( &kfPartMaxLinearVelocity, KF_SENSOR_STEP );
        RegisterVariable( &kfPartMaxAngularVelocity, KPC_GROUP_PARTS, "Part max angular velocity" );
        SetStep( &kfPartMaxAngularVelocity, KF_SENSOR_STEP );
        RegisterVariable( &kfPartMass, KPC_GROUP_PARTS, "Part mass" );
        SetStep( &kfPartMass, KF_SENSOR_STEP );
        RegisterVariable( &kfPartInertiaMultiplier, KPC_GROUP_PARTS, "Part inertia multiplier" );
        SetStep( &kfPartInertiaMultiplier, KF_SENSOR_STEP );
        RegisterVariable( &kfPartExtraGravity, KPC_GROUP_PARTS, "Part extra gravity" );
        SetStep( &kfPartExtraGravity, KF_SENSOR_STEP );
        RegisterVariable( &kfPartDynamicFriction, KPC_GROUP_PARTS, "Part dynamic friction" );
        SetStep( &kfPartDynamicFriction, KF_SENSOR_STEP );
        RegisterVariable( &kfPartStaticFriction, KPC_GROUP_PARTS, "Part static friction" );
        SetStep( &kfPartStaticFriction, KF_SENSOR_STEP );

        mfAngularVelocityForDetachment = kfAngularVelocityForDetachment;
        mfAngularVelocityDecay         = kfAngularVelocityDecay;
        RegisterVariable( &mfAngularVelocityForDetachment, KPC_GROUP_DETACH, "Angular velocity for detachment" );
        SetStep( &mfAngularVelocityForDetachment, KF_SENSOR_STEP );
        RegisterVariable( &mfAngularVelocityDecay, KPC_GROUP_DETACH, "Angular velocity decay" );
        SetStep( &mfAngularVelocityDecay, KF_DECAY_STEP );

        RegisterVariable( &mfPenetrationLimitMultiplier, KPC_GROUP_DETACH, "Joint penetration multiplier" );
        SetStep( &mfPenetrationLimitMultiplier, KF_SENSOR_STEP );
        mfPenetrationLimitMultiplier = kfJointPenetrationMultiplier;
        RegisterVariable( &mfForceLimitMultiplier, KPC_GROUP_DETACH, "Joint force multiplier" );
        SetStep( &mfForceLimitMultiplier, KF_SENSOR_STEP );
        mfForceLimitMultiplier = kfJointForceMultiplier;

        SetChangeCallback( &mfPenetrationLimitMultiplier, &JointPentrationMultiplierCallback, this );
        SetChangeCallback( &mfForceLimitMultiplier,       &JointForceMultiplierCallback, this );
        SetChangeCallback( &mfAngularVelocityForDetachment, &AngularVelocityDetachmentCallback, this );
        SetChangeCallback( &mfAngularVelocityDecay,         &AngularVelocityDetachmentCallback, this );

        // ---- initial selection -----------------------------------------------------------------
        miSelectedRig = 0;
        OnSelectedRigChange( &miSelectedRig, this );
        miSelectedSensor = 0;
        OnSelectedSensorChange( &miSelectedSensor, this );
    }

    // =============================================================================================
    // DrawDetachedWheels  @ 0x825DED20
    //
    // Draw a hollow sphere at each of the four wheels whose physics state has gone "detached" (Wheel::
    // mu8State == 2). For each wheel: resolve its streamed wheel spec (StreamedDeformationSpec::GetW), and
    // -- if the spec is mapped to a tag point (liTagPointIndex != -1) and the wheel position is finite --
    // transform the wheel position by the car's world transform and draw the marker.
    // =============================================================================================
    void DeformationDebugComponent::DrawDetachedWheels(CgsDev::Debug3DImmediateRender* lpRender)
    {
        CGS_ASSERT( lpRender != nullptr, "lpDisplay != NULL" );   // BrnDeformationDebugComponent.cpp:536

        DeformableObject* lpRig = mpSelectedRig;
        if ( lpRig == nullptr )
        {
            return;
        }
        const StreamedDeformationSpec* lpSpec = lpRig->GetDeformationSpec();
        if ( lpSpec == nullptr )
        {
            return;
        }
        BrnPhysics::Vehicle::VehiclePhysics* lpPhysics = lpRig->GetVehiclePhysicsDebug();
        if ( lpPhysics == nullptr )
        {
            return;
        }

        const Matrix44Affine& lrXform = lpPhysics->GetTransform();

        for ( s32 liWheel = 0; liWheel < 4; ++liWheel )
        {
            const BrnPhysics::Vehicle::Wheel& lrWheel =
                lpPhysics->GetWheel( static_cast<BrnPhysics::Vehicle::VehiclePhysics::EVehicleDrivenWheel>(liWheel) );
            CGS_ASSERT( &lrWheel != nullptr, "lpWheel" );   // BrnDeformationDebugComponent.cpp:556 (non-gating)

            const WheelSpec* lpWheelSpec = lpSpec->GetW( liWheel );
            CGS_ASSERT( lpWheelSpec != nullptr, "lpWheelSpec" );   // BrnDeformationDebugComponent.cpp:560

            if ( lpWheelSpec->liTagPointIndex == -1 )
            {
                continue;
            }

            // The wheels that have actually gone detached (mu8State == E_WHEEL_STATE_DETACHED, 2) always
            // get a marker. A still-attached wheel only warrants one once its mapped tag point has
            // displaced far enough from its rest position: the X360 (0x825DEE74-0x825DEEC0) reads the
            // rig's tag point at liTagPointIndex, forms (current pos - spec initial pos), and compares its
            // squared length (vmsum3fp128) against the tag spec's squared detach threshold
            // (mfDetachThresholdSquared, splatted) with a vcmpgtfp. -- NOT the wheel's body-point velocity,
            // and NOT against zero.
            if ( lrWheel.mu8State != 2 )
            {
                const TagPoint& lrTag = lpRig->GetTagPointDebug( lpWheelSpec->liTagPointIndex );
                const Vector3& lvCurrent  = lrTag.GetPosition();
                const Vector3  lvInitial  = lrTag.GetInitialPosition();
                const Vector3  lvDisplace = rw::math::vpu::Subtract( lvCurrent, lvInitial );
                const f32 lfDisplaceSq = rw::math::vpu::MagnitudeSquared( lvDisplace );
                if ( !( lfDisplaceSq > lrTag.GetDetachThresholdSquared() ) )
                {
                    continue;
                }
            }

            // The wheel position must be finite (the X360 vcmpeqfp. self-compare across xyz). Skip NaNs.
            const Vector3& lvWheelPos = lrWheel.mPosition;
            const bool lbFinite = ( lvWheelPos.x == lvWheelPos.x ) && ( lvWheelPos.y == lvWheelPos.y )
                                  && ( lvWheelPos.z == lvWheelPos.z );
            CGS_ASSERT( lbFinite, "Invalid wheel position: , please tell Graham D." );   // Wheel.h:412

            const Vector3 lvWorld = TransformPointToWorld( lrXform, lvWheelPos );
            lpRender->DrawHollowSphere( lvWorld, KF_DEBUG_SPHERE_RADIUS, KRGBA_WHITE );
        }
    }

    namespace
    {
        // ---------------------------------------------------------------------------------------------
        // "Render only this car part" membership test (the X360 dword_82CDB49C filter, asm
        // 0x82606784-0x82606830 for tag points / 0x82606998-0x82606A44 for driven points).
        //
        // When a specific car part is selected (giDebugOnlyRenderThisCarPart != -1), each FLAT-array
        // tag/driven point is only drawn if it is owned by an IK part whose spec mesh-id matches the
        // selection. The console does this by ADDRESS IDENTITY: it walks every IK part whose
        // spec->GetMeshId() == selectedPart and, for each, scans the part's own point window comparing
        // the window element ADDRESS against the flat-array element address (the part windows point
        // INTO the rig's flat pools, so a window element and the matching flat element are the same
        // object). Reconstructed here with the same pointer-identity test via the named accessors.
        // ---------------------------------------------------------------------------------------------
        bool IsTagPointInSelectedPart(DeformableObject& lrRig, const TagPoint* lpFlatTag, s32 liSelectedPart)
        {
            const s32 liNumParts = lrRig.GetNumIKPartsDebug();
            for ( s32 liPart = 0; liPart < liNumParts; ++liPart )
            {
                IKBodyPart& lrPart = lrRig.GetIKPartDebug( liPart );
                if ( lrPart.GetSpec()->GetMeshId() != liSelectedPart )
                {
                    continue;
                }
                const s32 liWindow = lrPart.GetSpec()->GetNumberOfTagPoints();
                for ( s32 li = 0; li < liWindow; ++li )
                {
                    if ( lrPart.GetTagPoint( li ) == lpFlatTag )
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool IsDrivenPointInSelectedPart(DeformableObject& lrRig, const IKDrivenPoint* lpFlatDriven,
                                         s32 liSelectedPart)
        {
            const s32 liNumParts = lrRig.GetNumIKPartsDebug();
            for ( s32 liPart = 0; liPart < liNumParts; ++liPart )
            {
                IKBodyPart& lrPart = lrRig.GetIKPartDebug( liPart );
                if ( lrPart.GetSpec()->GetMeshId() != liSelectedPart )
                {
                    continue;
                }
                const s32 liWindow = lrPart.GetSpec()->GetNumberOfDrivenPoints();
                for ( s32 li = 0; li < liWindow; ++li )
                {
                    if ( lrPart.GetDrivenPoint( li ) == lpFlatDriven )
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    }

    // =============================================================================================
    // RenderWorld  @ 0x82606548
    //
    // Draw EVERY active deformation rig into the 3D debug renderer, gated by the per-feature render
    // flags. The X360 walks the manager's active-rig bit array (mModelsAdded, manager+75904) with a
    // next-set-bit scan; for each live rig it pulls the rig out of the model pool (manager+76032,
    // stride 26496 == GetDeformableObject) and -- if the rig's mbActive byte (rig+26402) is set --
    // renders, in order, off the rig's FLAT pools:
    //   * the rig's deformation sensors, ONCE per rig (gated by mbRenderDeformationRig);
    //   * the FLAT tag-point array (count rig+19216, base rig+15120, stride 32): a green marker
    //     triangle, a white highlight sphere on the selected point, and an optional dark-green
    //     skinning-offset line;
    //   * the FLAT driven-point array (count rig+25376, base rig+19232, stride 48): a red marker
    //     triangle, a white highlight sphere on the selected point, a white IK direction arrow, the
    //     dark-red driving-tag-point connection lines, and an optional skinning-offset line;
    //   * when a specific car part is selected (giDebugOnlyRenderThisCarPart != -1), a DrawAxis gizmo
    //     for each of that part's tag points that carries a deformation joint.
    // Finally, off mpSelectedRig only: the drive-time-limits box, the deformed box, and the detached
    // wheels.
    //
    // Each tag/driven point is filtered by the "render only this car part" selector (a pointer-identity
    // membership test against the selected part's point windows -- see the file-scope helpers above).
    // The selected-point highlight is gated on the mpSelectedRig POINTER (rig+20), not the int index.
    // =============================================================================================
    void DeformationDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpRender)
    {
        CGS_ASSERT( mpDeformationManager != nullptr, "mpDeformationManager != NULL" );   // BrnDeformationDebugComponent.cpp:266

        const s32 liSelectedPart = giDebugOnlyRenderThisCarPart;   // -1 == all parts

        // Walk every live model slot via the manager's active-rig bit array (the asm's next-set-bit
        // scan over mModelsAdded). IsDeformableObjectActive(i) is exactly the bit the scan tests, so
        // iterating 0..27 and skipping inactive slots reproduces the same rig set in index order.
        for ( s32 liRigIndex = 0; liRigIndex < 28; ++liRigIndex )
        {
            if ( mpDeformationManager == nullptr || !mpDeformationManager->IsDeformableObjectActive( liRigIndex ) )
            {
                continue;
            }

            DeformableObject* lpRig = mpDeformationManager->GetDeformableObject( liRigIndex );

            // The rig must be live (asm rig+26402 == mbActive) before any of its points draw.
            if ( !lpRig->IsActiveDebug() )
            {
                continue;
            }

            const Matrix44Affine& lrXform = lpRig->GetVehiclePhysicsDebug()->GetTransform();

            // ---- sensors (ONCE per rig, before the point loops) --------------------------------
            if ( mbRenderDeformationRig )
            {
                lpRig->RenderSensorsDebug( lpRender, miSelectedSensor );
            }

            // ---- tag points (FLAT array) -------------------------------------------------------
            if ( mbRenderTagPoints )
            {
                const s32 liNumTagPoints = lpRig->GetNumTagPointsDebug();
                CGS_ASSERT( liNumTagPoints < 128, "liNumTagPoints < 128" );   // BrnDeformationDebugComponent.cpp:295
                for ( s32 liTag = 0; liTag < liNumTagPoints; ++liTag )
                {
                    TagPoint& lrTag = lpRig->GetTagPointDebug( liTag );

                    // Filter by the selected car part (pointer-identity membership; -1 == draw all).
                    if ( liSelectedPart != -1 && !IsTagPointInSelectedPart( *lpRig, &lrTag, liSelectedPart ) )
                    {
                        continue;
                    }

                    const Vector3 lvWorld = TransformPointToWorld( lrXform, lrTag.GetPosition() );

                    // Highlight the selected tag point (gate on the mpSelectedRig POINTER, rig+20).
                    if ( mpSelectedRig != nullptr && miSelectedPoint == liTag )
                    {
                        lpRender->DrawHollowSphere( lvWorld, KF_DEBUG_SPHERE_RADIUS, KRGBA_WHITE );
                    }

                    // A small marker triangle at each tag point (green). The X360 corners are
                    // A=lvWorld, B=lvWorld+{0,+r,0}, C=lvWorld+{0,0,+r} (in that order).
                    const Vector3 lvB = { lvWorld.x, lvWorld.y + KF_DEBUG_TRIANGLE_RADIUS, lvWorld.z,                         0.0f };
                    const Vector3 lvC = { lvWorld.x, lvWorld.y,                         lvWorld.z + KF_DEBUG_TRIANGLE_RADIUS, 0.0f };
                    lpRender->DrawHollowTriangle( lvWorld, lvB, lvC, KRGBA_GREEN );

                    // The skinning offset (tag world pos -> rotated initial-position offset), drawn as
                    // a line. lvWorld - rotate(pos - init) == transform(init), so draw to the
                    // world-space initial position.
                    if ( mbRenderOffsets )
                    {
                        const Vector3 lvInit = TransformPointToWorld( lrXform, lrTag.GetInitialPosition() );
                        lpRender->DrawLine( lvWorld, lvInit, KRGBA_OFFSET );
                    }
                }
            }

            // ---- driven points (FLAT array) ----------------------------------------------------
            if ( mbRenderDrivenPoints )
            {
                const s32 liNumDrivenPoints = lpRig->GetNumDrivenPointsDebug();
                CGS_ASSERT( liNumDrivenPoints < 128, "liNumDrivenPoints < 128" );   // BrnDeformationDebugComponent.cpp:362
                // The highlight index subtracts the SELECTED rig's tag-point count (asm reads
                // *(mpSelectedRig+19216), not the iterated rig's), so cache it once when a rig is
                // selected. Left at 0 when there is no selection (the highlight gate is off anyway).
                const s32 liSelectedRigTagPoints = ( mpSelectedRig != nullptr )
                                                       ? mpSelectedRig->GetNumTagPointsDebug() : 0;
                for ( s32 liDriven = 0; liDriven < liNumDrivenPoints; ++liDriven )
                {
                    IKDrivenPoint& lrDriven = lpRig->GetDrivenPointDebug( liDriven );

                    if ( liSelectedPart != -1 && !IsDrivenPointInSelectedPart( *lpRig, &lrDriven, liSelectedPart ) )
                    {
                        continue;
                    }

                    const Vector3 lvWorld = TransformPointToWorld( lrXform, lrDriven.GetPosition() );

                    // Highlight the selected driven point (gate on mpSelectedRig; the global point index
                    // minus the SELECTED rig's tag-point count selects into the driven-point array).
                    if ( mpSelectedRig != nullptr && miSelectedPoint - liSelectedRigTagPoints == liDriven )
                    {
                        lpRender->DrawHollowSphere( lvWorld, KF_DEBUG_SPHERE_RADIUS, KRGBA_WHITE );
                    }

                    const Vector3 lvB = { lvWorld.x, lvWorld.y + KF_DEBUG_TRIANGLE_RADIUS, lvWorld.z,                         0.0f };
                    const Vector3 lvC = { lvWorld.x, lvWorld.y,                         lvWorld.z + KF_DEBUG_TRIANGLE_RADIUS, 0.0f };
                    lpRender->DrawHollowTriangle( lvWorld, lvB, lvC, KRGBA_RED );

                    // The IK direction arrow: from the driven point to (point + rotated constraint
                    // direction). The X360 splats the driven point's direction lane (mDirection,
                    // driven+16) and rotates it by the car transform (no translation).
                    const Vector3 lvArrowTip = TransformDirectionToWorld( lrXform, lrDriven.GetDirection() );
                    const Vector3 lvArrowEnd = { lvWorld.x + lvArrowTip.x, lvWorld.y + lvArrowTip.y,
                                                 lvWorld.z + lvArrowTip.z, 0.0f };
                    lpRender->DrawArrow( lvWorld, lvArrowEnd, KRGBA_WHITE );

                    // The two connection lines to the driven point's driving tag points (A and B): each
                    // tag-point world position joined to the driven point.
                    if ( mbRenderDrivenPointConnections )
                    {
                        const TagPoint* lpTagA = lrDriven.GetTagPointA();
                        const TagPoint* lpTagB = lrDriven.GetTagPointB();
                        const Vector3 lvWorldA = TransformPointToWorld( lrXform, lpTagA->GetPosition() );
                        lpRender->DrawLine( lvWorld, lvWorldA, KRGBA_CONNECTION );
                        const Vector3 lvWorldB = TransformPointToWorld( lrXform, lpTagB->GetPosition() );
                        lpRender->DrawLine( lvWorld, lvWorldB, KRGBA_CONNECTION );
                    }

                    // The driven point's skinning offset (to its world-space original position).
                    if ( mbRenderOffsets )
                    {
                        const Vector3 lvOrig = TransformPointToWorld( lrXform, lrDriven.GetOriginalPosition() );
                        lpRender->DrawLine( lvWorld, lvOrig, KRGBA_OFFSET );
                    }
                }
            }

            // ---- per-joint axis gizmos for the selected car part -------------------------------
            // When a specific part is selected, draw a coordinate-axis gizmo at each of that part's
            // tag points that carries a deformation joint (asm 0x82606BF4-0x82606E24). The gizmo basis
            // is the joint's (defaultDirection, axis, axis x defaultDirection) frame, orthonormalised
            // then rotated by the car transform, anchored at the joint's transformed position.
            if ( liSelectedPart != -1 )
            {
                const s32 liNumParts = lpRig->GetNumIKPartsDebug();
                for ( s32 liPart = 0; liPart < liNumParts; ++liPart )
                {
                    IKBodyPart& lrPart = lpRig->GetIKPartDebug( liPart );
                    const IKBodyPartSpec* lpPartSpec = lrPart.GetSpec();
                    if ( lpPartSpec->GetMeshId() != liSelectedPart )
                    {
                        continue;
                    }

                    const s32 liNumPartTags = lpPartSpec->GetNumberOfTagPoints();
                    for ( s32 liTag = 0; liTag < liNumPartTags; ++liTag )
                    {
                        const TagPoint* lpTag = lrPart.GetTagPoint( liTag );
                        if ( !lpTag->HasJoint() )
                        {
                            continue;
                        }

                        const DeformationJointSpec* lpJoint = lpPartSpec->GetJointSpec( lpTag->GetJointIndex() );

                        // Build the joint's local basis: rows = (defaultDirection, axis,
                        // axis x defaultDirection), then orthonormalise (the X360 cross + OrthoNormalize3x3).
                        Matrix44Affine lLocalBasis;
                        lLocalBasis.xAxis = lpJoint->mJointDefaultDirection;
                        lLocalBasis.yAxis = lpJoint->mJointAxis;
                        lLocalBasis.zAxis = rw::math::vpu::Cross( lpJoint->mJointAxis, lpJoint->mJointDefaultDirection );
                        lLocalBasis.wAxis = lpJoint->mJointPosition;
                        const Matrix44Affine lOrtho = rw::math::vpu::OrthoNormalize3x3( lLocalBasis );

                        // Rotate each orthonormal axis by the car transform and place the gizmo at the
                        // transformed joint position.
                        Matrix44Affine lWorld;
                        lWorld.xAxis = TransformDirectionToWorld( lrXform, lOrtho.xAxis );
                        lWorld.yAxis = TransformDirectionToWorld( lrXform, lOrtho.yAxis );
                        lWorld.zAxis = TransformDirectionToWorld( lrXform, lOrtho.zAxis );
                        lWorld.wAxis = TransformPointToWorld( lrXform, lpJoint->mJointPosition );
                        lpRender->DrawAxis( lWorld, KRGBA_WHITE );
                    }
                }
            }
        }

        // ---- drive-time-limits box (mpSelectedRig only) ------------------------------------------
        if ( mbDrawDrivetimeLimitsBox && mpSelectedRig != nullptr )
        {
            const Matrix44Affine& lrXform = mpSelectedRig->GetVehiclePhysicsDebug()->GetTransform();
            lpRender->DrawBox( mpSelectedRig->GetDriveTimeLimitsMin(), mpSelectedRig->GetDriveTimeLimitsMax(),
                               lrXform, KRGBA_WHITE );
        }

        // ---- deformed box (mpSelectedRig only) ---------------------------------------------------
        if ( mbDrawDeformedBox && mpSelectedRig != nullptr )
        {
            const Matrix44Affine& lrXform = mpSelectedRig->GetVehiclePhysicsDebug()->GetTransform();
            CgsGeometric::AxisAlignedBox lBox;
            mpSelectedRig->GetDeformedBoundingBox( &lBox );
            const Vector3 lvMin = { lBox.mMin.x, lBox.mMin.y, lBox.mMin.z, 0.0f };
            const Vector3 lvMax = { lBox.mMax.x, lBox.mMax.y, lBox.mMax.z, 0.0f };
            lpRender->DrawBox( lvMin, lvMax, lrXform, KRGBA_BLUE );
        }

        // ---- detached wheels (uses mpSelectedRig internally) -------------------------------------
        if ( mbDrawDetachedWheels )
        {
            DrawDetachedWheels( lpRender );
        }
    }
}
}
