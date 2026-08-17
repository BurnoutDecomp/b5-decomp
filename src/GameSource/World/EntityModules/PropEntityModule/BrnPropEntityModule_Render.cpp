// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/
//   BrnPropEntityModule_Render.cpp
//
// The prop-entity RENDER leg, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnWorld::PropEntityModule::GenerateDispatchLists  @ 0x822FB4F0  (774 insns)
//   BrnWorld::PropEntityModule::RenderPropAndCoronas   @ 0x822DC010  (187 insns)
//   BrnWorld::PropEntityModule::RenderModel            @ 0x822C4918  (620 insns)
//
// This is the consumer of WorldModule::FilterFrustumTestResults' maPropEntityIds
// (owner 3 = a whole prop, owner 34 = a replay prop/part). Everything downstream
// already works: props submit to GDL OBJECT list 11, BrnRendererModule::
// ConvertObjectsToMeshes expands that into mesh lists 11/15 and RenderWorldPasses
// dispatches them.
//
// ---- THE CALLING CONVENTION, PINNED FROM THE ASM ---------------------------
// All three signatures were re-derived from the X360 parameter-save area rather
// than trusted from Hex-Rays (which renders GenerateDispatchLists as 37 ints and
// RenderPropAndCoronas as 21). The rule that makes the three consistent, read off
// the homing stores in GenerateDispatchLists' prologue (r4->arg_1C, r5->arg_24,
// r6->arg_2C, r8->arg_4C, r9->arg_54, r10->arg_5C) plus the caller/callee stack
// offsets that must agree (WorldModule::GenerateDispatchLists @0x827D295C writes
// arg_64 / arg_6F / arg_77; RenderModel reads arg_6C / arg_77 / arg_7F / arg_84):
//
//   * the parameter save area starts at incoming-SP + 0x14 and advances 8 bytes
//     per parameter;
//   * a __vector4 parameter is passed in v1 and reserves 16 bytes of save area but
//     consumes NO GPR;
//   * a float parameter is passed in f1 and DOES consume its GPR slot (the
//     PPC float-arg rule this project keeps getting bitten by);
//   * GPR assignment counts non-vector parameters only: r3..r10, then the stack.
//
// Applying it:
//   GenerateDispatchLists  this r3 | lpInput r4 | array r5 | viewProj r6 |
//                          cameraPos v1 | zoom f1 (skips r7) | shaderLod r8 |
//                          modelOnly r9 | opaque r10 | transparent @0x64 |
//                          lbRenderingEnvironmentMap @0x6C | lbRenderCoronas @0x74
//   RenderModel            this r3 | model r4 | transform r5 | frame r6 |
//                          viewProj r7 | cameraPos v1 | zoom f1 (skips r8) |
//                          modelOnly r9 | opaque r10 | transparent @0x6C |
//                          envMap @0x74 | zOnly @0x7C | shaderLod @0x84
//   RenderPropAndCoronas   this r3 | propGraphics r4 | transform r5 | typeId r6 |
//                          frame r7 | viewProj r8 | cameraPos v1 | zoom f1 (skips
//                          r9) | modelOnly r10 | opaque @0x64 | transparent @0x6C |
//                          envMap @0x74 | shaderLod @0x7C | renderCoronas @0x84 |
//                          VFXProp table @0x8C | zOnly @0x94 | coronas @0x9C
//
// The two TRAILING BOOLS on GenerateDispatchLists are REAL (the DecFIGS DWARF
// declares them too: BrnEntityModuleUnity.cpp:33003 names them
// lbRenderingEnvironmentMap / lbRenderCoronas). Proof from the two call sites in
// WorldModule::GenerateDispatchLists @0x827D1CE8:
//   main-view  @0x827D294C..0x827D2958 : @0x6F = r31(0), @0x77 = 1,  @0x64 = 15
//   env-map    @0x827D2C58..0x827D2C70 : @0x6F = 1,      @0x77 = r31(0), @0x64 = r28
// i.e. the main view renders coronas and is not the env map; the env-map face pass
// is the env map and renders no coronas. Dropping them collapsed the two passes.
//
// ---- WHAT IS PARKED (named, none of it faked) ------------------------------
// 1. THE REPLAY ARMS of GenerateDispatchLists (owner 34, and the three mbInReplay
//    gates on the live arm). They need BrnWorld::PropEntityModule::RenderReplayProp
//    (a separate X360 function, 14 parameters, not declared in the module header and
//    not in this slice), BrnReplays::PropSerialiserFrame::IsCellActive /
//    IsPropAddedToScene / GetPartTransform, and PropZoneManager's private
//    mauStartIndexOfZone[] to turn a global entity index into a zone-local one. A
//    one-shot log fires the first time a replay id reaches this function.
// 2. THE CORONA LOOP of RenderPropAndCoronas (the whole tail after the RenderModel
//    call). Fully decoded -- see the block comment at the call site -- but it walks
//    BrnParticle::VFXPropCollection / VFXProp / VFXPropState / VFXCoronaType /
//    VFXCoronaTypeData, and of those only VFXPropCollection even has a forward
//    declaration in the tree (SharedClasses/Graphics/VFXPropsResourceType.h holds
//    only the resource-type handler). Those are STREAMED SERIALISED records; minting
//    their host layouts from the DWARF outside this slice is exactly how a silent
//    on-disc/host stride mismatch gets in. Left for the TU that homes them.
// 3. THE OVERHEAD-SIGN REFRESH tail of GenerateDispatchLists (@0x822FBE20..
//    0x822FC0C8, gated on lbRenderCoronas, runs every 60 frames). It appends to
//    PropEntityModule::mVisibleOverheadSigns, which B1 could only model as
//    1052 opaque bytes because GuiOverheadSignInfoEvent::VisibleOverheadSignArray has
//    no committed home, and it calls BrnGui::OverheadSignScore<32>::IsFull/Grow, which
//    do not exist in the tree either.
// 4. THE DEBUG BOUNDING-CIRCLE tail of RenderModel (@0x822C50C4..0x822C52AC, gated
//    on mbDrawBoundingSpheres, which Construct clears). Its two file-scope gates,
//    byte_82CDB6B0 and flt_82CDB6B4, have exactly ONE cross-reference each in the
//    whole image -- RenderModel's own read -- so neither initialiser is recoverable
//    from a function-only export, and the distance threshold cannot be invented. The
//    five LOD colours it uses ARE recovered (they match the world module's
//    KAU_LOD_COLOURS byte for byte) and are recorded in the comment at the site.
//
// ---- Model::RenderModelOnly ------------------------------------------------
// RenderModel is one of the five sites the X360 inlines Model::RenderModelOnly into
// (the asserts carry CgsModel.h lines 399/400/403/404/409/410/412; the DWARF declares
// it at CgsModel.h:158 with 16 parameters). There is no out-of-line symbol anywhere in
// the image. Following A5's precedent in CgsModel.cpp's FlushInstanceCollection, the
// expansion is reproduced VERBATIM at its site rather than synthesised into a general
// member. A joint de-inlining pass now looks WORTHWHILE and cheap: this file's two
// expansions (the Z-only arm and the camera arm) plus WorldEntityModule::RenderInstance
// already agree on the same twelve-step shape -- GetRenderable / assert / GetList /
// GetBin / SetShaderConstantData(0, transform) / (count % 128 == 0) / BeginPacket /
// AddToBin / EndPacket / Submit -- and differ only in the technique byte, the two list
// ids and the trailer. Four of the five sites are now written; the fifth
// (TrafficEntityModule::RenderTrafficCar @0x82728B08) is the only one still unread.
// ============================================================================

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

#include "GameSource/World/BrnEntityTypes.h"                              // E_ENTITYTYPE_PROP
#include "GameSource/World/BrnShaderLodInfo.h"                            // ShaderLodInfo
#include "GameSource/World/ShadowMap/BrnShadowMap.h"                      // BrnWorld::ShadowMap

#include "SharedClasses/Physics/Props/BrnPropGraphicsList.h"              // PropGraphics / PropPartGraphics
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"         // PropPhysicsDataHeader::GetType
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"           // PropTypeData::GetNumberOfParts

#include "GameShared/GameClasses/Graphics/CgsModel.h"                     // Model / Renderable
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"           // ShaderConstantTable
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"       // DispatchFrame / Bin / List
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h" // DrawRenderable::AddToBin
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint

#include "rw/math/vpu/vector3_operation.h"                                // MagnitudeSquared

#include <cstddef>   // offsetof (the layout tripwire below)

// The global runtime shader-constant register (X360 symbol mShaderConstantTable; bodied by
// the CgsShaderConstants TU). Mirrors the committed extern in the sibling render TUs.
namespace CgsGraphics { extern ::ShaderConstantTable mShaderConstantTable; }

// The X360 tests the visible id's owner against 3 AND 34. BrnEntityTypes.h's enum names
// E_ENTITYTYPE_FIRST_REPLAY_TYPE(32) and E_ENTITYTYPE_REPLAY_RACECAR(33) and then stops at
// E_ENTITYTYPE_COUNT(34), so 34 -- the REPLAY PROP owner -- is unnamed there.
// PropEntityModule::LeaveReplay @0x822C4810 pins it: it removes scene entities
// `(i << 10) | 0x22000000` (replay props) and `(i << 10) | 0x22000001` (replay parts).
// Named locally rather than editing the shared enum from this slice.
static const u8 KU8_ENTITYTYPE_REPLAY_PROP = 34;

namespace BrnWorld
{

// =============================================================================
// Layout tripwires for the ONE structural assumption this TU makes (never called).
// It is a file-scope static, not a PropEntityModule member: the class already has an
// _AssertLayout() declared and defined in B1's BrnPropEntityModule.cpp, and a second
// definition would be an ODR violation.
//
// The assumption: the X360 hands RenderPropAndCoronas / RenderModel the raw
// PropEntityInstance* / PropPartEntityInstance* where a `const Matrix44Affine*` is
// expected (@0x822FB9B8 `mr r5,r29` and @0x822FBAE4 `lvx128 v0,r0,r31`). That is only
// legal because the world transform is the FIRST member of both. This TU spells the
// argument &instance->mWorldTransform, so the two stay equivalent -- these asserts are
// what would fail loudly if a future layout change broke that equivalence and someone
// reintroduced the pointer-punning form.
// =============================================================================
static void _AssertRenderLayout()
{
    static_assert( offsetof( PropEntityInstance, mWorldTransform ) == 0,
                   "PropEntityInstance::mWorldTransform must be the first member" );
    static_assert( offsetof( PropPartEntityInstance, mWorldTransform ) == 0,
                   "PropPartEntityInstance::mWorldTransform must be the first member" );
}

// =============================================================================
// RenderModel  @ 0x822C4918  (BrnPropEntityModule.cpp:2425 in the DWARF)
//
// Distance-cull, pick the LOD state, then either buffer the model for instanced
// collection or expand Model::RenderModelOnly inline and stamp one DrawRenderable.
// Returns false when the model was culled (RenderPropAndCoronas uses the return to
// decide whether the prop's coronas are submitted at all), true otherwise.
//
// PARAMETER NAMES are the DecFIGS DWARF's (BrnEntityModuleUnity.cpp:18704). The
// committed header spells the two bools lbIsPart / lbIsSmashed -- they are NOT that:
// the asm uses the first to select the env-map technique / LOD state 2 and the second
// to take the Z-only draw path. Reported to the conductor for a rename; the positions
// and types are identical, so this definition matches the declaration either way.
// =============================================================================
bool
PropEntityModule::RenderModel(
    CgsGraphics::Model*     lpModel,
    const Matrix44Affine*   lpTransform,
    DispatchFrame*          lpDispatchFrame,
    Matrix44::InParam       lCameraViewProjection,
    Vector3::InParam        lCameraPosition,
    f32                     lfLodDistanceZoomScale,
    s32                     liModelOnlyDisplayList,
    s32                     liOpaqueList,
    s32                     liTransparentList,
    bool                    lbRenderingEnvironmentMap,
    bool                    lbUseZOnlyRendering,
    const ShaderLodInfo*    lpShaderLodInfo )
{
    // @0x822C4938: the incoming r7 (lCameraViewProjection) is homed nowhere and read
    // nowhere -- the shipped body never uses it. Kept in the signature because both the
    // DWARF and every call site pass it.
    (void)lCameraViewProjection;

    CGS_ASSERT( lpModel != 0, "lpModel != NULL" );
    CGS_ASSERT( lpModel->GetNumLods() > 0, "lpModel->GetNumLods() > 0" );

    // @0x822C49A4..0x822C49D8. The cull metric is the SCALED squared distance from the
    // camera to the transform's translation row.
    const f32 lfScaledDistanceSq =
        rw::math::vpu::MagnitudeSquared( lCameraPosition - lpTransform->Pos() )
        * lfLodDistanceZoomScale;

    const u32 luNumLods    = lpModel->GetNumLods();
    const u32 luLastLod    = luNumLods - 1;
    CGS_ASSERT( luLastLod < luNumLods, "Invalid LOD index" );

    // @0x822C4A00..0x822C4A4C: cull against the LAST lod's switch distance (overridden by
    // the debug slider when mbOverrideLodDistances is armed). `addis r11,r31,3 ; addi
    // r11,r11,0x4CD3 ; slwi 2` is (numLods + 0x34CD3) * 4 == module + 0xD3350 +
    // (numLods-1)*4 == &mauOverrideLodDistances[numLods-1] -- indexed by NAME here.
    f32 lfCullDistance = lpModel->GetLodDistance( luLastLod );
    if ( mbOverrideLodDistances )
    {
        lfCullDistance = static_cast< f32 >( mauOverrideLodDistances[ luLastLod ] );
    }
    if ( lfScaledDistanceSq >= lfCullDistance * lfCullDistance )
    {
        return false;
    }

    // ---- LOD-state selection (@0x822C4A68..0x822C4B58) ----------------------
    CgsGraphics::Model::State leLodState = CgsGraphics::Model::E_STATE_LOD_0;

    if ( mbOverrideLod )
    {
        const CgsGraphics::Model::State leOverride =
            static_cast< CgsGraphics::Model::State >( miLodOverrideValue );
        if ( !lpModel->DoesStateExist( leOverride ) )
        {
            return false;
        }
        leLodState = leOverride;
    }
    else if ( lbRenderingEnvironmentMap )
    {
        // The env-map pass draws LOD 2 or nothing at all.
        if ( !lpModel->DoesStateExist( CgsGraphics::Model::E_STATE_LOD_2 ) )
        {
            return false;
        }
        leLodState = CgsGraphics::Model::E_STATE_LOD_2;
    }
    else
    {
        // First existing LOD whose (possibly overridden) switch distance covers us.
        // The X360 leaves leLodState at 0 when the walk falls off the end.
        for ( u32 luLod = 0; luLod < luNumLods; ++luLod )
        {
            const CgsGraphics::Model::State leCandidate =
                static_cast< CgsGraphics::Model::State >( luLod );
            if ( !lpModel->DoesStateExist( leCandidate ) )
            {
                continue;
            }

            f32 lfLodDistance = lpModel->GetLodDistance( luLod );
            if ( mbOverrideLodDistances )
            {
                lfLodDistance = static_cast< f32 >( mauOverrideLodDistances[ luLod ] );
            }

            if ( lfScaledDistanceSq <= lfLodDistance * lfLodDistance )
            {
                leLodState = leCandidate;
                break;
            }
        }
    }

    CGS_ASSERT( lpModel->DoesStateExist( leLodState ), "lpModel->DoesStateExist( leLodState )" );

    // ---- instanced models go to the collector, not to a draw ----------------
    // @0x822C4B90: bit 0 of mu8Flags. The collector batches up to
    // Model::KU_MAX_INSTANCES_PER_GROUP compatible submissions into ONE DrawRenderable.
    if ( lpModel->GetFlag( CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER ) )
    {
        CgsGraphics::ModelInstanceCollector::AddInstance( lpModel, lpTransform, leLodState );
        return true;
    }

    // ---- technique byte (@0x822C4DC0..0x822C4EB0) ---------------------------
    // The Z-only arm hard-codes E_TECHNIQUE_ZONLY; otherwise this is exactly
    // ShaderLodInfo's two queries, whose bodies the X360 folded in here (the override /
    // mbUseShaderLod / clamped-bounding-sphere chain at 0x822C4E0C..0x822C4EAC reads
    // lpShaderLodInfo +0x24 / +0x28 / +0x2C / +0x10 / +0x00, i.e. miEnvMapTechnique /
    // miOverrideTechnique / mbUseShaderLod / mMaxBelievableRadius / mShaderLod1NearDistance).
    u8 lu8Technique;
    if ( lbUseZOnlyRendering )
    {
        lu8Technique = static_cast< u8 >( E_TECHNIQUE_ZONLY );
    }
    else if ( lbRenderingEnvironmentMap )
    {
        lu8Technique = static_cast< u8 >( lpShaderLodInfo->GetEnvMapTechnique() );
    }
    else
    {
        const Renderable* lpLod0Renderable =
            lpModel->GetRenderable( CgsGraphics::Model::E_STATE_LOD_0 );
        lu8Technique = static_cast< u8 >( lpShaderLodInfo->GetLodTechnique(
            lpLod0Renderable->mBoundingSphere, *lpTransform, lCameraPosition ) );
    }

    // =========================================================================
    // Model::RenderModelOnly INLINED (CgsModel.h:399/400/403/404/409/410/412).
    // Reproduced at the site; see the banner.
    // =========================================================================
    CGS_ASSERT( leLodState < CgsGraphics::Model::E_STATE_COUNT, "leState < E_STATE_COUNT" );

    const Renderable* lpRenderable = lpModel->GetRenderable( leLodState );
    CGS_ASSERT( lpRenderable != 0, "Missing renderable in a model" );
    CGS_ASSERT( lpDispatchFrame != 0, "lpDispatchFrame" );

    CgsGraphics::DispatchList* lpDispatchList =
        lpDispatchFrame->GetList( static_cast< u32 >( liModelOnlyDisplayList ) );
    CgsGraphics::DispatchBin*  lpDispatchBin  = &lpDispatchFrame->GetBin();
    CGS_ASSERT( lpDispatchBin != 0, "lpDispatchBin" );
    CGS_ASSERT( lpDispatchList != 0, "lpDispatchList" );
    CGS_ASSERT( !lpModel->GetFlag( CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER ),
                "lbInstancing == GetFlag(CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER)" );

    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 0, *lpTransform );

    // @0x822C4D80 / @0x822C506C: `clrlwi r11,r30,25 ; cntlzw ; extrwi r5,r11,1,26` is the
    // strength-reduced (count % Model::KU_OBJECTS_PER_JOB_BLOCK) == 0 resend cadence, read
    // BEFORE BeginPacket.
    const bool lbSendAllShaderConstants =
        ( lpDispatchList->GetCount() % CgsGraphics::Model::KU_OBJECTS_PER_JOB_BLOCK ) == 0u;

    lpDispatchBin->BeginPacket();
    if ( lbUseZOnlyRendering )
    {
        // @0x822C4D7C..0x822C4DB8. BOTH mesh-list slots take the OPAQUE list (`extsb r7,r16 ;
        // mr r6,r7`), the frustum-enable byte and the technique byte are both 1, the draw is
        // Z-only, and the trailer is preZList 0xFF / preZTechnique 0 / instanceCount 0 /
        // excludeMeshBits 0. Same shape as WorldEntityModule::RenderInstance's shadow arm.
        CgsGraphics::DrawRenderable::AddToBin(
            lpRenderable, lpDispatchFrame, lbSendAllShaderConstants,
            static_cast< s8 >( liOpaqueList ), static_cast< s8 >( liOpaqueList ),
            1u, lu8Technique, true,
            0xFFu, 0u, 0, 0u );
    }
    else
    {
        // @0x822C5068..0x822C50A8. r6 = extsb(liOpaqueList), r7 = extsb(arg_6C) =
        // liTransparentList, r8 = 1 (frustum enable), r9 = the technique byte, r10 = 0
        // (not Z-only); trailer preZList 0xFF / preZTechnique 0 / instanceCount 0 /
        // excludeMeshBits 0. Note the prop path sends 0xFF -- "no pre-Z re-emit" -- always,
        // unlike the world path, which forwards the caller's pre-Z list id.
        CgsGraphics::DrawRenderable::AddToBin(
            lpRenderable, lpDispatchFrame, lbSendAllShaderConstants,
            static_cast< s8 >( liOpaqueList ), static_cast< s8 >( liTransparentList ),
            1u, lu8Technique, false,
            0xFFu, 0u, 0, 0u );
    }

    // @0x822C50AC..0x822C50C0: DispatchBin::EndPacket() inlined, submitted with sort key 0.
    lpDispatchList->Submit( 0, lpDispatchBin->EndPacket() );

    // -------------------------------------------------------------------------
    // PARKED (see banner item 4): the mbDrawBoundingSpheres debug tail
    // @0x822C50C4..0x822C52AC.
    //   if ( mbDrawBoundingSpheres &&
    //        Magnitude( lCameraPosition - lpTransform->Pos() ) < flt_82CDB6B4 )
    //   {
    //       CgsDev::DebugInterface lDebugInterface;
    //       if ( byte_82CDB6B0 )
    //       {
    //           const Renderable* lpLodRenderable = lpModel->GetRenderable( leLodState );
    //           Vector3 lCentre = TransformPoint( *lpTransform,
    //                                 lpLodRenderable->mBoundingSphere.GetVector3() );
    //           f32 lfRadius = lpLodRenderable->mBoundingSphere.GetPlus()
    //                        * Magnitude( Max( row0, Max( row1, row2 ) ) );
    //           lDebugInterface.GetRender().DrawCircle(
    //               lCentre, Normalise( lCameraPosition - lCentre ), lfRadius,
    //               KAU_LOD_COLOURS[ leLodState ] );
    //       }
    //   }
    // with KAU_LOD_COLOURS = { 0xFF00FF00, 0xFFFF0000, 0xFF0000FF, 0xFF00FFFF, 0xFFFFFF00 }
    // (stack-materialised at 0x822C50D4..0x822C5108; byte-identical to the world module's).
    // Blocked ONLY on flt_82CDB6B4 -- the distance gate -- whose initialiser has no
    // recoverable value: the whole-export scan finds one reference, this read.
    // -------------------------------------------------------------------------

    return true;
}

// =============================================================================
// RenderPropAndCoronas  @ 0x822DC010  (187 insns)
//
// ⚠️ SIGNATURE CORRECTED. B1 placed a placeholder modelled on RenderModel because the
// DecFIGS DWARF does not declare this X360-only function and Hex-Rays renders it as a
// 21-int blob. The real shape, pinned off the prologue @0x822DC030..0x822DC074 and the
// single call site @0x822FB9A8..0x822FBA04 (offsets per the banner's ABI rule):
//
//   r3  this
//   r4  const BrnPhysics::Props::PropGraphics*  -- NOT a Model*. The body's very first
//       act is `lwz r4,4(r4)`, i.e. lpPropGraphics->mpPropModel (console +4), which it
//       forwards as RenderModel's lpModel. GDL passes the table slot straight from
//       mPropGraphicsManager.
//   r5  const Matrix44Affine* -- forwarded UNCHANGED as RenderModel's lpTransform and
//       then re-read at +0x00/+0x10/+0x20/+0x30 for the corona transform. GDL passes the
//       PropEntityInstance*, whose mWorldTransform is at offset 0.
//   r6  u32 luPropTypeId -- bounds-checked against the VFX prop table and used as its
//       16-byte-strided index.
//   r7  DispatchFrame*        r8  const Matrix44&        v1  Vector3 camera position
//   f1  f32 zoom scale (skips r9)                        r10 s32 liModelOnlyDisplayList
//   @0x64 liOpaqueList   @0x6C liTransparentList   @0x74 bool lbRenderingEnvironmentMap
//   @0x7C const ShaderLodInfo*                     @0x84 bool lbRenderCoronas
//   @0x8C BrnParticle::VFXProp* (the collection's prop table, which GDL pre-loads once)
//   @0x94 bool lbUseZOnlyRendering
//   @0x9C BrnCoronaManager::BrnSubmissionInterface*  -- taken here as the console's raw
//         32-bit word, because that is what InputBuffer_Dispatch still stores it as (see
//         that header's POINTER WIDTH note); the parked corona loop never dereferences it,
//         and reconstituting a host pointer from a 32-bit slot would be the truncation bug.
//
// RETURN TYPE: void. The epilogue sets no r3 (both early-outs branch straight to it with
// whatever RenderModel left there), and the only caller discards it. B1's `bool` was
// copied from RenderModel.
//
// Because the corona half is parked (banner item 2) the last three parameters would be
// unused; they are kept in the signature -- they are what the console passes, and the
// follow-up TU needs them. The two the parked loop no longer needs a TYPE for
// (the VFX prop table and the corona interface) are taken as the console's raw words so
// this file does not have to mint a serialised-record layout it cannot verify.
// =============================================================================
void
PropEntityModule::RenderPropAndCoronas(
    const BrnPhysics::Props::PropGraphics* lpPropGraphics,
    const Matrix44Affine*   lpTransform,
    u32                     luPropTypeId,
    DispatchFrame*          lpDispatchFrame,
    Matrix44::InParam       lCameraViewProjection,
    Vector3::InParam        lCameraPosition,
    f32                     lfLodDistanceZoomScale,
    s32                     liModelOnlyDisplayList,
    s32                     liOpaqueList,
    s32                     liTransparentList,
    bool                    lbRenderingEnvironmentMap,
    const ShaderLodInfo*    lpShaderLodInfo,
    bool                    lbRenderCoronas,
    const void*             lpVFXPropTable,
    bool                    lbUseZOnlyRendering,
    u32                     luCoronaSubmissionInterface )
{
    // @0x822DC03C: the model comes out of the graphics record; every other register is a
    // straight forward of this function's own parameter.
    const bool lbDrawn = RenderModel( lpPropGraphics->mpPropModel,
                                      lpTransform,
                                      lpDispatchFrame,
                                      lCameraViewProjection,
                                      lCameraPosition,
                                      lfLodDistanceZoomScale,
                                      liModelOnlyDisplayList,
                                      liOpaqueList,
                                      liTransparentList,
                                      lbRenderingEnvironmentMap,
                                      lbUseZOnlyRendering,
                                      lpShaderLodInfo );

    // @0x822DC078..0x822DC08C: coronas only when the caller asked for them AND the model
    // survived its distance cull.
    if ( !lbRenderCoronas || !lbDrawn )
    {
        return;
    }

    (void)luPropTypeId;
    (void)lpVFXPropTable;
    (void)luCoronaSubmissionInterface;

    // -------------------------------------------------------------------------
    // PARKED (see banner item 2): the corona loop @0x822DC090..0x822DC2DC. FULLY
    // DECODED -- the follow-up TU only has to home the five BrnParticle records. The
    // DecFIGS DWARF (SharedClasses/Graphics/VFXPropsResourceType.h) already matches the
    // asm field for field:
    //
    //   VFXPropCollection { VFXProp* mpPropTable; u32 muPropTableSize; ... }
    //   VFXProp           { u64 mPropID; VFXPropState* mpPropStates; u32 muNumPropStates; }
    //                       -- console stride 16, which is the asm's `slwi r11,r30,4`
    //   VFXPropState      { VFXMaterial* mpVFXMaterial; u32 muNumVFXMaterials;
    //                       VFXCoronaType* mpCoronaType; u32 muNumCoronas; }
    //                       -- the asm's *(state+8) / *(state+0xC)
    //   VFXCoronaType     { Matrix44Affine mTransform; VFXCoronaTypeData* mpTypeData;
    //                       f32 mrTimeOffset; }  -- console stride 0x50, the asm's r24 += 0x50
    //   VFXCoronaTypeData { u32 mnID; u32 mType; f32 mrTimeOn; f32 mrTimeOff;
    //                       f32 mrSizeMin; f32 mrSizeMax; f32 mrMasterTime;
    //                       bool mbSynchronised; }
    //
    // The body:
    //   const VFXPropCollection* lpCollection = mVFXPropCollection.GetMemoryResource();
    //   CGS_ASSERT( luPropTypeId < lpCollection->muPropTableSize,
    //               "Prop TypeId not in Vfx prop table. If new prop..." );
    //   const VFXProp* lpVFXProp = &lpVFXPropTable[ luPropTypeId ];   // caller-supplied table
    //   CGS_ASSERT( lpVFXProp != 0, "lpVFXProp != NULL" );
    //   const VFXPropState* lpState = lpVFXProp->GetState( 0 );
    //   for ( u32 luCorona = 0; luCorona < lpState->muNumCoronas; ++luCorona )
    //   {
    //       const VFXCoronaType* lpCoronaType = &lpState->mpCoronaType[ luCorona ];
    //       CGS_ASSERT( lpCoronaType != 0, "lpCoronaType != NULL" );
    //
    //       // @0x822DC180..0x822DC1D0. Read the vmaddfp operands in ENCODING order
    //       // (vD,vA,vB,vC => vD = vA*vC + vB) for the plain form and in LOGICAL order for
    //       // the ...128 form; getting this backwards silently swaps the accumulator and
    //       // the multiplier and puts every corona at the wrong place.
    //       Vector3 lWorldPos    = TransformPoint ( *lpTransform, lpCoronaType->mTransform.Pos() );
    //       Vector3 lWorldNormal = TransformVector( *lpTransform, lpCoronaType->mTransform.At() );
    //
    //       // Back-face cull: skip when the camera is behind the corona's plane.
    //       if ( Dot3( lWorldPos - lCameraPosition, lWorldNormal ) >= 0.0f ) continue;
    //
    //       const VFXCoronaTypeData* lpData = lpCoronaType->mpTypeData;
    //       f32 lfIntensity;
    //       if ( lpData->mrTimeOn == flt_82001CC0 && lpData->mrTimeOff == flt_82001CC0 )
    //       {
    //           lfIntensity = 1.0f;                       // flt_82001C98
    //       }
    //       else
    //       {
    //           // @0x822DC214..0x822DC298: a vrefp/vnmsubfp/vmaddfp Newton-Raphson
    //           // reciprocal, a vrfiz truncate and a vnmsubfp -- i.e. the strength-reduced
    //           // fmod, then a triangular on/off ramp.
    //           const f32 lfPeriod = lpData->mrTimeOn + lpData->mrTimeOff;
    //           const f32 lfPhase  = fmodf( lpData->mrMasterTime + lpCoronaType->mrTimeOffset,
    //                                       lfPeriod );
    //           lfIntensity = ( lfPhase <= lpData->mrTimeOn )
    //                             ? ( lfPhase / lpData->mrTimeOn )
    //                             : ( 1.0f - ( lfPhase - lpData->mrTimeOn ) / lpData->mrTimeOff );
    //       }
    //
    //       const Vector2 lSizeMinMax = { lpData->mrSizeMin, lpData->mrSizeMax };
    //       lpCoronaSubmissionInterface->AddPropCorona( &lSizeMinMax,
    //                                                   lpData->GetCoronaType(),
    //                                                   lWorldPos, lWorldNormal, lfIntensity );
    //   }
    //
    // Still unresolved for the follow-up: the value of flt_82001CC0 (the "both times are
    // this => always on" sentinel, almost certainly 0.0f but not readable from a
    // function-only export) and AddPropCorona's exact parameter order, which must be
    // pinned off 0x823FD138's own prologue rather than off this call site.
    // -------------------------------------------------------------------------
}

// =============================================================================
// GenerateDispatchLists  @ 0x822FB4F0  (774 insns; BrnPropEntityModule.cpp:2530-ish --
// the asserts carry lines 2551 / 2554 / 2584 / 2638 / 2693 / 2707 / 2721 / 2724 / 2725)
//
// Walk the frustum-visible prop id array WorldModule::FilterFrustumTestResults filled and
// submit each one: a whole prop through RenderPropAndCoronas, a smashed prop's part
// through RenderModel, all bracketed by one instanced-collection pass.
// =============================================================================
void
PropEntityModule::GenerateDispatchLists(
    PropEntityIO::InputBuffer_Dispatch*                    lpInput,
    const Array<CgsSceneManager::EntityId, 5400u>&         lrVisibleEntities,
    Matrix44::InParam                                      lCameraViewProjection,
    Vector3::InParam                                       lCameraPosition,
    f32                                                    lfMainCameraZoomFactor,
    const ShaderLodInfo*                                   lpShaderLodInfo,
    s32                                                    liModelOnlyDisplayList,
    s32                                                    liOpaqueList,
    s32                                                    liTransparentList,
    bool                                                   lbRenderingEnvironmentMap,
    bool                                                   lbRenderCoronas )
{
    lpInput->LockForRead();

    DispatchFrame* lpDispatchFrame = lpInput->GetDispatchFrame();
    ShadowMap*     lpShadowMap     = lpInput->GetShadowMap();

    // @0x822FB59C / @0x822FB5D4 (cpp:2551 / 2554).
    //
    // ⭐ DEGRADED TO A LOG-ONCE GATE 2026-08-12 (conductor). The console assert is faithful
    // and stays faithful in spirit -- but on THIS tree the corona submission interface is
    // NULL BY KNOWN DESIGN, not by accident: its only writer is
    // WorldModule::BridgeWorldModuleToEntityModules_Render, which is still an inert boot gate
    // (parked with reason -- the shipped signature has no `this` and no shadow-map source, and
    // its body is a hole in the IDA export). So this condition is guaranteed false on every
    // frame that renders props.
    //
    // MEASURED: as a hard assert it fired 281 times in a single ~2-minute run -- the single
    // largest source in the log -- halting the game each time and making the build unplayable.
    // That is not a faithful console diagnostic doing its job; it is a known PC gap screaming
    // once per frame. Same treatment the project already gave DebugComponent::Update and the
    // contact-generation liIndex tripwire when they began firing per-entity-per-frame.
    //
    // ⛔ RESTORE THE HARD ASSERT when that bridge is bodied -- at which point a NULL here
    // really would be a bug worth stopping for.
    if ( lpInput->GetCoronaSubmissionInterface() == 0 )
    {
        // (gpDebugPrint guard added: every other log site in this file checks it, and this
        // one runs on the first prop dispatch frame, before the log is guaranteed up.)
        static bool s_bLoggedNullCorona = false;
        if ( !s_bLoggedNullCorona && CgsDev::Log::gpDebugPrint != 0 )
        {
            s_bLoggedNullCorona = true;
            *CgsDev::Log::gpDebugPrint
                << "[props-gdl] corona submission interface is NULL -- prop coronas skipped "
                   "(BridgeWorldModuleToEntityModules_Render still gated) [FLAG PC boot gate]\n";
        }
    }
    CGS_ASSERT( lpShadowMap != 0, "lpShadowMap" );

    // @0x822FB5F8: the shadow map's two bytes, ANDed.
    const bool lbUseZOnlyRendering = lpShadowMap != 0 &&
                                     lpShadowMap->IsRenderingShadowMap() &&
                                     lpShadowMap->IsUsingZOnlyRenderingPath();

    // ---- [FLAG PC bring-up gate] NOT IN THE CONSOLE -------------------------
    // ⛔ DELETE-WHEN WorldModule::BridgeWorldModuleToEntityModules_Render is bodied.
    // On the console this buffer is always seeded before the call. In this tree it is NOT:
    // its only writer is BridgeWorldModuleToEntityModules_Render, still an inert boot gate
    // at WorldLinkStubs.cpp:1690, so the frame / shadow map / corona interface are all
    // still zero. Without this guard the very first frame that reaches this function
    // null-derefs in RenderModel's DispatchFrame::GetList. The two asserts above are the
    // faithful tripwires; this is the bring-up safety net under them.
    if ( lpDispatchFrame == 0 || lpShadowMap == 0 )
    {
        static bool sbLoggedUnseeded = false;
        if ( !sbLoggedUnseeded && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLoggedUnseeded = true;
            *CgsDev::Log::gpDebugPrint
                << "[props-gdl] BLOCKED: the prop dispatch input is unseeded (frame="
                << ( lpDispatchFrame != 0 ? 1 : 0 ) << " shadowMap="
                << ( lpShadowMap != 0 ? 1 : 0 )
                << "). WorldModule::BridgeWorldModuleToEntityModules_Render is still an "
                   "inert stub -- no prop can be drawn until it seeds this buffer.\n";
        }
        lpInput->UnlockForRead();
        return;
    }

    // @0x822FB618: 1.0f / (zoom * zoom). Every callee multiplies a SQUARED distance by it.
    const f32 lfLodDistanceZoomScale =
        1.0f / ( lfMainCameraZoomFactor * lfMainCameraZoomFactor );

    const u32 luNumEntities = lrVisibleEntities.GetLength();

    // [DIAG prop-render wave] the object-list depth BEFORE this module contributes --
    // `submitted` only counts RenderPropAndCoronas/RenderModel CALLS, and RenderModel can
    // still return false on the LOD-distance cull, so the honest "did props become draw
    // commands" measure is the delta on the display list itself.
    const s32 liListCountBefore =
        static_cast< s32 >( lpDispatchFrame->GetList( liModelOnlyDisplayList )->GetCount() );

    CgsGraphics::ModelInstanceCollector::BeginInstanceCollection(
        lpDispatchFrame, &lCameraViewProjection,
        liModelOnlyDisplayList, liOpaqueList, liTransparentList,
        0xFFu, lbUseZOnlyRendering );

    // @0x822FB688: the VFX prop table is fetched ONCE, outside the loop, and handed to
    // every RenderPropAndCoronas call (which re-fetches the collection itself for the
    // bounds check). Parked with the corona loop -- see RenderPropAndCoronas -- so the
    // console's `*(u32*)mVFXPropCollection.GetMemoryResource()` read is not performed here
    // and the parameter is passed as null rather than as a guessed table pointer.
    const void* lpVFXPropTable = 0;

    s32 liSubmitted = 0;

    // ---- [DIAG prop-render wave 2026-08-12] DROP BISECTION -------------------
    // ⛔ DELETE-WHEN submitted > 0. Every path out of this loop that is NOT a
    // submission is silent (no assert fires on any of them), so the witness below
    // cannot tell "the ids are not props" from "the ids are props with no graphics
    // record". These counters split them.
    s32 liOwnerWholeProp   = 0;   // owner 3
    s32 liOwnerReplayProp  = 0;   // owner 34
    s32 liOwnerOther       = 0;   // anything FilterFrustumTestResults should not have passed
    s32 liDropReplay       = 0;   // the !mbInReplay && owner==34 silent skip
    s32 liDropPartNoGfx    = 0;   // part arm, GetPropGraphics() == 0
    s32 liDropWholeNoGfx   = 0;   // whole arm, GetPropGraphics() == 0
    u32 lauSampleTypeId[ 8 ] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    s32 laiSampleZone[ 8 ]   = { -1, -1, -1, -1, -1, -1, -1, -1 };
    s32 liSamples            = 0;

    // The registration-table census, printed once and BEFORE the loop (the loop
    // dereferences whatever the table holds, so this has to survive a bad record):
    // how many of the 500 type slots CachePropGraphicsLists populated, which ids they
    // are, and how many of those records still carry a null body Model -- i.e. an
    // unresolved bundle import, which RenderModel would walk straight into.
    static bool sbLoggedRegistry = false;
    if ( !sbLoggedRegistry && CgsDev::Log::gpDebugPrint != 0 )
    {
        sbLoggedRegistry = true;
        s32 liRegistered  = 0;
        s32 liNullModels  = 0;
        *CgsDev::Log::gpDebugPrint << "[props-gdl] registry: types";
        for ( u32 luType = 0; luType < BrnPhysics::Props::KU_MAX_PROP_TYPES; ++luType )
        {
            const BrnPhysics::Props::PropGraphics* lpRegistered =
                mPropGraphicsManager.GetPropGraphics( luType );
            if ( lpRegistered != 0 )
            {
                ++liRegistered;
                if ( lpRegistered->mpPropModel == 0 ) { ++liNullModels; }
                if ( liRegistered <= 32 )
                {
                    *CgsDev::Log::gpDebugPrint << " " << static_cast< s32 >( luType );
                }
            }
        }
        *CgsDev::Log::gpDebugPrint << " | registered=" << liRegistered
                                   << " nullModel=" << liNullModels << "\n";
    }

    for ( u32 luEntity = 0; luEntity < luNumEntities; ++luEntity )
    {
        const CgsSceneManager::EntityId lEntityId = lrVisibleEntities.GetItem( luEntity );
        const u8 lu8Owner = lEntityId.GetOwner();

        // @0x822FB794 (cpp:2584).
        CGS_ASSERT( lu8Owner == BrnWorld::E_ENTITYTYPE_PROP ||
                    lu8Owner == KU8_ENTITYTYPE_REPLAY_PROP,
                    "PropEntityModule is trying to render something that isn't a prop" );

        // [DIAG prop-render wave] owner census -- see the block above the loop.
        if ( lu8Owner == BrnWorld::E_ENTITYTYPE_PROP )            { ++liOwnerWholeProp; }
        else if ( lu8Owner == KU8_ENTITYTYPE_REPLAY_PROP )        { ++liOwnerReplayProp; }
        else                                                      { ++liOwnerOther; }

        // @0x822FB7E0: replay ids are only ever looked at while the replay is running.
        if ( !mbInReplay && lu8Owner == KU8_ENTITYTYPE_REPLAY_PROP )
        {
            ++liDropReplay;   // [DIAG prop-render wave]
            continue;
        }

        if ( mbInReplay || lu8Owner == KU8_ENTITYTYPE_REPLAY_PROP )
        {
            // -----------------------------------------------------------------
            // PARKED (see banner item 1). The four replay arms the X360 has here:
            //   * owner 34, part index 0   -> RenderReplayProp( lEntityId, ... )
            //     @0x822FBA0C: 14 arguments, forwarding the same frame / viewProj /
            //     cameraPos / zoom / three list ids / envMap / shaderLod / renderCoronas /
            //     VFX table / zOnly / corona interface this function received.
            //   * owner 34, part index != 0 -> the recorded-part arm @0x822FBB20: the part's
            //     prop type and part id come out of two Array<u16,N> tables at
            //     PropEntitySerialiser::GetStaticLayout() + 0x3A20 + 0x3810 / + 0x3912, and
            //     its transform from PropSerialiserFrame::GetPartTransform.
            //   * a LIVE prop while mbInReplay -> skipped when
            //     PropSerialiserFrame::IsCellActive( GetCellId( pos.x, pos.z ) ), and again
            //     when !IsPropAddedToScene( zone, entityIndex - mauStartIndexOfZone[zone] ).
            //   * a LIVE part while mbInReplay -> skipped outright (@0x822FBA7C).
            // None of RenderReplayProp / IsCellActive / IsPropAddedToScene /
            // GetPartTransform is declared in the tree, and mauStartIndexOfZone is private
            // to PropZoneManager with no accessor. Skipping is the only non-inventing
            // behaviour: during a replay no props are drawn, and the log says so once.
            // -----------------------------------------------------------------
            static bool sbLoggedReplayPark = false;
            if ( !sbLoggedReplayPark && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedReplayPark = true;
                *CgsDev::Log::gpDebugPrint
                    << "[props-gdl] PARKED: replay prop dispatch not reconstructed "
                       "(RenderReplayProp / PropSerialiserFrame); props are not drawn "
                       "while mbInReplay\n";
            }
            continue;
        }

        if ( lEntityId.GetPartIndex() != 0 )
        {
            // ---- a part of a smashed prop (@0x822FBA88..0x822FBB1C) ----------
            CGS_ASSERT( lu8Owner == BrnWorld::E_ENTITYTYPE_PROP,
                        "mEntityId.GetOwner() == E_ENTITYTYPE_PROP" );

            PropPartEntityInstance* lpPart =
                mZoneManager.GetPart( PropEntityID( lEntityId ) );
            CGS_ASSERT( lpPart != 0, "lpPart != NULL" );

            // The X360 copies the four transform rows onto its own stack before the call
            // (0x822FBAE0..0x822FBB18); a reference into the instance is the same thing.
            const Matrix44Affine& lrTransform = lpPart->mWorldTransform;
            const u32 luPropTypeId = lpPart->muTypeId;
            const u32 luPartId     = lpPart->muPartId;

            // ---- shared part tail (@0x822FBCD4..0x822FBDF0) ------------------
            CGS_ASSERT( luPropTypeId < BrnPhysics::Props::KU_MAX_PROP_TYPES,
                        "luType < BrnPhysics::Props::KU_MAX_PROP_TYPES" );

            const BrnPhysics::Props::PropGraphics* lpPropGraphics =
                mPropGraphicsManager.GetPropGraphics( luPropTypeId );
            if ( lpPropGraphics == 0 )
            {
                // [DIAG prop-render wave]
                ++liDropPartNoGfx;
                if ( liSamples < 8 )
                {
                    lauSampleTypeId[ liSamples ] = luPropTypeId;
                    laiSampleZone[ liSamples ]   = -2;   // -2 == "a part, zone not read here"
                    ++liSamples;
                }
                continue;
            }

            CGS_ASSERT( luPartId <
                            GetPropPhysicsDataHeader()->GetType( luPropTypeId )->GetNumberOfParts(),
                        "liPartTypeId < mpPropPhysicsDataHeader->GetType( liPropTypeId )->muNumberOfParts" );

            const BrnPhysics::Props::PropPartGraphics* lpPartGraphics =
                &lpPropGraphics->mpParts[ luPartId ];
            CGS_ASSERT( lpPartGraphics != 0, "lpPartGraphics != NULL" );
            CGS_ASSERT( lpPartGraphics->mpPropModel != 0, "lpPartGraphics->mpPropModel != NULL" );

            RenderModel( lpPartGraphics->mpPropModel, &lrTransform, lpDispatchFrame,
                         lCameraViewProjection, lCameraPosition, lfLodDistanceZoomScale,
                         liModelOnlyDisplayList, liOpaqueList, liTransparentList,
                         lbRenderingEnvironmentMap, lbUseZOnlyRendering, lpShaderLodInfo );
            ++liSubmitted;
        }
        else
        {
            // ---- a whole, unsmashed prop (@0x822FB82C..0x822FBA04) -----------
            CGS_ASSERT( lu8Owner == BrnWorld::E_ENTITYTYPE_PROP,
                        "mEntityId.GetOwner() == E_ENTITYTYPE_PROP" );

            PropEntityInstance* lpProp = mZoneManager.GetProp( PropEntityID( lEntityId ) );

            const u16 lu16ZoneIndex = lpProp->GetZone();
            const u32 luPropTypeId  = lpProp->GetTypeId();

            // @0x822FB870 (cpp:2621): a SIGNED compare against -1 on the u16 zone.
            CGS_ASSERT( static_cast< s16 >( lu16ZoneIndex ) > -1, "liZoneIndex > -1" );

            CGS_ASSERT( luPropTypeId < BrnPhysics::Props::KU_MAX_PROP_TYPES,
                        "luType < BrnPhysics::Props::KU_MAX_PROP_TYPES" );

            const BrnPhysics::Props::PropGraphics* lpPropGraphics =
                mPropGraphicsManager.GetPropGraphics( luPropTypeId );
            if ( lpPropGraphics == 0 )
            {
                // [DIAG prop-render wave]
                ++liDropWholeNoGfx;
                if ( liSamples < 8 )
                {
                    lauSampleTypeId[ liSamples ] = luPropTypeId;
                    laiSampleZone[ liSamples ]   = static_cast< s32 >( lu16ZoneIndex );
                    ++liSamples;
                }
                continue;
            }

            // @0x822FB97C (cpp:2638).
            CGS_ASSERT( !lpProp->IsSmashed(), "!lpProp->IsSmashed()" );

            RenderPropAndCoronas( lpPropGraphics, &lpProp->GetWorldTransform(), luPropTypeId,
                                  lpDispatchFrame, lCameraViewProjection, lCameraPosition,
                                  lfLodDistanceZoomScale,
                                  liModelOnlyDisplayList, liOpaqueList, liTransparentList,
                                  lbRenderingEnvironmentMap, lpShaderLodInfo, lbRenderCoronas,
                                  lpVFXPropTable, lbUseZOnlyRendering,
                                  lpInput->GetCoronaSubmissionInterface() );
            ++liSubmitted;
        }
    }

    CgsGraphics::ModelInstanceCollector::EndInstanceCollection();
    lpInput->UnlockForRead();

    // -------------------------------------------------------------------------
    // PARKED (see banner item 3): the overhead-sign refresh @0x822FBE20..0x822FC0C8.
    //   if ( lbRenderCoronas && --miFramesUntilUpdateVisibleSigns <= 0 )
    //   {
    //       mVisibleOverheadSigns.Clear();
    //       for each visible non-replay whole prop:
    //           if ( !prop->IsSmashed()-flag-bit-0x20 &&
    //                type->muSomeId is one of { 500950, 500930, 506050 }  (overhead signs)
    //                                        or { 428180, 428152 }        (the "score" pair) &&
    //                !mVisibleOverheadSigns.IsFull() &&
    //                MagnitudeSquared( mPlayerPosition - prop->mWorldTransform.Pos() )
    //                    <= unk_82FAD730 )
    //           {
    //               entry = mVisibleOverheadSigns.Grow();
    //               entry->position = prop->mWorldTransform.Pos();
    //               entry->id       = PropEntityID( id ).GetEntityIndex();
    //               entry->position += ( score-pair ? unk_82FAD900 : unk_82FAD740 );
    //           }
    //       miFramesUntilUpdateVisibleSigns = 60;
    //   }
    // mVisibleOverheadSigns has no real type (B1 could only model it as 1052 opaque bytes)
    // and BrnGui::OverheadSignScore<32>::IsFull/Grow do not exist in the tree. Nothing in
    // the tree reads the list, so not maintaining it changes no observable behaviour yet.
    // -------------------------------------------------------------------------

    // ---- [DIAG prop-render wave 2026-08-12] THE SUBMISSION WITNESS -----------
    // ⛔ DELETE-WHEN props are confirmed on screen.
    // Answers the one question boot verification needs: did anything reach GDL object list
    // 11 at all, and if not, was the visible-id array empty or was every prop dropped for
    // want of a graphics record? Latched on the (visible, submitted) pair, not on a
    // "printed once" bool, so the line reprints whenever the picture changes.
    // Reflections step 1: this now runs SEVEN times a frame (main view + six env-map faces),
    // and a (visible, submitted) latch shared across all seven reprinted on nearly every call
    // (63k lines in one 200 s boot). The main view keeps its change-latched witness; each
    // env-map list gets ONE line, the first time it submits anything -- which is the fact a boot
    // needs ("props reach the face lists"), without a per-frame log.
    if ( CgsDev::Log::gpDebugPrint != 0 )
    {
        static s32  siLastVisible   = -1;
        static s32  siLastSubmitted = -1;
        static bool sabEnvMapListWitnessed[ 32 ] = {};
        const s32   liVisible       = static_cast< s32 >( luNumEntities );
        bool lbPrint;
        if ( lbRenderingEnvironmentMap )
        {
            const bool lbKnownList = ( liModelOnlyDisplayList >= 0 && liModelOnlyDisplayList < 32 );
            lbPrint = lbKnownList && liSubmitted > 0 && !sabEnvMapListWitnessed[ liModelOnlyDisplayList ];
            if ( lbPrint )
            {
                sabEnvMapListWitnessed[ liModelOnlyDisplayList ] = true;
            }
        }
        else
        {
            lbPrint = ( liVisible != siLastVisible || liSubmitted != siLastSubmitted );
        }
        if ( lbPrint )
        {
            if ( !lbRenderingEnvironmentMap )
            {
                siLastVisible   = liVisible;
                siLastSubmitted = liSubmitted;
            }
            *CgsDev::Log::gpDebugPrint
                << "[props-gdl] visible=" << liVisible
                << " submitted=" << liSubmitted
                << " objectList=" << liModelOnlyDisplayList
                << " opaque=" << liOpaqueList
                << " transparent=" << liTransparentList
                << " zOnly=" << ( lbUseZOnlyRendering ? 1 : 0 )
                << " envMap=" << ( lbRenderingEnvironmentMap ? 1 : 0 )
                << " | owner3=" << liOwnerWholeProp
                << " owner34=" << liOwnerReplayProp
                << " ownerOther=" << liOwnerOther
                << " | dropReplay=" << liDropReplay
                << " dropWholeNoGfx=" << liDropWholeNoGfx
                << " dropPartNoGfx=" << liDropPartNoGfx
                << " inReplay=" << ( mbInReplay ? 1 : 0 )
                << " | list" << liModelOnlyDisplayList << " "
                << liListCountBefore << "->"
                << static_cast< s32 >(
                       lpDispatchFrame->GetList( liModelOnlyDisplayList )->GetCount() )
                << "\n";

            if ( liSamples > 0 )
            {
                *CgsDev::Log::gpDebugPrint << "[props-gdl]   dropped samples (typeId/zone):";
                for ( s32 liSample = 0; liSample < liSamples; ++liSample )
                {
                    *CgsDev::Log::gpDebugPrint
                        << " " << static_cast< s32 >( lauSampleTypeId[ liSample ] )
                        << "/" << laiSampleZone[ liSample ];
                }
                *CgsDev::Log::gpDebugPrint << "\n";
            }
        }

    }
}

} // namespace BrnWorld
