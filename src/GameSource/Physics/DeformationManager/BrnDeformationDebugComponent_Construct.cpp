#include "GameSource/Physics/DeformationManager/BrnDeformationDebugComponent.h"
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"                          // DeformationManager
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"         // DeformableObject + debug accessors
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h"        // DeformationSensor
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"  // StreamedDeformationSpec
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                                // CgsGeometric::Sphere (GetLocalSphereCentre)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                                // CGS_ASSERT

// ==================================================================================================
// The DeformationDebugComponent::Construct chain -- SPLIT OUT of BrnDeformationDebugComponent.cpp on
// 2026-08-03 (task #116). BUILD-MECHANICS SPLIT ONLY: every body below was MOVED verbatim.
//
// == WHY THE SPLIT ==
// BrnPhysics::PhysicsModule::Construct @0x825AE308 had been a LIVE EMPTY STUB in WorldLinkStubs.cpp;
// un-stubbing it pulls in DeformationManager::Construct @0x82621510, whose first act is to construct
// this component. BrnDeformationDebugComponent.cpp AS A WHOLE cannot be mounted: a MEASURED trial
// link (task #116, M2) put it at 53 unresolved externals -- 25 from OnActivate, 12 from RenderWorld,
// the rest from DrawDetachedWheels / CompressSelectedRig_MaxDrivetime / DetachPart / the point
// callbacks. That is the debug-menu registration surface and the Debug3DImmediateRender draw API.
//
// ⭐ NOT ONE of those 53 was referenced from Construct, OnSelectedRigChange or OnSelectedSensorChange.
// The construct chain needs nothing new -- which is why this split costs zero.
//
// The two out-of-line accessors come along because OnSelectedSensorChange calls both and this file
// was their only home.
//
// TO RE-MERGE: close the 53, mount BrnDeformationDebugComponent.cpp, move this text back.
// ==================================================================================================

namespace BrnPhysics
{
namespace Deformation
{
    // Set by OnSelectedRigChange; defined by the deformation-physics TUs (see the extern block in
    // BrnDeformationDebugComponent.cpp).
    extern bool kbAllowDeformationDebug;

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

    // ==============================================================================================
    // ⚠️⚠️ THE THREE VIRTUALS -- LOUD STUBS, AND HERE IS EXACTLY WHY.
    //
    // Defining DeformationManager::mDebugComponent (BrnDeformationConstructShims.cpp) instantiates
    // this class, and instantiating a polymorphic class emits its VTABLE, which the linker will not
    // let you have unless EVERY virtual has a definition somewhere in the link. This class declares
    // three (BrnDeformationDebugComponent.h:62/:67/:71) and all three have REAL, COMPLETE bodies
    // already written in BrnDeformationDebugComponent.cpp:
    //     Update      @0x82623178      RenderWorld @0x82606548      OnActivate  @0x82623198
    // Those bodies are NOT reproduced here and NOT re-derived; they are exactly the functions that
    // make that TU unmountable. The MEASURED trial link (task #116, M2) attributes 25 of its 53
    // unresolved externals to OnActivate and 12 to RenderWorld -- the CgsDev debug-menu
    // RegisterVariable/SetStep/SetRange surface and the Debug3DImmediateRender draw API, which are
    // blocked behind the DebugUI Window/MenuItem/CustomWindow layout reconstruction.
    //
    // ⛔ SO THESE ARE STUBS, AND THEY ARE DELIBERATELY LOUD -- never a quiet no-op. Nothing on this
    // build can reach them: a DebugComponent's virtuals are driven by the debug menu, and this
    // component is only ever handed to it by DeformationDebugComponent_Register, which is called
    // from DeformationManager::Prepare -- still in the unmounted BrnDeformationManager.cpp. If that
    // ever changes, the assert fires immediately instead of the component silently drawing nothing.
    //
    // ⚠️ WHEN BrnDeformationDebugComponent.cpp IS MOUNTED, DELETE THIS BLOCK FIRST -- otherwise it is
    // a duplicate-symbol error (which is the failure mode you want here, not a silent override).
    // ==============================================================================================
    void DeformationDebugComponent::Update()
    {
        CGS_ASSERT(false, "DeformationDebugComponent::Update: vtable stub reached -- the real body is "
                          "in BrnDeformationDebugComponent.cpp; mount it (see task #116)");
    }

    void DeformationDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* /*lpRender*/)
    {
        CGS_ASSERT(false, "DeformationDebugComponent::RenderWorld: vtable stub reached -- the real body "
                          "is in BrnDeformationDebugComponent.cpp; mount it (see task #116)");
    }

    void DeformationDebugComponent::OnActivate()
    {
        CGS_ASSERT(false, "DeformationDebugComponent::OnActivate: vtable stub reached -- the real body "
                          "is in BrnDeformationDebugComponent.cpp; mount it (see task #116)");
    }
}
}
