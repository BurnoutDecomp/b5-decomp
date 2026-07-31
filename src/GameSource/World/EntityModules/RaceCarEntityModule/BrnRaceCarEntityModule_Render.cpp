// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/
//   BrnRaceCarEntityModule_Render.cpp
//
// The race-car RENDER leg, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnWorld::RaceCarEntityModule::GenerateDispatchLists @ 0x822E79F8
//   BrnWorld::RaceCarEntityModule::RenderRaceCar         @ 0x822CF6A0
//
// ---- SIGNATURES COME FROM THE ASM, NOT FROM THE OLD DECLARATIONS -----------
// GenerateDispatchLists' committed PC declaration carried five arguments; the console
// call site (WorldModule::GenerateDispatchLists @0x827D27A0..0x827D27C8) loads NINE:
//     r4 = lpInput   r5 = &maRaceCarEntityIds   r6 = 12   r7 = 19   r8 = 20   r9 = 0
//     v1 = fog scattering   v2 = fog colour + white level   v3 = camera position
// 12 is the dispatch-frame OBJECT list; 19/20 are the race-car OPAQUE and TRANSPARENT
// MESH lists (compare the world's own 2 / 11 / 15). Those three integers are forwarded
// verbatim into RenderRaceCar and are the only thing that tells the submission leaf
// where to put the car, so dropping them made the render leg unimplementable.
//
// RenderRaceCar's own prologue (@0x822CF6C4..0x822CF764) homes r3->r20 (this),
// r5->r16 (the RenderParams), r6->r25 (the vehicle graphics ResourcePtr) and spills
// r4/r7/r8/r9/r10 to arg_1C/arg_64/arg_6C/arg_74/arg_7C; the call site @0x822E8534
// adds two STACK arguments -- arg_84 = the frame's ShadowMap and arg_8F = the
// "render attached geometry" bool. v1 is the camera->car DISTANCE (the caller builds it
// with vmsum3fp128 + vrsqrtefp + a Newton step + vmulfp, i.e. sqrt of the squared
// distance, broadcast into all four lanes) -- NOT a direction vector.
//
// ---- WHAT THIS FILE DOES *NOT* RECONSTRUCT (all of it named, none of it faked) ----
// 1. GenerateDispatchLists' OTHER branch. The console function is
//        if (byte_82CDB6B9) { ...scene-manager/replay path... } else { ...this... }
//    where byte_82CDB6B9 is a build-config byte with exactly ONE xref in the whole XEX
//    (this test) and no writer. The arm reconstructed here is the `else`: an 8-slot
//    sweep of maActiveRaceCars. The `if` arm walks the scene-manager visible-entity ids
//    instead and additionally drives the replay serialiser
//    (BrnReplays::RaceCarEntitySerialiser::GetStaticLayout), the blobby-shadow buffer
//    (BrnBlobbyShadowManager::BrnBlobbyShadowBuffer::AddShadow), ShadowMap::CalcOptimisedLod
//    and RaceCarEntityModule::SubmitCoronasForRaceCar -- none of which exist in the tree.
//    It is a later wave, not a stub: no empty branch is written for it here.
// 2. RenderRaceCar's WHEEL block (`if (mbRenderWheels && lbRenderAttachedGeometry)`).
//    It gathers per-wheel instance matrices and submits ONE instanced draw through
//    CgsGraphics::Model::SetupShaderConstantsForInstancing, which does not exist in the
//    tree, and whose draw leaf DrawInstancedIndexedPrimitive_Custom is an explicit
//    CGS_ASSERT(false) trap (CgsDispatcherCommands.cpp:1045 / :1177). A bodyshell-only
//    car is the honest state of this build.
// 3. RenderRaceCar's CRACKED-GLASS loop and its shader constants 24/25. The glass loop
//    needs the spec's shattered-glass part table plus two .rodata constant vectors the
//    function-only IDA export does not carry; constant 24 (the brake/reverse light
//    emission vector) additionally needs three module debug floats at +100232..+100240
//    that are not named in this header's layout yet.
// 4. The four IsNormal3x3 / IsOrthogonal3x3 dev-assert blocks (~73% of the function's
//    2008 instructions is assert scaffolding).
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h" // InputBuffer_GenerateDispatchLists

#include "GameShared/GameClasses/Graphics/CgsModel.h"                    // CgsGraphics::Model / Renderable
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"          // ShaderConstantTable
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"      // DispatchFrame / DispatchBin / DispatchList
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h" // DrawRenderable::AddToBin
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameSource/World/ShadowMap/BrnShadowMap.h"                     // BrnWorld::ShadowMap
#include "SharedClasses/World/BrnVehicleGraphicsSpec.h"                  // BrnVehicle::GraphicsSpec
#include "rw/math/vpu/matrix44affine_operation.h"                        // rw::math::vpu::Mult

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint

#include <cmath>   // powf / sqrtf

// The global runtime shader-constant register (X360 symbol mShaderConstantTable; bodied
// by the CgsShaderConstants TU). Mirrors the committed extern in the sibling TUs.
namespace CgsGraphics { extern ::ShaderConstantTable mShaderConstantTable; }

namespace BrnWorld
{

// ============================================================================
// RenderRaceCar  @ 0x822CF6A0
//
// Submit one race car's body-part draws into the frame's race-car lists. Flow (asm
// order, asserts elided -- see the banner):
//   * bail out entirely when the car is hidden        (`if (!*(rp + 5131))`)
//   * publish the per-car shader constants: paint (20), pearlescent (21), the
//     deformation verlet block (22/23, only when damaged), the fog blend (26)
//   * for each body part: visibility gate -> the part's Model -> DoesStateExist(mLOD)
//     -> world matrix = partLocator * bodyTransform -> the detached-part queue may
//     override that matrix -> bind it as shader constant 0 -> BeginPacket ->
//     DrawRenderable::AddToBin -> DispatchList::Submit.
// ============================================================================
void
RaceCarEntityModule::RenderRaceCar( CgsGraphics::DispatchFrame* lpDispatchFrame,
                                    ActiveRaceCar::RenderParams* lpRenderParams,
                                    const CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>* lpCarGraphics,
                                    const CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>* lpWheelGraphics,
                                    s32 liObjectList,
                                    s32 liOpaqueMeshList,
                                    s32 liTransparentMeshList,
                                    const ShadowMap* lpShadowMap,
                                    bool lbRenderAttachedGeometry,
                                    f32  lfCameraDistance,
                                    Vector4 lvFogScattering,
                                    Vector4 lvFogColourPlusWhiteLevel )
{
    CGS_ASSERT( lpRenderParams != 0, "lpRenderParams" );
    CGS_ASSERT( lpDispatchFrame != 0, "lpDispatchFrame" );
    CGS_ASSERT( lpShadowMap != 0, "lpShadowMap" );
    (void)lpWheelGraphics;   // consumed only by the (unreconstructed) wheel block

    // The X360 loads the four body-transform rows into v124/v126/v125/v117 straight from
    // r5 (the RenderParams) before anything else -- the matrix at offset 0 IS
    // mBodyTransform.
    const Matrix44Affine lBodyTransform = lpRenderParams->GetBodyTransform();

    // `if (!*(rp + 5131))` -- mbIsHidden gates the ENTIRE function body.
    if ( lpRenderParams->IsRaceCarHidden() )
    {
        return;
    }

    // Every spec read in the console body goes through ResourcePtr::operator-> (the
    // `BrnVehicle::GraphicsSpec_::operat(v56)` calls); it is hoisted once here.
    const BrnVehicle::GraphicsSpec* lpCarGraphicsSpec = lpCarGraphics->operator->();

    // ---- the shadow-pass selector ------------------------------------------
    // v320 = (shadowMap[0] && shadowMap[1]): the two LEADING ShadowMap bytes the asm reads
    // as `lbz r11, 0(a40)` / `lbz r11, 1(a40)` -- the same pair
    // WorldEntityModule::GenerateDispatchLists reads to select its shadow pass, i.e.
    // {mbRenderingShadowMap, mbUseZOnlyRenderingPath}. It selects the shadow variant of both
    // the technique index and the AddToBin routing (see below).
    const bool lbShadowPass = lpShadowMap->IsRenderingShadowMap()
                           && lpShadowMap->IsUsingZOnlyRenderingPath();

    // ---- per-car shader constants ------------------------------------------
    // 20 / 21: the paint + pearlescent tints, straight out of the render snapshot
    // (`lvx128 v1, r16, 2944` / `..., 2960`).
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 20, lpRenderParams->GetPaintColour() );
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 21, lpRenderParams->GetPearlescentColour() );

    // 26: the per-car FOG blend. The console inlines a powf here (vlogefp / vexptefp plus
    // the usual polynomial refinement over the .rodata coefficient vectors at
    // 0x82014AC0..0x82014AF0); de-optimised back to the library call it is:
    //     t = powf( saturate( distance * scattering.x - scattering.y ), scattering.z )
    //         * scattering.w
    //     constant26 = { fogColour.xyz * t, 1 - t }
    // (`vrlimi128 v1, v13, 1, 0` writes the 1-t into lane w only.)
    {
        f32 lfBlend = lfCameraDistance * lvFogScattering.x - lvFogScattering.y;
        if ( lfBlend < 0.0f ) { lfBlend = 0.0f; }
        if ( lfBlend > 1.0f ) { lfBlend = 1.0f; }
        const f32 lfFog = powf( lfBlend, lvFogScattering.z ) * lvFogScattering.w;

        const Vector4 lv4Fog = { lvFogColourPlusWhiteLevel.x * lfFog,
                                 lvFogColourPlusWhiteLevel.y * lfFog,
                                 lvFogColourPlusWhiteLevel.z * lfFog,
                                 1.0f - lfFog };
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 26, lv4Fog );
    }

    // 22 / 23: the deformation verlet block. `v107 = mbDamaged; if (kbAllowDeformationDebug)
    // v107 = 1;` then `if (v107 || <module debug bool>)` uploads the 128-entry offset array
    // as constant 22 and its companion scale as 23.
    // NOT reconstructed: ShaderConstantTable::SetShaderConstantArrayData has no body in this
    // tree for ANY of its five overloads (all declaration-only in CgsShaderConstants.h), so
    // the array upload cannot be expressed yet. It runs only for a DAMAGED car and nothing
    // on this build deforms one. mbDamaged is still read -- it selects the technique below.
    const bool lbDamaged = lpRenderParams->IsDamaged();

    // ---- the body-part loop -------------------------------------------------
    // `if (*(this + 99147))` -- the module's own render switch (see the header's
    // two-point offset fit). dword_82CDB49C is a debug "draw only this part index"
    // filter (-1 == every part); it has no writer in the XEX, so the -1 path is the
    // only reachable one and the filter is not modelled.
    if ( mbRenderCarsDuringCrash )
    {
        const ActiveRaceCar::RenderParams::DetachedPartRenderQueue& lrDetachedParts =
            lpRenderParams->GetDetachedPartQueue();

        const CgsGraphics::Model::State leLOD = lpRenderParams->GetLOD();

        for ( u32 luPartIdx = 0; luPartIdx < lpCarGraphicsSpec->muPartsCount; ++luPartIdx )
        {
            if ( !lpRenderParams->IsPartVisible( static_cast< u8 >( luPartIdx ) ) )
            {
                continue;
            }

            const CgsGraphics::Model* lpModel = lpCarGraphicsSpec->GetPartModel( luPartIdx );
            if ( lpModel == 0 )
            {
                continue;
            }

            if ( !lpModel->DoesStateExist( leLOD ) )
            {
                continue;
            }

            // World matrix = partLocator * bodyTransform. The asm broadcasts each locator row
            // component (vspltw) and accumulates it against the three body rotation rows, seeding
            // the translation row with the body translation row -- i.e. exactly rw::math::vpu::Mult
            // with the LOCATOR on the left.
            Matrix44Affine lPartWorldMatrix =
                rw::math::vpu::Mult( lpCarGraphicsSpec->GetPartLocators()[ luPartIdx ], lBodyTransform );

            // A part that has been knocked off the car carries its own world transform in the
            // detached-part queue; the queue entry's mbIsAttached then decides which of the two
            // draw paths runs. Parts with no queue entry stay attached (r28 seeds to 1).
            bool lbIsAttached = true;
            for ( s32 liEvent = 0; liEvent < lrDetachedParts.GetLength(); ++liEvent )
            {
                const DetachedPartRenderEvent& lrEvent = lrDetachedParts.GetEvent( liEvent );
                if ( lrEvent.miPartIndex == static_cast< s32 >( luPartIdx ) )
                {
                    lPartWorldMatrix = lrEvent.mTransform;
                    lbIsAttached     = lrEvent.mbIsAttached;
                    break;
                }
            }

            // An ATTACHED part is drawn only when the caller asked for the attached geometry
            // (the same bool that gates the glass and the wheels). A DETACHED part is always
            // drawn. Both paths converge on the submission below; what differs is the state of
            // shader constant 24, which the console arms for attached parts and zeroes for
            // detached ones (not reconstructed -- see the banner).
            if ( lbIsAttached && !lbRenderAttachedGeometry )
            {
                continue;
            }

            // Technique index. Two bits: bit 0 == "not damaged", bit 1 == "shadow pass".
            // (`v169 = damaged ? (shadow ? 3 : 0) : (shadow ? 2 : 1)`, the console spelling it
            // with _cntlzw.)
            u8 lu8Technique;
            if ( lbDamaged )
            {
                lu8Technique = lbShadowPass ? 3u : 0u;
            }
            else
            {
                lu8Technique = lbShadowPass ? 2u : 1u;
            }

            const CgsGraphics::Renderable* lpRenderable = lpModel->GetRenderable( leLOD );
            CGS_ASSERT( lpRenderable != 0, "Missing renderable in a model" );

            CgsGraphics::DispatchList* lpDispatchList = lpDispatchFrame->GetList( liObjectList );
            CGS_ASSERT( lpDispatchList != 0, "lpDispatchList" );

            // Bind the part's world matrix for this draw's shader constants.
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 0, lPartWorldMatrix );

            const bool lbFirstInList = ( lpDispatchList->GetCount() & 0x7F ) == 0;

            lpDispatchFrame->GetBin().BeginPacket();
            if ( lbShadowPass )
            {
                // Shadow variant (@0x822D0724..0x822D0764): both mesh lists are the OPAQUE one
                // and the draw is Z-only. The trailer bytes are preZList = 0xFF, preZTechnique 0,
                // instanceCount 0, excludeMeshBits = mu8RenderDamageFlags.
                CgsGraphics::DrawRenderable::AddToBin(
                    lpRenderable, lpDispatchFrame, lbFirstInList,
                    static_cast< s8 >( liOpaqueMeshList ), static_cast< s8 >( liOpaqueMeshList ),
                    1, lu8Technique, true,
                    0xFFu, 0u, 0, lpRenderParams->GetRenderDamageFlag() );
            }
            else
            {
                // Camera variant (@0x822D08E0..0x822D0924).
                CgsGraphics::DrawRenderable::AddToBin(
                    lpRenderable, lpDispatchFrame, lbFirstInList,
                    static_cast< s8 >( liOpaqueMeshList ), static_cast< s8 >( liTransparentMeshList ),
                    1, lu8Technique, false,
                    0xFFu, 0u, 0, lpRenderParams->GetRenderDamageFlag() );
            }

            lpDispatchList->Submit( 0, lpDispatchFrame->GetBin().EndPacket() );
        }
    }

    // NOT reconstructed (see the banner), in console order after the body-part loop:
    //   * `if (lbRenderAttachedGeometry)`  -- the cracked-glass loop, which walks the
    //     spec's shattered-glass part table and calls BrnWorld::SetGlassFractureConstants
    //     per pane before its own AddToBin/Submit;
    //   * `if (mbRenderWheels && lbRenderAttachedGeometry)` -- the wheel block, one
    //     instanced draw through Model::SetupShaderConstantsForInstancing.
}


// ============================================================================
// GenerateDispatchLists  @ 0x822E79F8  (the `else` arm -- see the banner)
//
//   for (i = 0; i < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i)
//   {
//       ActiveRaceCar& car = maActiveRaceCars[i];
//       if (!car.IsActive())                                       continue;
//       if (!car.mbRenderThisFrame || car.mbIsInCarSelectOnline)   continue;
//       distance = |cameraPosition - car.GetTransform().wAxis|;
//       gfx   = ResourcePtr(mRaceCarStreamer.GetGraphicsResource(i));
//       wheel = ResourcePtr(mRaceCarStreamer.GetWheelGraphicsResource(i));
//       RenderRaceCar(frame, &car.mRenderParams, &gfx, &wheel,
//                     objectList, opaqueMeshList, transparentMeshList,
//                     shadowMap, true, distance, fogScattering, fogColour);
//   }
//
// The console builds the two ResourcePtr locals with BaseResourcePtr::CreateFromHandle
// and unlinks them from the resource's intrusive user list right after the call (that is
// what the four pointer-fixup blocks after each RenderRaceCar call are); reproduced here
// as plain ResourcePtr copies, which carry the same acquire/release semantics without
// poking the list by hand.
// ============================================================================
void
RaceCarEntityModule::GenerateDispatchLists(
    RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists* lpInput,
    const Array< CgsSceneManager::EntityId, 32u >& lrVisibleEntities,
    s32  liObjectList,
    s32  liOpaqueMeshList,
    s32  liTransparentMeshList,
    bool lbEnvironmentMapPass,
    Vector4 lvFogScattering,
    Vector4 lvFogColourPlusWhiteLevel,
    Vector3 lvCameraPosition )
{
    CGS_ASSERT( lpInput != 0, "lpInput != NULL" );
    (void)lrVisibleEntities;    // consumed only by the scene-query arm (see the banner)
    (void)lbEnvironmentMapPass;

    lpInput->LockForRead();

    CgsGraphics::DispatchFrame* lpDispatchFrame = lpInput->GetDispatchFrame();
    const ShadowMap*            lpShadowMap     = lpInput->GetShadowMap();
    CGS_ASSERT( lpShadowMap != 0, "lpShadowMap" );

    if ( lpDispatchFrame != 0 && lpShadowMap != 0 )
    {
        for ( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
        {
            ActiveRaceCar& lrActiveRaceCar = maActiveRaceCars[ liCar ];

            if ( !lrActiveRaceCar.IsActive() )
            {
                continue;
            }
            if ( !lrActiveRaceCar.ShouldRenderThisFrame() )
            {
                continue;
            }

            // |camera - car|: the console's vmsum3fp128 / vrsqrtefp / Newton-step / vmulfp
            // chain over (cameraPosition - transform.wAxis), with the vcmpeqfp/vsel128
            // selecting zero when the squared length is exactly zero.
            const Matrix44Affine lCarTransform = lrActiveRaceCar.GetTransform();
            const f32 lfDeltaX   = lvCameraPosition.x - lCarTransform.wAxis.x;
            const f32 lfDeltaY   = lvCameraPosition.y - lCarTransform.wAxis.y;
            const f32 lfDeltaZ   = lvCameraPosition.z - lCarTransform.wAxis.z;
            const f32 lfLengthSq = lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ;
            const f32 lfDistance = ( lfLengthSq != 0.0f ) ? sqrtf( lfLengthSq ) : 0.0f;

            // The console COPIES both ResourcePtrs into stack locals here
            // (BaseResourcePtr::CreateFromHandle) and unlinks them from the resource's
            // intrusive alias list right after the call. That round trip only registers a
            // temporary alias; ResourcePtr's copy constructor has no reconstructed body in
            // this tree, so the read-only render use takes the streamer's own references.
            const RaceCarStreamer::GraphicsResourcePtr&      lrCarGraphics =
                mRaceCarStreamer.GetGraphicsResource( liCar );
            const RaceCarStreamer::WheelGraphicsResourcePtr& lrWheelGraphics =
                mRaceCarStreamer.GetWheelGraphicsResource( liCar );

            RenderRaceCar( lpDispatchFrame,
                           lrActiveRaceCar.GetRenderParams(),
                           &lrCarGraphics,
                           &lrWheelGraphics,
                           liObjectList,
                           liOpaqueMeshList,
                           liTransparentMeshList,
                           lpShadowMap,
                           true,
                           lfDistance,
                           lvFogScattering,
                           lvFogColourPlusWhiteLevel );
        }
    }

    lpInput->UnlockForRead();
}

// ============================================================================
// [FLAG PC bring-up] RenderStreamedCarBringUp -- NOT an X360 function.
//
// The console reaches RenderRaceCar through
//   RaceCarEntityModule::AttachActiveRaceCar @0x822F4DB0
//     -> ActiveRaceCar::Attach @0x822BEEE0             (muState = E_STATE_ATTACHED)
//     -> ActiveRaceCar::OnResourcesLoaded @0x822EB168   (the resources arrive)
//     -> the vehicle physics module fills mRenderParams every frame (mBodyTransform,
//        the part-visibility mask, the wheel transforms, mLOD)
//     -> GenerateDispatchLists above sees IsActive() && mbRenderThisFrame.
// On this build ALL SEVEN callers of AttachActiveRaceCar are absent, the vehicle physics
// module does not exist, and the streamer can never report IsRaceCarLoaded() because the
// attribute resource VEH_<id>_AT.bin is not ported and the wheel-graphics GameData handler
// is still deferred. So nothing sets muState, nothing sets mbRenderThisFrame, and nothing
// ever writes a body transform.
//
// This is the smallest honest stand-in for that chain: take the car whose BODY the
// streamer has actually delivered, pose it where the caller asks, and drive the REAL
// RenderRaceCar with a locally-owned RenderParams. Everything downstream -- the part loop,
// the locator maths, the shader constants, AddToBin, Submit, the dispatch interpreter and
// the D3D9 leaf -- is the reconstructed console path.
//
// DELETE this function (and its GenerateDispatchListsBringUp call site) once
// AttachActiveRaceCar and the vehicle physics publisher land.
// ============================================================================
void
RaceCarEntityModule::RenderStreamedCarBringUp( CgsGraphics::DispatchFrame* lpDispatchFrame,
                                               const ShadowMap* lpShadowMap,
                                               const Matrix44Affine& lrCarTransform,
                                               f32 lfCameraDistance,
                                               Vector4 lvFogScattering,
                                               Vector4 lvFogColourPlusWhiteLevel )
{
    if ( lpDispatchFrame == 0 || lpShadowMap == 0 )
    {
        return;
    }

    const s32 KI_BRING_UP_CAR = 0;   // the slot StreamFirstUnlockedCarBringUp requested

    if ( !mRaceCarStreamer.IsRaceCarActive( KI_BRING_UP_CAR ) ||
         !mRaceCarStreamer.IsGraphicsLoadedBringUp( KI_BRING_UP_CAR ) )
    {
        return;
    }

    // The render snapshot the physics side would own. Reset() is the console's own
    // just-spawned visual state (identity transforms, white paint, LOD 4, the authored part
    // mask); only the body pose is supplied from outside.
    static ActiveRaceCar::RenderParams sBringUpRenderParams;
    static bool sbBringUpRenderParamsReset = false;
    if ( !sbBringUpRenderParamsReset )
    {
        sbBringUpRenderParamsReset = true;
        sBringUpRenderParams.Reset();

        // Reset() seeds the console's authored 0xB80FFFFFFFF part mask, which leaves most of
        // the Cavalry's 24 body parts hidden. With no deformation/damage system to maintain
        // it, make the whole shell visible -- this is the console's own MakeAllPartsVisible.
        sBringUpRenderParams.MakeAllPartsVisible();

        // LOD 0 (Reset seeds state 4; the Cavalry's part models do not all carry that one).
        sBringUpRenderParams.SetLOD( CgsGraphics::Model::E_STATE_LOD_0 );
    }
    sBringUpRenderParams.SetBodyTransform( lrCarTransform );

    const RaceCarStreamer::GraphicsResourcePtr& lrCarGraphics =
        mRaceCarStreamer.GetGraphicsResourceBringUp( KI_BRING_UP_CAR );

    if ( CgsDev::Log::gpDebugPrint != 0 )
    {
        static bool sbLogged = false;
        if ( !sbLogged )
        {
            sbLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[racecar-render] bring-up producer live: car " << KI_BRING_UP_CAR
                << " parts "
                << static_cast< s32 >( lrCarGraphics.operator->()->muPartsCount )
                << " at (" << lrCarTransform.wAxis.x << ", " << lrCarTransform.wAxis.y
                << ", " << lrCarTransform.wAxis.z << ")\n";
        }
    }

    // The wheel resource is never bound on this build; RenderRaceCar's wheel block is not
    // reconstructed and never dereferences it (see the banner).
    static const RaceCarStreamer::WheelGraphicsResourcePtr sNullWheelGraphics;

    RenderRaceCar( lpDispatchFrame,
                   &sBringUpRenderParams,
                   &lrCarGraphics,
                   &sNullWheelGraphics,
                   KI_RACE_CAR_OBJECT_LIST,
                   KI_RACE_CAR_OPAQUE_MESH_LIST,
                   KI_RACE_CAR_TRANSPARENT_MESH_LIST,
                   lpShadowMap,
                   true,
                   lfCameraDistance,
                   lvFogScattering,
                   lvFogColourPlusWhiteLevel );
}

}
