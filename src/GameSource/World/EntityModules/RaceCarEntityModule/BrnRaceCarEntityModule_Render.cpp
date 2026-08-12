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
// 2. RenderRaceCar's CRACKED-GLASS loop and its shader constant 24. The glass loop
//    needs the spec's shattered-glass part table plus two .rodata constant vectors the
//    function-only IDA export does not carry; constant 24 (the brake/reverse light
//    emission vector) additionally needs three module debug floats at +100232..+100240
//    that are not named in this header's layout yet.
// 3. The IsNormal3x3 / IsOrthogonal3x3 / IsValid dev-assert blocks (~73% of the
//    function's 2008 instructions is assert scaffolding), including the wheel loop's
//    own rw::math::IsValid(lWheelMatrix).
//
// ---- THE WHEEL BLOCK IS NOW HERE, AND IT IS A FAITHFUL DECOMPILE ------------
// It submits ONE DrawRenderable command with instanceCount == the number of wheels,
// exactly as the console does. The PC's D3D9 back end cannot honour that in one draw
// (the fallback shader takes a single WVP per draw and the instanced draw leaf is a
// trap), so the N-instance command is expanded into N single-instance mesh commands in
// CgsGraphics::DrawRenderable::Interpret -- the same PC bring-up shim that already
// carries the per-object WVP there. The deviation belongs in that render back end, NOT
// in this function.
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
#include "SharedClasses/World/BrnWheelGraphicsSpec.h"                    // BrnWheel::GraphicsSpec
#include "rw/math/vpu/matrix44affine_operation.h"                        // rw::math::vpu::Mult

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint

#include <cmath>   // powf / sqrtf

// The global runtime shader-constant register (X360 symbol mShaderConstantTable; bodied
// by the CgsShaderConstants TU). Mirrors the committed extern in the sibling TUs.
namespace CgsGraphics { extern ::ShaderConstantTable mShaderConstantTable; }

// ============================================================================
// The two file-scope globals the wheel block reads. Both are console globals, and
// both were recovered from the image rather than guessed.
// ============================================================================

// dword_82CDB4A0. NAMED, not inferred: RaceCarEntityModuleDebugComponent::OnActivate
// @0x822C2538 registers it as the debug variable "Graphics/Vehicles.../Wheels to
// Render" with range [0, 4]. The shipped image holds 4. RenderRaceCar re-reads it on
// every loop iteration (a debug slider can move under the loop) and
// TrafficEntityModule::RenderTrafficCar @0x82728B08 shares it, which is why it is a
// plain global here and not a file-static.
s32 giWheelsToRender = 4;

// unk_82FAD6F0. The wheel-spin blur reference: constant 25's lane x is
// angularVelocity / gvWheelBlurConstants.x clamped to 1, and the same value picks the
// blurred-vs-static wheel technique.
// ⚠️ MEASURED: all 16 bytes read ZERO in the shipped image and a whole-export scan finds
// exactly ONE reference -- RenderRaceCar's own read. So on the console the divide is by
// zero, the quotient is +inf for any spinning wheel, and the vminfp clamp pins the
// constant at 1.0f; a stationary wheel gives 0/0. The PC keeps the console arithmetic
// (x87/SSE produce the same inf/NaN without trapping, and the fallback shader does not
// sample constant 25) rather than inventing a divisor. Named, zero-initialised, and
// left for the day a writer turns up -- the export set is known to have holes.
Vector4 gvWheelBlurConstants = { 0.0f, 0.0f, 0.0f, 0.0f };

// The console's wheel matrix / pointer stack arrays are four entries long
// (`__vector_constructor_iterator(v390, 64, 4, ...)`), which is also the debug
// variable's upper bound.
static const s32 KU_WHEELS_TO_RENDER_MAX = 4;

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

    // [PC diagnostic] print the value at the CONSUMING end. UpdateActiveRaceCarColours
    // prints the same two vectors where it writes them; this prints what actually reaches
    // shader constant 20/21, so a silent drop between the two shows up as a mismatch rather
    // than as a plausible-looking white car. (The existing [WorldShader] / [WorldSamplers]
    // tallies cannot answer this -- both saturate at 4096 draws, long before a car exists.)
    {
        // Latched on the VALUE, not on a "printed once" bool -- the paint starts at the
        // palette-0/colour-0 fallback and only becomes the car's authored colour once game
        // action 79 (CarSelectChangeColourAction) has been processed, several frames later.
        static f32 sfLastLoggedPaintX = -1.0f;
        static f32 sfLastLoggedPaintY = -1.0f;
        static f32 sfLastLoggedPaintZ = -1.0f;
        const Vector4& lrPaintProbe = lpRenderParams->GetPaintColour();
        if( ( lrPaintProbe.x != sfLastLoggedPaintX
              || lrPaintProbe.y != sfLastLoggedPaintY
              || lrPaintProbe.z != sfLastLoggedPaintZ )
            && CgsDev::Log::gpDebugPrint != 0 )
        {
            sfLastLoggedPaintX = lrPaintProbe.x;
            sfLastLoggedPaintY = lrPaintProbe.y;
            sfLastLoggedPaintZ = lrPaintProbe.z;
            const Vector4& lrPaint = lpRenderParams->GetPaintColour();
            const Vector4& lrPearl = lpRenderParams->GetPearlescentColour();
            *CgsDev::Log::gpDebugPrint
                << "[racecar-paint] RenderRaceCar -> shader constant 20 g_paintColour ("
                << lrPaint.x << ", " << lrPaint.y << ", " << lrPaint.z << ", " << lrPaint.w
                << ") / 21 g_pearlescentColour (" << lrPearl.x << ", " << lrPearl.y << ", "
                << lrPearl.z << ", " << lrPearl.w << ")\n";
        }
    }

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

    // ---- [DIAG carrender wave 2026-08-12] THE PER-PART WITNESS -----------------
    // ⛔ DELETE-WHEN the wheels + panels are confirmed on a booted run.
    // Answers, in ONE line per part, the two questions the frame poses: "where does a body
    // part's world matrix come from?" and "is anything flung?". It re-walks the spec
    // read-only -- it cannot perturb the loop above.
    // Latched on the number of parts sitting further than 3 m from the body origin (plus the
    // detached-queue length), NOT on a "printed once" bool: a run where nothing is flung and
    // a run where six panels are flung print different lines, and a change mid-run reprints.
    if ( mbRenderCarsDuringCrash && CgsDev::Log::gpDebugPrint != 0 )
    {
        const ActiveRaceCar::RenderParams::DetachedPartRenderQueue& lrDiagQueue =
            lpRenderParams->GetDetachedPartQueue();

        s32 liFarParts = 0;
        f32 lfMaxDist  = 0.0f;
        for ( u32 luDiag = 0; luDiag < lpCarGraphicsSpec->muPartsCount; ++luDiag )
        {
            const Matrix44Affine lDiagWorld =
                rw::math::vpu::Mult( lpCarGraphicsSpec->GetPartLocators()[ luDiag ], lBodyTransform );
            const f32 lfDX = lDiagWorld.wAxis.x - lBodyTransform.wAxis.x;
            const f32 lfDY = lDiagWorld.wAxis.y - lBodyTransform.wAxis.y;
            const f32 lfDZ = lDiagWorld.wAxis.z - lBodyTransform.wAxis.z;
            const f32 lfD  = sqrtf( lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ );
            if ( lfD > lfMaxDist ) { lfMaxDist = lfD; }
            if ( lfD > 3.0f )      { ++liFarParts; }
        }

        // The Y-OVER-TIME probe (observable (d)): one compact line per CENTIMETRE of body
        // travel, budgeted so a settling car prints its whole descent and a settled one goes
        // quiet. This is what proves a render change did not disturb the physics.
        {
            static s32 siLastBodyYcm = 0x7FFFFFFF;
            static s32 siYBudget     = 60;
            const s32  liBodyYcm     = static_cast< s32 >( lBodyTransform.wAxis.y * 100.0f );
            if ( liBodyYcm != siLastBodyYcm && siYBudget > 0 )
            {
                siLastBodyYcm = liBodyYcm;
                --siYBudget;
                *CgsDev::Log::gpDebugPrint
                    << "[carrender-y] bodyDraw y " << lBodyTransform.wAxis.y
                    << " wheel0 exists " << ( lpRenderParams->GetWheelExists( 0u ) ? 1 : 0 )
                    << " wheel0 y " << lpRenderParams->GetWheelTransform( 0u ).wAxis.y
                    << " archPartWorldY " << rw::math::vpu::Mult(
                           lpCarGraphicsSpec->GetPartLocators()[ 3 ], lBodyTransform ).wAxis.y
                    << "\n";
            }
        }

        static s32 siLastFarParts   = -1;
        static s32 siLastQueueLen   = -1;
        if ( liFarParts != siLastFarParts || lrDiagQueue.GetLength() != siLastQueueLen )
        {
            siLastFarParts = liFarParts;
            siLastQueueLen = lrDiagQueue.GetLength();

            *CgsDev::Log::gpDebugPrint
                << "[carrender] parts " << lpCarGraphicsSpec->muPartsCount
                << " sizeof(locator) " << static_cast< s32 >( sizeof( Matrix44Affine ) )
                << " detachedQueueLen " << lrDiagQueue.GetLength()
                << " farParts(>3m) " << liFarParts
                << " maxPartDist " << lfMaxDist
                << " body (" << lBodyTransform.wAxis.x << ", " << lBodyTransform.wAxis.y
                << ", " << lBodyTransform.wAxis.z << ")\n";

            for ( u32 luDiag = 0; luDiag < lpCarGraphicsSpec->muPartsCount; ++luDiag )
            {
                const Matrix44Affine& lrLoc = lpCarGraphicsSpec->GetPartLocators()[ luDiag ];
                const Matrix44Affine  lDiagWorld = rw::math::vpu::Mult( lrLoc, lBodyTransform );
                const CgsGraphics::Model* lpDiagModel = lpCarGraphicsSpec->GetPartModel( luDiag );

                // did the queue override this part?
                s32 liQueueHit = -1;
                for ( s32 liEv = 0; liEv < lrDiagQueue.GetLength(); ++liEv )
                {
                    if ( lrDiagQueue.GetEvent( liEv ).miPartIndex == static_cast< s32 >( luDiag ) )
                    {
                        liQueueHit = liEv;
                        break;
                    }
                }

                *CgsDev::Log::gpDebugPrint
                    << "[carrender]  part " << static_cast< s32 >( luDiag )
                    << " vis " << ( lpRenderParams->IsPartVisible( static_cast< u8 >( luDiag ) ) ? 1 : 0 )
                    << " model " << ( lpDiagModel != 0 ? 1 : 0 )
                    << " locT (" << lrLoc.wAxis.x << ", " << lrLoc.wAxis.y << ", " << lrLoc.wAxis.z
                    << ") locScaleRow0 (" << lrLoc.xAxis.x << ", " << lrLoc.xAxis.y << ", "
                    << lrLoc.xAxis.z << ")"
                    << " world (" << lDiagWorld.wAxis.x << ", " << lDiagWorld.wAxis.y << ", "
                    << lDiagWorld.wAxis.z << ") queueHit " << liQueueHit << "\n";
            }

            // The wheel half of the same witness: what the wheel block will read.
            for ( u32 luDiagW = 0; luDiagW < 4u; ++luDiagW )
            {
                const Matrix44Affine& lrWT = lpRenderParams->GetWheelTransform( luDiagW );
                const Matrix44Affine& lrWS = lpRenderParams->GetWheelScaleMatrix( luDiagW );
                *CgsDev::Log::gpDebugPrint
                    << "[carrender]  wheel " << static_cast< s32 >( luDiagW )
                    << " exists " << ( lpRenderParams->GetWheelExists( luDiagW ) ? 1 : 0 )
                    << " T (" << lrWT.wAxis.x << ", " << lrWT.wAxis.y << ", " << lrWT.wAxis.z
                    << ") scaleDiag (" << lrWS.xAxis.x << ", " << lrWS.yAxis.y << ", "
                    << lrWS.zAxis.z << ")\n";
            }
        }
    }
    // ---- end [DIAG carrender wave] --------------------------------------------

    // NOT reconstructed (see the banner), in console order after the body-part loop:
    //   * `if (lbRenderAttachedGeometry)`  -- the cracked-glass loop, which walks the
    //     spec's shattered-glass part table and calls BrnWorld::SetGlassFractureConstants
    //     per pane before its own AddToBin/Submit.

    // ---- the WHEEL block  (@0x822D0F60..0x822D15BC) -------------------------
    // `if (*(this + 99148) && a42)` -- the module's wheel switch and the caller's
    // "render attached geometry" bool. One INSTANCED draw for all four wheels: the
    // per-wheel world matrices go up as shader constant 6 and the per-wheel spin
    // constants as 7, then a single DrawRenderable command carries the instance count.

    // [DIAG wheel wave] The outcome code for the block below, reported once per DISTINCT
    // value at the end. Latched on the VALUE, never on a "printed once" bool: this block
    // has five different ways to draw nothing and a "did it run" flag could only ever
    // report the first car's answer.
    //   0 no wheel resource / block skipped   1 spec has no wheel model
    //   2 no LOD state on the model           3 no wheel reported as existing
    //   4..8 SUBMITTED with 1..5 instances
    s32 liWheelDiagCode = 0;

    if ( mbRenderWheels && lbRenderAttachedGeometry && lpWheelGraphics->HasMemoryResource() )
    {
        // [FLAG PC boot gate] `HasMemoryResource()` is NOT console. The console reaches
        // this block only for a car whose whole resource set is in (its own
        // RaceCarStreamer::GetWheelGraphicsResource asserts IsRaceCarLoaded() before
        // handing the pointer over), so it dereferences unguarded. This build renders
        // cars with PARTIAL sets through the BringUp getters -- see GenerateDispatchLists
        // -- and a car really can go E_STATE_ACTIVE before its wheel graphics reply
        // lands (measured: car 2 is placed on track and drawn several frames before
        // "STRM: Wheel graphics loaded: 2"). Without the gate that car fires
        // ResourcePtr::operator->'s "Can not instance resource pointer" assert on every
        // frame of the render walk. DELETE with the BringUp getters.
        const BrnWheel::GraphicsSpec* lpWheelGraphicsSpec = lpWheelGraphics->operator->();

        // `v230 = *(v229 + 4)` -- the spec's wheel model (its calliper twin at +8 is
        // not drawn by this block).
        const CgsGraphics::Model* lpWheelModel = lpWheelGraphicsSpec->GetWheelModel();

        // The wheels never draw at LOD 0: `if (v231 <= 1) v231 = 1`.
        CgsGraphics::Model::State leWheelLOD = lpRenderParams->GetLOD();
        if ( leWheelLOD <= CgsGraphics::Model::E_STATE_LOD_1 )
        {
            leWheelLOD = CgsGraphics::Model::E_STATE_LOD_1;
        }

        liWheelDiagCode = 1;

        if ( lpWheelModel != 0 )
        {
            // Fall back to LOD 1 when the requested state is absent, then bail if even
            // that is missing (the console calls DoesStateExist twice, in that order).
            if ( !lpWheelModel->DoesStateExist( leWheelLOD ) )
            {
                leWheelLOD = CgsGraphics::Model::E_STATE_LOD_1;
            }

            liWheelDiagCode = 2;

            if ( lpWheelModel->DoesStateExist( leWheelLOD ) )
            {
                liWheelDiagCode = 3;
                // The three console stack arrays. maWheelMatrices / mapWheelMatrices are
                // four entries because the loop bound tops out at four; the CONSTANTS
                // array is sized to what the callee reads (see the note on
                // KU_MAX_INSTANCES_PER_GROUP below).
                Matrix44Affine        laWheelMatrices[ KU_WHEELS_TO_RENDER_MAX ];
                const Matrix44Affine* lapWheelMatrices[ KU_WHEELS_TO_RENDER_MAX ] = { 0, 0, 0, 0 };

                // ⚠️ The console declares this array FOUR entries long
                // (`__vector_constructor_iterator(v389, 16, 4, ...)`) but
                // SetupShaderConstantsForInstancing reads FIVE
                // (KU_MAX_INSTANCES_PER_GROUP) -- a 16-byte read off the end of the
                // caller's stack slot. Sized to the callee's contract and zeroed, which
                // changes no value the console produces (the loop can never fill a fifth
                // entry) and removes the overread.
                Vector4 laWheelConstants[ CgsGraphics::Model::KU_MAX_INSTANCES_PER_GROUP ] =
                    { { 0.f, 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 0.f },
                      { 0.f, 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 0.f } };

                s32 liInstanceCount = 0;

                // Technique. Seeded 0 (the spinning/blurred variant) OUTSIDE the loop and
                // only ever raised, so it is sticky across wheels: 1 as soon as any wheel
                // is turning slower than the blur threshold, 2 for the shadow pass.
                u8 lu8Technique = 0;

                for ( s32 liWheel = 0; liWheel < giWheelsToRender; ++liWheel )
                {
                    if ( !lpRenderParams->GetWheelExists( static_cast< u32 >( liWheel ) ) )
                    {
                        continue;
                    }

                    // ⚠️ GetWheelTransform is ALREADY a world transform -- unlike the body
                    // parts, the body matrix is NOT re-applied here. The only composition is
                    // the per-wheel scale matrix on the LEFT:
                    //   row i = S[i].x*T[0] + S[i].y*T[1] + S[i].z*T[2] (+ T[3] on row 3)
                    // (the vmulfp128/vmaddfp chain @0x822D10D0..0x822D1134; `vmaddfp` prints
                    // as vD,vA,vB,vC and computes vA*vC+vB, which is what makes this
                    // Mult(scale, transform) and not the other order).
                    const Matrix44Affine lWheelMatrix = rw::math::vpu::Mult(
                        lpRenderParams->GetWheelScaleMatrix( static_cast< u32 >( liWheel ) ),
                        lpRenderParams->GetWheelTransform( static_cast< u32 >( liWheel ) ) );

                    // Shader constant 25, "g_wheelConstants": the wheel's spin blur factor
                    // in lane x and nothing else. `vrlimi128 v0, v13, 8, 0` -- mask 8 is
                    // lane X, and the vector it merges into (v123) is `vspltisw128 0`, so
                    // yzw are ZERO. (The previous wave's decode had the lanes the other way
                    // round; the mask bit settles it, and the fog block eight lines up uses
                    // mask 1 == lane w for contrast.)
                    const f32 lfSpin =
                        lpRenderParams->GetWheelAngularVelocity( static_cast< u32 >( liWheel ) )
                        / gvWheelBlurConstants.x;
                    Vector4 lv4WheelConstants = { lfSpin, 0.0f, 0.0f, 0.0f };
                    if ( lv4WheelConstants.x > 1.0f )   // vminfp128 against vcsxwfp(1) == 1.0f
                    {
                        lv4WheelConstants.x = 1.0f;
                    }
                    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 25, lv4WheelConstants );

                    // `vcmpgtfp. v0, v0, v13` + the CR6 "all lanes" bit: a wheel turning
                    // slower than the threshold selects the non-blurred technique.
                    if ( gvWheelBlurConstants.x >
                         lpRenderParams->GetWheelAngularVelocity( static_cast< u32 >( liWheel ) ) )
                    {
                        lu8Technique = 1u;
                    }
                    if ( lbShadowPass )
                    {
                        lu8Technique = 2u;
                    }

                    // (The four rw::math::IsValid(lWheelMatrix) dev-assert blocks that follow
                    // in the console are elided, exactly as the body-part loop's
                    // IsNormal3x3/IsOrthogonal3x3 blocks are -- see the banner.)

                    // Everything is appended at the COMPACTED index, not at liWheel: the
                    // console advances r28/r26/v237 only when the wheel exists.
                    laWheelMatrices[ liInstanceCount ]   = lWheelMatrix;
                    laWheelConstants[ liInstanceCount ]  = lv4WheelConstants;
                    lapWheelMatrices[ liInstanceCount ]  = &laWheelMatrices[ liInstanceCount ];
                    ++liInstanceCount;
                }

                if ( liInstanceCount > 0 )
                {
                    liWheelDiagCode = 3 + liInstanceCount;

                    // ================================================================
                    // ✅ THE GATE NOW PASSES (2026-08-04, task 133). READ THIS FIRST -- the
                    // history below is kept because its warning still stands, but its
                    // conclusion ("this is a DATA gap ... fix the wheel model") was WRONG.
                    //
                    // The flag was never authored on disc in ANY build (X360 retail, our port
                    // and the BPR Remaster all carry mu8Flags == 0x00). It is COMPUTED AT LOAD
                    // by CgsResource::ModelResourceType::PostFixUp @0x828A7A68 -- an override
                    // b5-decomp simply never declared, so it bound to the empty base and the
                    // pass ran, did nothing and reported nothing. Declaring and implementing it
                    // re-opened this gate by itself, with no change here.
                    //
                    // The "spike through the roof" was NOT the wheel geometry being undrawable.
                    // Device::DrawIndexedMeshPC was submitting the FULL index range of a mesh
                    // whose index buffer holds mu8InstanceCount (=5) slices with the instance
                    // number in the high bits of every index -- so 4/5 of the index values
                    // pointed outside the 240-vertex vertex buffer. It now submits slice 0.
                    // See the banner on Device::DrawIndexedMeshPC.
                    //
                    // ⚠️ WHAT IS STILL MISSING, MEASURED (task 133): the wheels draw, at the
                    // four correct world positions, with correct geometry and an in-range index
                    // run -- and are INVISIBLE, because they are 0.74 m UNDERGROUND.
                    // PlaceCarOnTrack seats the car's ORIGIN on the collision surface
                    // (y = -3.525 in the junkyard, BrnPlaceOnTrackManager's own measured
                    // number) and nothing lifts the body by the wheel radius, because nothing
                    // simulates the suspension. Wheel centre = origin - 0.409 (authored
                    // WheelSpec), wheel radius = 0.5 * 0.663 = 0.3315 -> the tyre bottom is
                    // 0.7405 below the ground plane. Proven by drawing the instanced meshes
                    // with the depth test defeated (BRN_WHEEL_ZALWAYS=1): all four wheels
                    // appear, correctly placed and shaped, through the sand.
                    // DELETE-WHEN the suspension settle lands (the vehicle-physics wall).
                    // ================================================================
                    //
                    // ---- history (task 118) ----------------------------------------
                    // ⛔ THE CONSOLE'S OWN GATE, RE-INSTATED (2026-08-03, task 118).
                    //
                    // The console's check here is
                    //   CGS_ASSERT(lpWheelModel->GetFlag(E_FLAG_MODEL_USES_INSTANCE_SHADER),
                    //              "lpWheelModel->GetFlag(...)")
                    // and on the ported WHE_51916650_GR wheel model it FAILS: the model's
                    // mu8Flags (Model +0x11) reads 0, so bit 0 is clear. It is not a scrambled
                    // header -- mu8NumStates (+0x12) and mu8NumRenderables (+0x10) in the same
                    // word both read correctly (DoesStateExist and GetRenderable succeed on
                    // this very model), which a byte-reversed word could not do.
                    //
                    // ⚠️⚠️ The wheel wave downgraded that assert to a one-shot log on the
                    // reasoning that it "is a NON-GATING assert on the console ... and it has
                    // no effect on this build at all". THAT REASONING WAS WRONG IN EFFECT, and
                    // submitting the draw anyway is what destroyed the rendered world for a
                    // full day. MEASURED, not inferred (task 118, dumped frames, bisected to
                    // this commit):
                    //   * with the draw submitted as authored (instanceCount == 4) the frame is
                    //     screen-filling dark shards from the junkyard chase camera;
                    //   * with the SAME draw submitted at instanceCount == 1 -- i.e. one copy,
                    //     under shader constant 0, the car body's own KNOWN-GOOD world matrix
                    //     that renders the bodyshell correctly in the same frame -- this
                    //     renderable draws as a single distorted SPIKE through the car's roof.
                    //     So the matrices are not the fault: THE WHEEL RENDERABLE'S GEOMETRY
                    //     IS NOT DRAWABLE AS LOADED. Instancing it four times just turns one
                    //     spike into four screen-filling ones.
                    //   * a DrawRenderable::Interpret probe over every expansion candidate in a
                    //     whole run shows this renderable (7 meshes, bounding sphere r 0.837,
                    //     lists 19/20) is the ONLY object ever expanded -- no world object is
                    //     touched, so the instancing shim itself is behaving.
                    //
                    // So the console's flag is doing exactly the job it exists for: it says
                    // "this model is fit for the instanced wheel path", and on this data it is
                    // not. Honour it. This is a DATA gap, not a code one -- the moment the
                    // ported wheel model carries the flag the wheels submit again with no
                    // further change here, and the frame is the check that proves it.
                    // ⛔ Do NOT re-enable this draw by deleting the gate; fix the wheel model.
                    // ================================================================
                    if ( !lpWheelModel->GetFlag( CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER ) )
                    {
                        liWheelDiagCode = 9;   // "gated: model not fit for the instanced path"
                        static bool sbLoggedInstanceShaderFlag = false;
                        if ( !sbLoggedInstanceShaderFlag && CgsDev::Log::gpDebugPrint != 0 )
                        {
                            sbLoggedInstanceShaderFlag = true;
                            *CgsDev::Log::gpDebugPrint
                                << "[racecar-wheels] WHEEL DRAW GATED OFF: model does not carry"
                                   " E_FLAG_MODEL_USES_INSTANCE_SHADER (the console asserts here);"
                                   " renderables " << lpWheelModel->GetNumRenderables()
                                << " states " << lpWheelModel->GetNumLods()
                                << " version " << lpWheelModel->GetVersionNumber()
                                << " -- its geometry draws as spikes, see the banner"
                                   " [FLAG PC data gap]\n";
                        }
                    }
                    else
                    {
                    const CgsGraphics::Renderable* lpWheelRenderable =
                        lpWheelModel->GetRenderable( leWheelLOD );

                    CgsGraphics::DispatchList* lpWheelList = lpDispatchFrame->GetList( liObjectList );
                    CGS_ASSERT( lpWheelList != 0, "lpDispatchList" );

                    // Publish constants 6 (the instance matrices) and 7 (the per-instance
                    // spin vectors) BEFORE the packet opens -- AddToBin drains whatever the
                    // constant table has marked dirty at that moment.
                    CgsGraphics::Model::SetupShaderConstantsForInstancing(
                        liInstanceCount, lapWheelMatrices, laWheelConstants );

                    const bool lbFirstInList = ( lpWheelList->GetCount() & 0x7F ) == 0;

                    lpDispatchFrame->GetBin().BeginPacket();
                    // @0x822D1584: frustumEnable 0 (the wheels ride inside the body's own
                    // bounds), preZList 0xFF, preZTechnique 0, excludeMeshBits 0, and the
                    // instance count that makes this ONE command draw all the wheels.
                    CgsGraphics::DrawRenderable::AddToBin(
                        lpWheelRenderable, lpDispatchFrame, lbFirstInList,
                        static_cast< s8 >( liOpaqueMeshList ), static_cast< s8 >( liTransparentMeshList ),
                        0, lu8Technique, lbShadowPass,
                        0xFFu, 0u, liInstanceCount, 0u );

                    lpWheelList->Submit( 0, lpDispatchFrame->GetBin().EndPacket() );
                    }
                }
            }
        }
    }

    // [DIAG wheel wave] one line per DISTINCT outcome (a bitmask of codes already seen,
    // so a car that draws wheels and a car whose wheel resource has not arrived each get
    // reported exactly once instead of every frame for ever).
    {
        static u32 suSeenWheelDiagCodes = 0;
        const u32  luBit = 1u << static_cast< u32 >( liWheelDiagCode );
        if ( ( suSeenWheelDiagCodes & luBit ) == 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            suSeenWheelDiagCodes |= luBit;
            *CgsDev::Log::gpDebugPrint
                << "[racecar-wheels] RenderRaceCar wheel block outcome " << liWheelDiagCode
                << " (0 no resource, 1 no model, 2 no LOD state, 3 no wheel exists, "
                   "4..8 submitted with 1..5 instances, 9 GATED -- model lacks "
                   "E_FLAG_MODEL_USES_INSTANCE_SHADER); wheels-to-render "
                << giWheelsToRender << "\n";
        }
    }
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
            //
            // [FLAG PC bring-up] ...but through the BringUp getters, not the console's.
            // GetGraphicsResource / GetWheelGraphicsResource both assert
            // IsRaceCarLoaded(), i.e. ALL FIVE resource bits, and on this build two of
            // them (physics + attributes) come from the unported VEH_<id>_AT.bin and a
            // third (wheel graphics) from a still-deferred GameData handler -- so the
            // console's own accessors fire a dev assert every single frame for a car that
            // is otherwise perfectly renderable. The BringUp pair returns the same member
            // without the all-resources precondition. RESTORE the console accessors the
            // moment AddVehicleData's attribute + wheel legs resolve.
            const RaceCarStreamer::GraphicsResourcePtr&      lrCarGraphics =
                mRaceCarStreamer.GetGraphicsResourceBringUp( liCar );
            const RaceCarStreamer::WheelGraphicsResourcePtr& lrWheelGraphics =
                mRaceCarStreamer.GetWheelGraphicsResourceBringUp( liCar );

            // RenderRaceCar Submits into the OBJECT list (12); 19/20 are mesh-bin routing
            // ids that the dispatch interpreter fans the packet out to, not lists this
            // producer appends to directly.
            const s32 liBefore = lpDispatchFrame->GetList( liObjectList )->GetCount();

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

            // [DIAG pose wave] one-shot proof that the console leg reached the submission
            // leaf, and with what pose. Cheap enough to leave in: it fires once per boot.
            static bool sbLoggedFirstSubmission = false;
            if ( !sbLoggedFirstSubmission && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedFirstSubmission = true;
                const Matrix44Affine& lrBody =
                    lrActiveRaceCar.GetRenderParams()->GetBodyTransform();
                *CgsDev::Log::gpDebugPrint
                    << "[racecar-render] GenerateDispatchLists -> RenderRaceCar: car "
                    << liCar << " at (" << lrBody.wAxis.x << ", " << lrBody.wAxis.y
                    << ", " << lrBody.wAxis.z << ") dist " << lfDistance
                    << " object list " << liObjectList << " " << liBefore << " -> "
                    << lpDispatchFrame->GetList( liObjectList )->GetCount()
                    << " (mesh bins " << liOpaqueMeshList << "/"
                    << liTransparentMeshList << ")\n";
            }
        }
    }

    lpInput->UnlockForRead();
}

}
