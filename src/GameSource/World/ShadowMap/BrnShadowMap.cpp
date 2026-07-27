#include "GameSource/World/ShadowMap/BrnShadowMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"            // CgsCore::SnPrintf (Construct's per-CSM debug paths)
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h" // CgsNumeric::Min (CalcOptimisedLod)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h" // Construct's debug-variable registration
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"           // CgsDev::DebugUI::StringList (type options)
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"    // ShaderConstantTable (SetConstantsForEnvmap)

#include <algorithm> // std::sort (ComputeOptimalViewVolume's candidate-plane ordering)
#include <cmath>     // std::sqrt/sin/cos (the vrsqrtefp + 2x NR chains, de-optimised exact)
#include <cstring>   // memcpy (the 128-byte frustum copies)

// ============================================================================
// GameSource/Unity/../World/ShadowMap/BrnShadowMap.cpp
//
// BrnWorld::ShadowMap -- the shadow-map manager (see BrnShadowMap.h). Bodied
// functions of the 7-function class TU:
//
//   CalcOptimisedLod          @ 0x827B42E8  (pure scalar int)
//   CalcLodDistanceModifier   @ 0x827B43A8  (trivial branch + splat)
//   ComputeTSMMatrix          @ 0x827BFF58  (wave-2 dedicated VMX pass -- the
//                                            trapezoidal-shadow-map best-fit solve)
//
// DECLARATION-ONLY (see BrnShadowMap.h for the per-function FLAG comments): Construct,
// CalculateShadowMapCameras, ComputeBoundingBoxMatrix, ComputeOptimalViewVolume,
// DebugRender. Each is a heavily VMX-laden pipeline that stays declared in the header
// until its own dependency TUs land.
// ============================================================================

namespace rw { namespace math { namespace vpu {
    // rw::math::vpu::Inverse @ 0x825B2628 -- 4x4 matrix inverse + determinant
    // out. PENDING declaration (the body is its own TU; canonical home is the
    // rw/math/vpu operation-header family); call shape attested at the
    // ComputeTSMMatrix bl @ 0x827C123C (r3 = struct-return slot, r4 = &matrix,
    // r5 = &determinant-out).
    Matrix44 Inverse(const Matrix44& lrMatrix, Vector4& lrDeterminant);
} } }

// The global runtime shader-constant register (X360 symbol mShaderConstantTable;
// same per-TU extern idiom as BrnWorldModule.cpp -- the defining home lands with
// the shader TU). SetConstantsForEnvmap stages c15/c16 through it.
namespace CgsGraphics { extern ::ShaderConstantTable mShaderConstantTable; }

namespace BrnWorld
{
    // BrnShadowMap.cpp:373-374 (DWARF). Per-shadow-map-slot LOD tuning tables, indexed by
    // muCurrentShadowMap (0..KU_NUM_SHADOW_MAPS-1). NOT marked `const` by the DWARF (game
    // code can presumably tune them at runtime via debug variables), but their rodata
    // contents are not in this dossier's exports -- carried as honest zeros, matching the
    // project convention for un-dumped rodata (never fabricated; see e.g.
    // BrnPhysicalBodyPart.cpp's KF_PART_* constants).
    s32 KA_SHADOWMAP_LOD_MODIFIER[KU_NUM_SHADOW_MAPS]          = { 0, 0, 0 };  // FLAG: rodata not recovered
    f32 KA_SHADOWMAP_LOD_DISTANCE_MODIFIER[KU_NUM_SHADOW_MAPS] = { 0.0f, 0.0f, 0.0f }; // FLAG: rodata not recovered

    // BrnShadowMap.cpp:378-379 (DWARF). Debug-toggleable optimisation gates the two
    // functions below branch on (byte_8300E120 / byte_8300E121 in the X360 asm). Default
    // value not attested in this dossier (no Construct/ctor initialiser recovered for these
    // in the boot trace); carried as `false` (optimisations off), the conservative/inert
    // default that keeps CalcOptimisedLod/CalcLodDistanceModifier on their early-out paths.
    bool sbOptimiseShadowLods         = false; // FLAG: default value not recovered
    bool sbOptimiseShadowLodDistances = false; // FLAG: default value not recovered

    // BrnShadowMap.cpp / BrnShadowMap.h:170, X360 0x827B42E8.
    //
    // If shadow-LOD optimisation is disabled, pass the lower-detail request through
    // unchanged (matches leHigherDetailState's early `return a2`). Otherwise, offset the
    // higher-detail state by the current shadow map's configured LOD modifier and clamp
    // the result to no more detailed than leHigherDetailState via the branchless
    // CgsNumeric::Min (the X360 inlines CgsBranchlessOperations.h's Min here, complete
    // with its CGS_ASSERT against the branchy reference -- reproduced by calling the
    // shared inline rather than re-deriving the sign-mask trick locally).
    CgsGraphics::Model::State ShadowMap::CalcOptimisedLod(CgsGraphics::Model::State leLowerDetailState,
                                                            CgsGraphics::Model::State leHigherDetailState)
    {
        if (!sbOptimiseShadowLods)
        {
            return leLowerDetailState;
        }

        const s32 liOffsetState = KA_SHADOWMAP_LOD_MODIFIER[muCurrentShadowMap] + static_cast<s32>(leLowerDetailState);

        // rw::core::stdc::Min(leHigherDetailState, liOffsetState) -- inlined branchless,
        // asserts at CgsBranchlessOperations.h:62 (fires via CgsNumeric::Min itself).
        return static_cast<CgsGraphics::Model::State>(
            CgsNumeric::Min(static_cast<s32>(leHigherDetailState), liOffsetState));
    }

    // BrnShadowMap.cpp / BrnShadowMap.h:173, X360 0x827B43A8.
    //
    // Returns the broadcast VecFloat LOD-distance modifier for the current shadow map slot
    // while the shadow-LOD-distance optimisation is enabled; an all-zero VecFloat
    // otherwise. Recovered from the asm's vspltisw/lvlx+vspltw idiom: `vspltisw v0,0` is a
    // zero broadcast, and `lvlx v0,r10,r11 ; vspltw v0,v0,0` loads
    // KA_SHADOWMAP_LOD_DISTANCE_MODIFIER[muCurrentShadowMap] and splats lane 0 across all
    // four lanes of the returned VecFloat -- i.e. `VecFloat(scalar)` construction.
    VecFloat ShadowMap::CalcLodDistanceModifier()
    {
        if (!sbOptimiseShadowLodDistances)
        {
            return VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };
        }

        const f32 lfModifier = KA_SHADOWMAP_LOD_DISTANCE_MODIFIER[muCurrentShadowMap];
        return VecFloat{ lfModifier, lfModifier, lfModifier, lfModifier };
    }

    // ========================================================================
    // Construct-time configuration constants (DWARF globals block, BrnShadowMap.cpp
    // :7..:61 -- names are the DWARF ground truth). The SCALAR values are recovered
    // from the X360 Construct asm (the pseudocode resolves the float immediates);
    // the per-slot ARRAYS live in un-dumped rodata (IDA exports carry no data --
    // see [[ida-exports-no-data]]), carried as honest zeros per the project
    // convention (same as KA_SHADOWMAP_LOD_MODIFIER above).
    // ========================================================================
    const f32 KF_SHADOWMAP_NEAR_PLANE_OFFSET = -450.0f;  // flt_820CA850 (@0x827B4468 -> +0x14D0)
    const f32 KF_SHADOWMAP_FAR_PLANE_OFFSET  = 650.0f;   // flt_820CA854 (@0x827B4488 -> +0x14D4)
    const f32 KF_SHADOWMAP_EYE_OFFSET        = 450.0f;   // flt_820CA858 (@0x827B4490 -> +0x14D8)
    const f32 KF_FADE_START_DISTANCE         = 30.0f;    // @0x827B468C -> +0x14F0
    const f32 KF_SHADOWMAP_FADE_TO_VALUE     = 0.825f;   // @0x827B46A8 -> +0x1518
    const f32 KF_DYNAMIC_FAR_CLIP_OFFSET     = 10.0f;    // @0x827B46A4 -> +0x1514
    const f32 KF_VARIABLE_BIAS_MIN           = 0.51f;    // 0x3F028F5C  -> +0x1520
    const f32 KF_BIAS_FRUSTUM_LENGTH         = 500.0f;   // @0x827B46C0 -> +0x1524

    // FLAG: rodata not recovered (values live in the un-dumped 0x820CA81C..0x820CA8A4
    // block the Construct loops index; the names/extents are DWARF-attested).
    const BrnWorld::ShadowMapType KA_SHADOWMAPTYPE[KU_NUM_SHADOW_MAPS] =
        { E_SHADOWMAP_TYPE_ORTHO, E_SHADOWMAP_TYPE_ORTHO, E_SHADOWMAP_TYPE_ORTHO };  // dword_820CA81C -- FLAG: rodata not recovered
    const f32 KAF_ORTHO_SCALE[KU_NUM_SHADOW_MAPS]                      = { 0.0f, 0.0f, 0.0f }; // flt_820CA870 -- FLAG: rodata not recovered
    const f32 KAF_CENTRE_AT_OFFSET[KU_NUM_SHADOW_MAPS]                 = { 0.0f, 0.0f, 0.0f }; // flt_820CA87C -- FLAG: rodata not recovered
    const f32 KAF_SHADOWMAP_SUBSET_FRUSTUM_NEAR_CLIP[KU_NUM_SHADOW_MAPS] = { 0.0f, 0.0f, 0.0f }; // flt_820CA88C -- FLAG: rodata not recovered
    const f32 KAF_SHADOWMAP_SUBSET_FRUSTUM_FAR_CLIP[KU_NUM_SHADOW_MAPS]  = { 0.0f, 0.0f, 0.0f }; // flt_820CA898 -- FLAG: rodata not recovered

    // DWARF BrnShadowMap.cpp:10 -- `const StringList[5] KA_SHADOWMAP_TYPE_OPTIONS`
    // (unk_820CA828), the option table Construct binds to each per-CSM "Type"
    // variable. FLAG: rodata (the four names + terminator) not recovered.
    const CgsDev::DebugUI::StringList KA_SHADOWMAP_TYPE_OPTIONS[5] =
    {
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },  // FLAG: rodata not recovered
    };

    // ========================================================================
    // BrnWorld::ShadowMap::Construct @ 0x827B43E8
    //
    // Initialise the shadow-map manager: seed the render toggles, the per-slot
    // configuration (ortho scale / centre-at offset / type / enabled + the
    // cached-matrix identity), the TSM sub-frustum table, the fade/bias tuning
    // scalars, then register the whole debug-variable surface under
    // "World\ShadowMap" (the CgsDev::DebugInterface automatic-handle idiom --
    // the X360 inlines the manager assert + critical-section enter/leave that
    // the committed DebugInterface default ctor / scope-exit models; same
    // rendering as WorldEntityModule::Construct @0x82302398).
    //
    // Every store below is asm-attested (r28 == 0 / r11 == 1 byte stores; the
    // float immediates resolved by the dossier pseudocode). Notably NOT
    // initialised by this body: muCurrentShadowMap (+0x14F4), mpTextureState /
    // mTextureStateParams / mTextureStateResource, mfNDotLFallOffCutoffWorld /
    // mfNDotLFallOffScaleWorld, and the camera/frustum aggregates themselves.
    // ========================================================================
    void ShadowMap::Construct()
    {
        // ---- render toggles + tuning scalars (0x827B4434..0x827B44E0) -------
        mbRenderTrafficIntoShadowMap  = false;                        // stb r28, 0x14FB
        mbRenderShadowMapView         = true;                         // stb r11, 0x14F8
        mbRenderWorldIntoShadowMap    = true;                         // stb r11, 0x14F9
        mfShadowMapNearPlane          = KF_SHADOWMAP_NEAR_PLANE_OFFSET; // stfs 0x14D0
        mbRenderRaceCarsIntoShadowMap = true;                         // stb r11, 0x14FA
        mShadowMapFocusPoint.SetZero();                   // stvx128 zero @+0x14E0
        mbRenderMultipleShadowMaps    = true;                         // stb r11, 0x14FC
        mfShadowMapFarPlane           = KF_SHADOWMAP_FAR_PLANE_OFFSET; // stfs 0x14D4
        mbRenderPropsIntoShadowMap    = true;                         // stb r11, 0x14FD
        mfEyeOffset                   = KF_SHADOWMAP_EYE_OFFSET;      // stfs 0x14D8
        mfIdealAspectRatio            = 2.0f;                         // flt_82001D9C, stfs 0x1508
        mbRenderPropsNearOnly         = false;                        // stb r28, 0x14FE
        mbRenderTrafficNearOnly       = false;                        // stb r28, 0x14FF
        mbRenderRaceCarsNearOnly      = false;                        // stb r28, 0x1500
        mbRenderingShadowMap          = false;                        // stb r28, 0(this)
        mbUseZOnlyRenderingPath       = true;                         // stb r11, 1(this)
        mbOptimiseForIdealAspectRatio = true;                         // stb r11, 0x1505
        mbShowLightFrustumInDebugRender = false;                      // stb r28, 0x1504

        // ---- per-slot configuration (loop @0x827B4524..0x827B45E0) ----------
        for (u32 luMap = 0; luMap < KU_NUM_SHADOW_MAPS; ++luMap)
        {
            mafShadowMapOrthoScale[luMap] = KAF_ORTHO_SCALE[luMap];       // stfs -0xC(r8)
            mafShadowMapAtOffset[luMap]   = KAF_CENTRE_AT_OFFSET[luMap];  // stfs 0(r8)
            maShadowMapTypes[luMap]       = KA_SHADOWMAPTYPE[luMap];      // stw 0x194(r8)
            mabMapEnabled[luMap]          = true;                         // stb r11, 0(r7)

            // maCachedMatrix[luMap] = the identity basis with a ZERO fourth row
            // (the four stvx128 @r9-0x20/-0x10/+0/+0x10, stride 0x40): rows
            // {1,0,0,0} {0,1,0,0} {0,0,1,0} {0,0,0,0} exactly as stored.
            maCachedMatrix[luMap].xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
            maCachedMatrix[luMap].yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
            maCachedMatrix[luMap].zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
            maCachedMatrix[luMap].wAxis = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        }

        mbUpdateDebugRender = true;                                   // stb r11, 0x1501
        mbUpdateTsmCamera   = true;                                   // stb r11, 0x1502
        mCachedOffsetWorld.SetZero();                     // stvx128 zero @+0x940

        // ---- TSM sub-frustum table (loop @0x827B4638..0x827B465C) -----------
        for (u32 luMap = 0; luMap < KU_NUM_SHADOW_MAPS; ++luMap)
        {
            maTsmBBInfo[luMap].mfNearClip      = KAF_SHADOWMAP_SUBSET_FRUSTUM_NEAR_CLIP[luMap]; // stfs -4(r10)
            maTsmBBInfo[luMap].mfFarClip       = KAF_SHADOWMAP_SUBSET_FRUSTUM_FAR_CLIP[luMap];  // stfs 0(r10)
            maTsmBBInfo[luMap].mbInvertCullMode = true;               // stb r11, 0x23C(r10)
            maTsmBBInfo[luMap].mfTsmSlideBack  = -0.6f;               // flt_820BC9D4, stfs 0x30C(r10)
            maTsmBBInfo[luMap].mbDebugRender   = false;               // stb r28, 0x310(r10)
        }

        mbPreferShortAndFatProjection = true;                         // stb r11, 0x1503
        mbDynamicFarClipPlane         = false;                        // stb r28, 0x151C
        mfFadeStartDistance           = KF_FADE_START_DISTANCE;       // stfs 0x14F0
        mfDynamicFarClipOffset        = KF_DYNAMIC_FAR_CLIP_OFFSET;   // stfs 0x1514
        mfShadowFadeToValue           = KF_SHADOWMAP_FADE_TO_VALUE;   // stfs 0x1518
        mfVariableBiasMin             = KF_VARIABLE_BIAS_MIN;         // stw 0x3F028F5C -> 0x1520
        mfBiasFrustumLength           = KF_BIAS_FRUSTUM_LENGTH;       // stfs 0x1524

        // ---- debug-variable registration (0x827B46CC..0x827B4938) -----------
        // The automatic DebugInterface handle = the inlined DebugManager
        // singleton-assert + DebugCriticalSection Enter/Leave pair.
        {
            CgsDev::DebugInterface lDebugInterface;
            char lacPath[64];   // the SnPrintf scratch (v76, 64 bytes)

            lDebugInterface.RegisterVariable( &mbUseZOnlyRenderingPath,      "World\\ShadowMap", "Z-only rendering" );
            lDebugInterface.RegisterVariable( &mbRenderShadowMapView,        "World\\ShadowMap", "Render Shadow map" );
            lDebugInterface.RegisterVariable( &mbRenderWorldIntoShadowMap,   "World\\ShadowMap", "Cast shadows from world" );
            lDebugInterface.RegisterVariable( &mbRenderRaceCarsIntoShadowMap,"World\\ShadowMap", "Cast shadows from cars" );
            lDebugInterface.RegisterVariable( &mbRenderTrafficIntoShadowMap, "World\\ShadowMap", "Cast shadows from traffic" );
            lDebugInterface.RegisterVariable( &mbRenderPropsIntoShadowMap,   "World\\ShadowMap", "Cast shadows from props" );
            lDebugInterface.RegisterVariable( &mfFadeStartDistance,          "World\\ShadowMap", "Fade start distance" );
            lDebugInterface.RegisterVariable( &mbRenderMultipleShadowMaps,   "World\\ShadowMap", "Render multiple shadow maps (CSM)" );

            for (u32 luCsm = 0; luCsm < KU_NUM_SHADOW_MAPS; ++luCsm)
            {
                CgsCore::SnPrintf( lacPath, 63, "World\\ShadowMap\\CSM %d", luCsm );
                lDebugInterface.RegisterVariable( &mafShadowMapAtOffset[luCsm],   lacPath, "Offset" );
                lDebugInterface.RegisterVariable( &mafShadowMapOrthoScale[luCsm], lacPath, "Scale" );
                lDebugInterface.RegisterVariable( &mabMapEnabled[luCsm],          lacPath, "Enabled" );
            }

            lDebugInterface.RegisterVariable( &mfShadowMapNearPlane, "World\\ShadowMap", "Near plane offset" );
            lDebugInterface.SetStep( &mfShadowMapNearPlane, 1.0f );
            lDebugInterface.RegisterVariable( &mfShadowMapFarPlane, "World\\ShadowMap", "Far plane offset" );
            lDebugInterface.SetStep( &mfShadowMapFarPlane, 5.0f );
            lDebugInterface.RegisterVariable( &mbDynamicFarClipPlane,  "World\\ShadowMap", "Dynamic far clip plane" );
            lDebugInterface.RegisterVariable( &mfDynamicFarClipOffset, "World\\ShadowMap", "Dynamic far clip offset" );
            lDebugInterface.RegisterVariable( &mfEyeOffset, "World\\ShadowMap", "Eye offset" );
            lDebugInterface.SetStep( &mfEyeOffset, 5.0f );
            lDebugInterface.RegisterVariable( &mbRenderRaceCarsNearOnly, "World\\ShadowMap", "RaceCars Near Shadowmap Only" );
            lDebugInterface.RegisterVariable( &mbRenderTrafficNearOnly,  "World\\ShadowMap", "Traffic Near Shadowmap Only" );
            lDebugInterface.RegisterVariable( &mbRenderPropsNearOnly,    "World\\ShadowMap", "Props Near Shadowmap Only" );
            lDebugInterface.RegisterVariable( &mfVariableBiasMin, "World\\ShadowMap", "Variable bias min" );
            lDebugInterface.SetStep( &mfVariableBiasMin, 0.05f );
            lDebugInterface.RegisterVariable( &mfBiasFrustumLength, "World\\ShadowMap", "Bias frustum length" );
            lDebugInterface.SetStep( &mfBiasFrustumLength, 10.0f );
            lDebugInterface.RegisterVariable( &mbUpdateDebugRender,          "World\\ShadowMap\\BB,TSM", "Update DebugRender" );
            lDebugInterface.RegisterVariable( &mbUpdateTsmCamera,            "World\\ShadowMap\\BB,TSM", "Update TSM Camera" );
            lDebugInterface.RegisterVariable( &mbPreferShortAndFatProjection,"World\\ShadowMap\\BB,TSM", "Prefer short and fat projection" );
            lDebugInterface.RegisterVariable( &mbOptimiseForIdealAspectRatio,"World\\ShadowMap\\BB,TSM", "Optimise for ideal aspect ratio" );
            lDebugInterface.RegisterVariable( &mfIdealAspectRatio, "World\\ShadowMap\\BB,TSM", "Ideal aspect ratio" );
            lDebugInterface.SetStep( &mfIdealAspectRatio, 0.1f );
            lDebugInterface.RegisterVariable( &mfShadowFadeToValue, "World\\ShadowMap", "Fade to value" );
            lDebugInterface.SetStep( &mfShadowFadeToValue, 0.025f );
            lDebugInterface.SetRange( &mfShadowFadeToValue, 0.0f, 1.0f );
            lDebugInterface.RegisterVariable( &mbShowLightFrustumInDebugRender, "World\\ShadowMap\\BB,TSM", "Show light frustum (DR)" );

            for (u32 luCsm = 0; luCsm < KU_NUM_SHADOW_MAPS; ++luCsm)
            {
                CgsCore::SnPrintf( lacPath, 63, "World\\ShadowMap\\BB,TSM\\CSM %d", luCsm );
                // The per-slot type is registered through the s32 mirror (the X360
                // 0x8282E3B8 overload); ShadowMapType is the 4-byte enum slot.
                lDebugInterface.RegisterVariable( reinterpret_cast<s32*>( &maShadowMapTypes[luCsm] ), lacPath, "Type" );
                lDebugInterface.SetOptions( reinterpret_cast<s32*>( &maShadowMapTypes[luCsm] ), KA_SHADOWMAP_TYPE_OPTIONS );
                lDebugInterface.RegisterVariable( &maTsmBBInfo[luCsm].mfFarClip,  lacPath, "Subfrustum Far Clip" );
                lDebugInterface.RegisterVariable( &maTsmBBInfo[luCsm].mfNearClip, lacPath, "Subfrustum Near Clip" );
                lDebugInterface.RegisterVariable( &maTsmBBInfo[luCsm].mbInvertCullMode, lacPath, "Invert cull mode" );
                lDebugInterface.RegisterVariable( &maTsmBBInfo[luCsm].mfTsmSlideBack, lacPath, "TSM slideback" );
                lDebugInterface.SetStep( &maTsmBBInfo[luCsm].mfTsmSlideBack, 0.05f );
                lDebugInterface.RegisterVariable( &maTsmBBInfo[luCsm].mbDebugRender, lacPath, "DebugRender" );
            }
        }
    }

    // ========================================================================
    // Inlined-on-console accessors. None of these has a standalone X360 body;
    // each is the member read/write the callers inline (semantics attested at
    // WorldModule::GenerateFrustumQueries @0x827DADF8 and WorldModule::
    // GenerateShadowMapDispatchLists @0x827C96D8 -- the offsets in the comments
    // are the console loads/stores those bodies perform on the embedded
    // ShadowMap at WorldModule+6170336).
    // ========================================================================

    // @0x827DADF8: the whole shadow path gates on the byte at ShadowMap+0x14F8
    // (mbRenderShadowMapView -- the "Render Shadow map" debug toggle).
    bool ShadowMap::IsEnabled() const                    { return mbRenderShadowMapView; }

    // @0x827DADF8 / @0x827C96D8: the per-cascade camera the frustum queries and
    // the dispatch feed read (viewproj rows @ +0x430 + 0x170*i + 0x80 ==
    // maCgsShadowMapCamera[i].mViewProjection).
    const CgsGraphics::Camera* ShadowMap::GetCascadeCamera( s32 liCascade ) const
    {
        return &maCgsShadowMapCamera[liCascade];
    }

    // @0x827C96D8: `*(ShadowMap+0x14F4) = luCascade` at the top of each cascade pass.
    void ShadowMap::SetCurrentCascadeIndex( u32 luCascade ) { muCurrentShadowMap = luCascade; }

    // @0x827C96D8: `*(ShadowMap+0) = 1` before the cascade dispatch (and the
    // matching clear on exit) -- the latch WorldEntityModule::GenerateDispatchLists
    // reads back through IsRenderingShadowMap.
    void ShadowMap::SetRenderingShadowMap( bool lbValue ) { mbRenderingShadowMap = lbValue; }

    // @0x827C96D8 (byte reads on the toggle block at +0x14F9..+0x1500).
    bool ShadowMap::GetRenderWorldIntoShadowMap() const    { return mbRenderWorldIntoShadowMap; }
    bool ShadowMap::GetRenderRaceCarsIntoShadowMap() const { return mbRenderRaceCarsIntoShadowMap; }
    bool ShadowMap::GetRenderTrafficIntoShadowMap() const  { return mbRenderTrafficIntoShadowMap; }
    bool ShadowMap::GetRenderMultipleShadowMaps() const    { return mbRenderMultipleShadowMaps; }
    bool ShadowMap::GetRenderPropsIntoShadowMap() const    { return mbRenderPropsIntoShadowMap; }
    bool ShadowMap::GetRenderPropsNearOnly() const         { return mbRenderPropsNearOnly; }
    bool ShadowMap::GetRenderTrafficNearOnly() const       { return mbRenderTrafficNearOnly; }
    bool ShadowMap::GetRenderRaceCarsNearOnly() const      { return mbRenderRaceCarsNearOnly; }

    // ========================================================================
    // BrnWorld::ShadowMap::SetConstantsForEnvmap @ 0x827C1AD0
    //
    // Stage the two environment-map shadow constants: c15 = {5,5,5,1} (the
    // env-map shadow tint/attenuation vector, flt_8200426C == 5.0 splat with a
    // 1.0 w) and c16 = {5, 1, mfBiasFrustumLength / (far - near), 0.2} -- the
    // bias term rides lane 2 (the vrefp + NR reciprocal of the +0x14D4-0x14D0
    // plane span, de-optimised exact).
    // ========================================================================
    void ShadowMap::SetConstantsForEnvmap()
    {
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 15u, Vector4{ 5.0f, 5.0f, 5.0f, 1.0f } );

        const f32 lfBias = mfBiasFrustumLength / ( mfShadowMapFarPlane - mfShadowMapNearPlane );
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 16u, Vector4{ 5.0f, 1.0f, lfBias, 0.2f } );
    }

    // ========================================================================
    // BrnWorld::ShadowMap::ComputeTSMMatrix @ 0x827BFF58 (wave-2 VMX pass)
    //
    // The Trapezoidal-Shadow-Map (Martin & Tan TSM) best-fit solve for shadow
    // map slot luIndex. The whole ~1450-instruction VMX body was decoded with
    // a symbolic evaluator over the asm (every vmaddfp/vnmsubfp operand order
    // validated by the Newton-Raphson chains collapsing to exact
    // reciprocals); the C++ below reproduces every store and every branch.
    //
    // Pipeline:
    //   1. if (mbUpdateTsmCamera): refresh mWorldToLight from the CGS shadow
    //      camera's view-projection, clone lrCamera into the TSM camera,
    //      clamp its near/far to [mfNearClip, mfFarClip] (fsel max/min),
    //      rebuilding the projection after each.
    //   2. Get the TSM camera frustum, convert (SetFromRwFrustum),
    //      CalcVertices -> 8 verts.
    //   3. Transform the 8 verts into the 2D post-projective light plane:
    //      each world-to-light basis ROW has its Z lane cleared (vrlimi128
    //      mask=2), so q.z == 0 while q.w keeps the real projective w.
    //   4. Face centres Cn (first 4) / Cf (last 4), midpoint M, centre-line
    //      unit dir U, left normal N = (U.y, -U.x, 0).
    //   5. Min/max of dot(U, q_i) -> extreme points kn (top) / kx (base).
    //   6. TSM slide-back eye: E = Pnear - U * (L*delta*(1+sb)) /
    //      (L - 2*delta - L*sb), delta = dot(M,U) - min, L = max - min,
    //      sb = mfTsmSlideBack, Pnear/Pfar = the centre-line points at min/max.
    //   7. Silhouette scan: for each q_i, the ray eye->q_i; if all other
    //      points lie on one side of that ray, q_i is a silhouette point
    //      (side A = ">=", side B = "<"). Optional global flag swaps sides.
    //   8. Trapezoid corners: intersect the base line (through kx, direction
    //      N) and the top line (through kn, direction N) with the two
    //      silhouette rays.
    //   9. Compose the trapezoid-to-unit-square map T1*R*T2*H*S1*N*T3*S2
    //      (translate mid-top, rotate top edge, translate eye, shear, scale,
    //      the projective y-divide matrix, centre the projective y range,
    //      final y scale) -> mBestFitMatrix.
    //  10. if (!mbInvertCullMode): post-multiply mBestFitMatrix by
    //      diag(-1,1,1,1).
    //  11. GetCgsFrustumParallel on the CGS shadow camera -> maFrustum[luIndex].
    //  12. if (mbUpdateDebugRender): fill gaTsmDebugRenderInfo[luIndex]
    //      (world-space frustum verts, trapezoid corners, per-stage-matrix
    //      corners, silhouette/extreme points, the light->world inverse) for
    //      the DebugRender line lists.
    //
    // Layout notes attested by this body: maCgsShadowMapCamera is the array
    // at X360 this+0x430 (the 368-byte stride == sizeof(CgsGraphics::Camera)
    // pins it as the Cgs camera array -- the maShadowMapCamera slots ahead of
    // it are the 352-byte director-style cameras: 16 + 3*352 == 1072 ==
    // 0x430); maTsmBBInfo at this+0x9C0 (stride 0x320: mfNearClip +0x00,
    // mfFarClip +0x04, mCamera +0x10, mWorldToLight +0x180, mbInvertCullMode
    // +0x240, mBestFitMatrix +0x250, mfTsmSlideBack +0x310); maFrustum at
    // this+0x1340 (stride 0x80 == sizeof(CgsGeometric::Frustum));
    // mbUpdateDebugRender @+0x1501 / mbUpdateTsmCamera @+0x1502 (pinned by
    // the committed muCurrentShadowMap @0x14F4 + the DWARF bool run
    // :263..:275). This body also attests the CgsGraphics::Camera projection-
    // scalar roles: maProjectionScalars[7] (+0x15C) = near clip, [8] (+0x160)
    // = far clip.
    //
    // RODATA / GLOBALS: flt_82004EF4 == 4.0f, flt_82001DA0 == 0.5f,
    // flt_82001D9C == 2.0f (values resolved by the dossier pseudocode).
    // unk_82CDA350 / unk_82CDA3C0 / unk_82CDA400 vperm lane controls: NOT
    // dumped, and NOT needed -- per the committed SeparatingDirection.cpp
    // precedent their observable lane routing is algebraically forced by use
    // (82CDA350 builds the 2D perpendicular (y, -x); 82CDA3C0/82CDA400 only
    // feed vsldoi row-assembly whose surviving lanes are pinned by the
    // affine-row structure -- identity/translation/scale rows verified by the
    // symbolic evaluation). gIVector/unk_82181510/20/30 are the four
    // identity-matrix rows (committed precedent: rendered as SetIdentity()).
    // ========================================================================

    // byte_8300FB34 @0x827C05C0: when set, the silhouette sides found by the
    // scan are swapped (A<->B, ray dirs too). FLAG: holder TU / default value
    // not recovered (debug-variable style toggle); carried as `false`, the
    // inert default.
    static bool sbTsmSwapSilhouetteSides = false; // FLAG: default not recovered

    // dword_82F30E24 @0x827C12F8: selects which composition-stage matrix the
    // debug fill maps the trapezoid corners through (index into the 11-entry
    // stage list below). FLAG: holder TU / default not recovered; 0 (identity
    // stage) is inert.
    static s32 siTsmDebugMatrixStage = 0;         // FLAG: default not recovered

    // unk_8300E220 -- the per-slot TSM debug-render blob (640 bytes each),
    // consumed by the declaration-only ShadowMap::DebugRender
    // ("gTsmDebugRenderInfo"). Field names are semantic (what this function
    // stores); the offsets are the asm ground truth.
    struct TsmDebugRenderInfo
    {
        Vector4  maFrustumVertsWorld[8];      // +0x000  raw CalcVertices output
        Vector4  maTrapezoidCornersWorld[4];  // +0x080  corners/w -> light-to-world
        Vector4  mFarCentreWorld;             // +0x0C0  Pfar
        Vector4  mNearCentreWorld;            // +0x0D0  Pnear
        Vector4  mEyeWorld;                   // +0x0E0  E
        Vector4  mMidPointWorld;              // +0x0F0  M
        CgsGeometric::Frustum mFrustum;       // +0x100  memcpy'd TSM camera frustum
        Vector4  maStageCornersWorld[4];      // +0x180  corners through the selected stage
        Vector4  mSilhouetteAWorld;           // +0x1C0  A
        Vector4  mSilhouetteBWorld;           // +0x1D0  B
        Vector4  mSilhouetteADirWorld;        // +0x1E0  A + rayDirA
        Vector4  mSilhouetteBDirWorld;        // +0x1F0  B + rayDirB
        Vector4  mMaxPointWorld;              // +0x200  kx
        Vector4  mMinPointWorld;              // +0x210  kn
        Vector4  mMaxPerpWorld;               // +0x220  kx + N
        Vector4  mMinPerpWorld;               // +0x230  kn + N
        Matrix44 mLightToWorld;               // +0x240  Inverse(mWorldToLight)
    };
    static_assert(sizeof(TsmDebugRenderInfo) == 640, "TsmDebugRenderInfo must be 640 bytes (X360 stride)");
    TsmDebugRenderInfo gaTsmDebugRenderInfo[KU_NUM_SHADOW_MAPS];

    // ------------------------------------------------------------------------
    // Local helpers -- the inlined rw::math::vpu shapes the body is built from.
    // ------------------------------------------------------------------------
    namespace
    {
        // Inlined rw::math::vpu::Mult(Vector4, Matrix44): the vspltw + vmaddfp
        // row combine (out[k] = sum_i v[i] * row_i[k]).
        Vector4 MultRow(const Vector4& lrV, const Matrix44& lrM)
        {
            const Vector4* lapRows[4] = { &lrM.xAxis, &lrM.yAxis, &lrM.zAxis, &lrM.wAxis };
            const f32 lafLanes[4] = { lrV.x, lrV.y, lrV.z, lrV.w };
            Vector4 lvOut; lvOut.SetZero();
            f32* lpfOut = &lvOut.x;
            for (int liRow = 0; liRow < 4; ++liRow)
            {
                const f32* lpfRow = &lapRows[liRow]->x;
                for (int liLane = 0; liLane < 4; ++liLane)
                {
                    lpfOut[liLane] += lafLanes[liRow] * lpfRow[liLane];
                }
            }
            return lvOut;
        }

        // Inlined rw::math::vpu::Mult(Matrix44, Matrix44): four row applications.
        Matrix44 MultMatrix(const Matrix44& lrA, const Matrix44& lrB)
        {
            Matrix44 lOut;
            lOut.xAxis = MultRow(lrA.xAxis, lrB);
            lOut.yAxis = MultRow(lrA.yAxis, lrB);
            lOut.zAxis = MultRow(lrA.zAxis, lrB);
            lOut.wAxis = MultRow(lrA.wAxis, lrB);
            return lOut;
        }

        // The debug fill's light-to-world point transform: affine combine of
        // (x, y, z, 1) with the inverse rows.
        Vector4 LightToWorldPoint(const Matrix44& lrInv, f32 lfX, f32 lfY, f32 lfZ)
        {
            Vector4 lvP; lvP.x = lfX; lvP.y = lfY; lvP.z = lfZ; lvP.w = 1.0f;
            return MultRow(lvP, lrInv);
        }

        // 2D line/ray intersection used for all four trapezoid corners
        // (0x827C05E8..0x827C07E0): the line through lrLinePoint with
        // direction N = (lfNx, lfNy) meets the ray through lrRayPoint with
        // direction (lfDirX, lfDirY) at lrLinePoint + N * s. The reciprocal
        // is the X360 vrefp + 2x Newton-Raphson, de-optimised exact. Result
        // carries w = 1 (vrlimi128 mask=1 insert of the 1.0 broadcast), z = 0.
        Vector4 IntersectPerpLineWithRay(const Vector4& lrLinePoint,
                                         f32 lfNx, f32 lfNy,
                                         const Vector4& lrRayPoint,
                                         f32 lfDirX, f32 lfDirY)
        {
            const f32 lfDenom = lfDirY * lfNx - lfDirX * lfNy;   // == dot(dir, U)
            const f32 lfNumer = lfDirX * (lrLinePoint.y - lrRayPoint.y)
                              - lfDirY * (lrLinePoint.x - lrRayPoint.x);
            const f32 lfS = lfNumer / lfDenom;

            Vector4 lvOut;
            lvOut.x = lrLinePoint.x + lfNx * lfS;
            lvOut.y = lrLinePoint.y + lfNy * lfS;
            lvOut.z = 0.0f;
            lvOut.w = 1.0f;
            return lvOut;
        }
    }

    void ShadowMap::ComputeTSMMatrix(u32 luIndex, CgsGraphics::Camera& lrCamera)
    {
        TsmBBInfo& lrTsm = maTsmBBInfo[luIndex];   // this + 0x9C0 + 0x320*luIndex

        // ---- 1. TSM camera refresh (0x827BFF7C..0x827C000C, gated) ------------
        if (mbUpdateTsmCamera)   // lbz 0x1502(this)
        {
            // mWorldToLight = the CGS shadow camera's view-projection (4 lvx/stvx
            // from this+0x4B0+0x170*i == maCgsShadowMapCamera[i].mViewProjection).
            lrTsm.mWorldToLight = maCgsShadowMapCamera[luIndex].mViewProjection;

            // Clone the passed camera into the TSM camera (r3 = &lrCamera source,
            // r4 = &lrTsm.mCamera destination).
            lrCamera.Clone(&lrTsm.mCamera);

            // Near clamp: scalar[7] = max(mfNearClip, cloned near) -- the branchless
            // fsel f0, (mfNearClip - camNear), mfNearClip, camNear @0x827BFFE8.
            // (This body attests maProjectionScalars[7]/[8] as the near/far scalars:
            // camera+0x15C / camera+0x160.)
            {
                f32& lrfCamNear = lrTsm.mCamera.maProjectionScalars[7];
                lrfCamNear = (lrTsm.mfNearClip - lrfCamNear >= 0.0f) ? lrTsm.mfNearClip
                                                                     : lrfCamNear;
                lrTsm.mCamera.UpdatePerspectiveProjectionMatrix();
            }

            // Far clamp: scalar[8] = min(cloned far, mfFarClip) -- fsel @0x827C0004.
            {
                f32& lrfCamFar = lrTsm.mCamera.maProjectionScalars[8];
                lrfCamFar = (lrTsm.mfFarClip - lrfCamFar >= 0.0f) ? lrfCamFar
                                                                  : lrTsm.mfFarClip;
                lrTsm.mCamera.UpdatePerspectiveProjectionMatrix();
            }
        }

        // ---- 2. Frustum -> 8 world-space vertices (0x827C004C..0x827C0064) ----
        CgsGraphics::CameraRwFrustum lRwFrustum;
        CgsGeometric::Frustum        lFrustum;
        Vector4                      laVerts[8];
        lrTsm.mCamera.GetFrustum(lRwFrustum);
        lFrustum.SetFromRwFrustum(lRwFrustum);
        lFrustum.CalcVertices(laVerts);

        // ---- 3. Into the 2D light plane (0x827C0014..0x827C0244) --------------
        // The four mWorldToLight rows are loaded with their Z lane cleared
        // (vrlimi128 v127/v125/v124/v123, v126(zero), mask=2), so every
        // transformed point has q.z == 0: the solve runs in the post-projective
        // light XY plane while q.w keeps the real projective w.
        Vector4 laQ[8];
        {
            const Vector4* lapRows[4] =
            {
                &lrTsm.mWorldToLight.xAxis, &lrTsm.mWorldToLight.yAxis,
                &lrTsm.mWorldToLight.zAxis, &lrTsm.mWorldToLight.wAxis
            };
            for (int liVert = 0; liVert < 8; ++liVert)
            {
                const Vector4& lrP = laVerts[liVert];
                const f32 lafLanes[4] = { lrP.x, lrP.y, lrP.z, 1.0f }; // affine seed = row3
                Vector4 lvQ; lvQ.SetZero();
                f32* lpfQ = &lvQ.x;
                for (int liRow = 0; liRow < 4; ++liRow)
                {
                    const f32* lpfRow = &lapRows[liRow]->x;
                    lpfQ[0] += lafLanes[liRow] * lpfRow[0];
                    lpfQ[1] += lafLanes[liRow] * lpfRow[1];
                    lpfQ[3] += lafLanes[liRow] * lpfRow[3];
                }
                lvQ.z = 0.0f;   // the cleared Z column
                laQ[liVert] = lvQ;
            }
        }

        // ---- 4. Face centres, midpoint, centre-line dir, perp -----------------
        // Cn/Cf = quarter sums (the vrefp(4.0) + 2x NR reciprocal == exact 0.25);
        // M = (Cn+Cf)*0.5; U = (Cf-Cn)/|Cf-Cn| (2D length -- q.z==0; the w lane
        // is scaled along, mirroring the vector op). N = (U.y, -U.x, 0) via the
        // unk_82CDA350 perpendicular shuffle + vrlimi z-clear.
        Vector4 lvCn; lvCn.SetZero();
        Vector4 lvCf; lvCf.SetZero();
        for (int liVert = 0; liVert < 4; ++liVert)
        {
            lvCn.x += laQ[liVert].x;     lvCn.y += laQ[liVert].y;     lvCn.w += laQ[liVert].w;
            lvCf.x += laQ[liVert + 4].x; lvCf.y += laQ[liVert + 4].y; lvCf.w += laQ[liVert + 4].w;
        }
        lvCn.x *= 0.25f; lvCn.y *= 0.25f; lvCn.w *= 0.25f;   // flt_82004EF4 == 4.0
        lvCf.x *= 0.25f; lvCf.y *= 0.25f; lvCf.w *= 0.25f;

        Vector4 lvMid;
        lvMid.x = (lvCn.x + lvCf.x) * 0.5f;                  // flt_82001DA0 == 0.5
        lvMid.y = (lvCn.y + lvCf.y) * 0.5f;
        lvMid.z = 0.0f;
        lvMid.w = (lvCn.w + lvCf.w) * 0.5f;

        Vector4 lvU;
        {
            const f32 lfDx = lvCf.x - lvCn.x;
            const f32 lfDy = lvCf.y - lvCn.y;
            // vmsum3fp128 + vrsqrtefp + 2x NR -> exact reciprocal length (z lane 0).
            const f32 lfInvLen = 1.0f / std::sqrt(lfDx * lfDx + lfDy * lfDy);
            lvU.x = lfDx * lfInvLen;
            lvU.y = lfDy * lfInvLen;
            lvU.z = 0.0f;
            lvU.w = (lvCf.w - lvCn.w) * lfInvLen;
        }
        const f32 lfNx = lvU.y;    // N = perp(U)
        const f32 lfNy = -lvU.x;

        // ---- 5. Min/max along the centre line (loop @0x827C036C..0x827C03C0) --
        f32 lfMinDot = lvU.x * laQ[0].x + lvU.y * laQ[0].y;
        f32 lfMaxDot = lfMinDot;
        Vector4 lvMinPoint = laQ[0];   // kn
        Vector4 lvMaxPoint = laQ[0];   // kx
        for (int liVert = 1; liVert < 8; ++liVert)
        {
            const f32 lfDot = lvU.x * laQ[liVert].x + lvU.y * laQ[liVert].y;
            if (lfMinDot > lfDot) { lfMinDot = lfDot; lvMinPoint = laQ[liVert]; }   // vcmpgtfp
            if (lfDot > lfMaxDot) { lfMaxDot = lfDot; lvMaxPoint = laQ[liVert]; }
        }

        // ---- 6. Centre-line points + slide-back eye (0x827C03C4..0x827C04B8) --
        // Pfar/Pnear = Cn + U*(dot - dot(Cn,U)); delta = dot(M,U) - min;
        // L = max-min; eye offset = L*delta*(1 + sb) / (L - 2*delta - L*sb);
        // E = Pnear - U*offset.
        const f32 lfCnDot = lvCn.x * lvU.x + lvCn.y * lvU.y;
        Vector4 lvPfar, lvPnear, lvEye;
        {
            const f32 lfTF = lfMaxDot - lfCnDot;
            lvPfar.x = lvCn.x + lvU.x * lfTF;  lvPfar.y = lvCn.y + lvU.y * lfTF;
            lvPfar.z = 0.0f;                   lvPfar.w = lvCn.w + lvU.w * lfTF;

            const f32 lfTN = lfMinDot - lfCnDot;
            lvPnear.x = lvCn.x + lvU.x * lfTN; lvPnear.y = lvCn.y + lvU.y * lfTN;
            lvPnear.z = 0.0f;                  lvPnear.w = lvCn.w + lvU.w * lfTN;

            const f32 lfSlideBack = lrTsm.mfTsmSlideBack;                 // tsm+0x310
            const f32 lfDelta     = lvMid.x * lvU.x + lvMid.y * lvU.y - lfMinDot;
            const f32 lfLength    = lfMaxDot - lfMinDot;
            const f32 lfDenom     = lfLength - lfDelta * 2.0f             // flt_82001D9C
                                  - lfLength * lfSlideBack;
            const f32 lfLD        = lfLength * lfDelta;
            const f32 lfOffset    = (lfLD * lfSlideBack + lfLD) / lfDenom; // vrefp + 2x NR
            lvEye.x = lvPnear.x - lvU.x * lfOffset;   // vxor sign flip + vmaddcfp128
            lvEye.y = lvPnear.y - lvU.y * lfOffset;
            lvEye.z = 0.0f;
            lvEye.w = lvPnear.w - lvU.w * lfOffset;
        }

        // ---- 7. Silhouette scan (8x8 loop @0x827C04BC..0x827C05BC) ------------
        // Defaults: both silhouette points = Pnear, both ray dirs = U.
        Vector4 lvSilA = lvPnear, lvSilB = lvPnear;
        f32 lfDirAx = lvU.x, lfDirAy = lvU.y;
        f32 lfDirBx = lvU.x, lfDirBy = lvU.y;
        for (u32 luI = 0; luI < 8; ++luI)
        {
            // n = normalize2(q_i - E) (vrsqrtefp + 2x NR, exact here; q.z==E.z==0).
            const f32 lfDx = laQ[luI].x - lvEye.x;
            const f32 lfDy = laQ[luI].y - lvEye.y;
            const f32 lfInvLen = 1.0f / std::sqrt(lfDx * lfDx + lfDy * lfDy);
            const f32 lfRx = lfDx * lfInvLen;
            const f32 lfRy = lfDy * lfInvLen;
            // ray plane = perp(n) = (n.y, -n.x, n.z==0) -- the unk_82CDA350
            // shuffle with vrlimi z <- splat(n,2).
            const f32 lfPx = lfRy;
            const f32 lfPy = -lfRx;
            const f32 lfThreshold = lfPx * laQ[luI].x + lfPy * laQ[luI].y;

            bool lbAnyGE = false, lbAnyLT = false;
            for (u32 luJ = 0; luJ < 8; ++luJ)
            {
                if (luJ == luI)
                {
                    continue;
                }
                const f32 lfSide = lfPx * laQ[luJ].x + lfPy * laQ[luJ].y;
                if (lfSide >= lfThreshold)   // vcmpgefp.
                {
                    lbAnyGE = true;
                }
                else
                {
                    lbAnyLT = true;
                }
            }
            if (lbAnyGE && !lbAnyLT)
            {
                lfDirAx = lfRx; lfDirAy = lfRy; lvSilA = laQ[luI];
            }
            else if (lbAnyLT && !lbAnyGE)
            {
                lfDirBx = lfRx; lfDirBy = lfRy; lvSilB = laQ[luI];
            }
        }
        if (sbTsmSwapSilhouetteSides)   // byte_8300FB34 @0x827C05C0
        {
            Vector4 lvT = lvSilB; lvSilB = lvSilA; lvSilA = lvT;
            f32 lfT;
            lfT = lfDirBx; lfDirBx = lfDirAx; lfDirAx = lfT;
            lfT = lfDirBy; lfDirBy = lfDirAy; lfDirAy = lfT;
        }

        // ---- 8. Trapezoid corners (0x827C05E8..0x827C07E0) --------------------
        // base line through kx, top line through kn, both with direction N; the
        // two silhouette rays cut them (w = 1 on every corner).
        const Vector4 lvC0 = IntersectPerpLineWithRay(lvMaxPoint, lfNx, lfNy, lvSilA, lfDirAx, lfDirAy); // base / ray A
        const Vector4 lvC1 = IntersectPerpLineWithRay(lvMaxPoint, lfNx, lfNy, lvSilB, lfDirBx, lfDirBy); // base / ray B
        const Vector4 lvC2 = IntersectPerpLineWithRay(lvMinPoint, lfNx, lfNy, lvSilB, lfDirBx, lfDirBy); // top  / ray B
        const Vector4 lvC3 = IntersectPerpLineWithRay(lvMinPoint, lfNx, lfNy, lvSilA, lfDirAx, lfDirAy); // top  / ray A

        // ---- 9. Trapezoid -> unit square composition (0x827C07E4..0x827C0F60) --
        // Stage rows assembled from the {0,1} perm/vsldoi constants; every
        // element verified by the symbolic evaluation of the store stream.

        // mid-top + T1 (translate mid-top to origin)
        Vector4 lvMidTop;
        lvMidTop.x = (lvC2.x + lvC3.x) * 0.5f;
        lvMidTop.y = (lvC2.y + lvC3.y) * 0.5f;
        lvMidTop.z = 0.0f;
        lvMidTop.w = 1.0f;
        Matrix44 lT1; lT1.SetIdentity();
        lT1.wAxis.x = -lvMidTop.x;
        lT1.wAxis.y = -lvMidTop.y;

        // R (rotate the top edge onto the x axis); u = normalize(c2 - c3) via
        // vmsum4fp128 + vrsqrtefp + 2x NR (z/w differences are zero).
        f32 lfUx, lfUy;
        {
            const f32 lfDx = lvC2.x - lvC3.x;
            const f32 lfDy = lvC2.y - lvC3.y;
            const f32 lfInvLen = 1.0f / std::sqrt(lfDx * lfDx + lfDy * lfDy);
            lfUx = lfDx * lfInvLen;
            lfUy = lfDy * lfInvLen;
        }
        Matrix44 lR; lR.SetIdentity();
        lR.xAxis.x =  lfUx; lR.xAxis.y = lfUy;
        lR.yAxis.x = -lfUy; lR.yAxis.y = lfUx;

        const Matrix44 lM2 = MultMatrix(lT1, lR);

        // T2 (translate the transformed eye to the origin)
        Vector4 lvEyeH = lvEye; lvEyeH.w = 1.0f;      // vrlimi w <- 1
        const Vector4 lvEyeT = MultRow(lvEyeH, lM2);
        Matrix44 lT2; lT2.SetIdentity();
        lT2.wAxis.x = -lvEyeT.x;
        lT2.wAxis.y = -lvEyeT.y;
        const Matrix44 lM3 = MultMatrix(lM2, lT2);

        // H (shear so the transformed mid-top sits on the y axis):
        // a = -midTop'.x / midTop'.y (vrefp + 2x NR).
        const Vector4 lvMidTopT = MultRow(lvMidTop, lM3);
        Matrix44 lH; lH.SetIdentity();
        lH.yAxis.x = -lvMidTopT.x / lvMidTopT.y;
        const Matrix44 lM4 = MultMatrix(lM3, lH);

        // S1 (scale the transformed top corner c2 to (1,1)) -- both reciprocals
        // vrefp + 2x NR, multiplied by the GetVecFloat_One numerator.
        const Vector4 lvC2T = MultRow(lvC2, lM4);
        Matrix44 lS1; lS1.SetIdentity();
        lS1.xAxis.x = 1.0f / lvC2T.x;
        lS1.yAxis.y = 1.0f / lvC2T.y;
        const Matrix44 lM5 = MultMatrix(lM4, lS1);

        // N (the projective trapezoid map): rows (1,0,0,0)/(0,1,0,1)/(0,0,1,0)/
        // (0,1,0,0) -- (x, y, z, w) -> (x, y + w, z, y). Row content pinned by
        // the evaluated store stream (perm-constant routing algebraically
        // forced; see the banner).
        Matrix44 lN; lN.SetIdentity();
        lN.yAxis.w = 1.0f;
        lN.wAxis.x = 0.0f; lN.wAxis.y = 1.0f; lN.wAxis.z = 0.0f; lN.wAxis.w = 0.0f;
        const Matrix44 lM6 = MultMatrix(lM5, lN);

        // T3 (centre the projective y range between base corner c0 and top
        // corner c2): ty = -0.5 * (y'(c0) + y'(c2)), y'(p) = (p*M6).y / (p*M6).w
        // (vrefp + 2x NR).
        const Vector4 lvC0N = MultRow(lvC0, lM6);
        const Vector4 lvC2N = MultRow(lvC2, lM6);
        const f32 lfTy = (lvC0N.y / lvC0N.w + lvC2N.y / lvC2N.w) * -0.5f;
        Matrix44 lT3; lT3.SetIdentity();
        lT3.wAxis.y = lfTy;
        const Matrix44 lM7 = MultMatrix(lM6, lT3);

        // S2 (final projective y scale, mapping the base corner to the far
        // edge): s = -(c0*M7).w / (c0*M7).y (vrefp + 2x NR, negated numerator).
        const Vector4 lvC0T = MultRow(lvC0, lM7);
        Matrix44 lS2; lS2.SetIdentity();
        lS2.yAxis.y = -lvC0T.w / lvC0T.y;
        lrTsm.mBestFitMatrix = MultMatrix(lM7, lS2);   // stores @0x827C0F48..0x827C0F60

        // ---- 10. Cull-mode fixup (0x827C0F64..0x827C1044) ----------------------
        // if (!mbInvertCullMode): post-multiply by diag(-1,1,1,1) -- the
        // constructed flip matrix rows verified from the perm/vsldoi assembly;
        // net effect: negate the X column.
        if (!lrTsm.mbInvertCullMode)   // lbz 0x240(tsm)
        {
            Matrix44 lFlipX; lFlipX.SetIdentity();
            lFlipX.xAxis.x = -1.0f;
            lrTsm.mBestFitMatrix = MultMatrix(lrTsm.mBestFitMatrix, lFlipX);
        }

        // ---- 11. CGS frustum refresh (0x827C1048..0x827C1060) ------------------
        // r3 = this+0x430+0x170*i (maCgsShadowMapCamera[i]), r4 = this+0x1340+
        // 0x80*i (maFrustum[i]). (Call shape reconciled 2026-07-27 to the DWARF
        // reference form -- CgsCamera.h:153.)
        maCgsShadowMapCamera[luIndex].GetCgsFrustumParallel(maFrustum[luIndex]);

        // ---- 12. Debug-render fill (0x827C11C4..0x827C161C, gated) -------------
        // The 11-entry stage list the selector indexes (built at
        // 0x827C1090..0x827C11EC; entry 0 = identity from the gIVector rows ==
        // SetIdentity, entry 10 = Mult(R, T1) -- the rotate-then-translate
        // variant).
        if (mbUpdateDebugRender)   // lbz 0x1501(this)
        {
            Matrix44 lIdentity; lIdentity.SetIdentity();        // gIVector/unk_821815x0 rows
            const Matrix44 lRT = MultMatrix(lR, lT1);
            const Matrix44* lapStages[11] =
            {
                &lIdentity,               // [0]
                &lT1,                     // [1]
                &lM2,                     // [2] T1*R
                &lM3,                     // [3] +T2
                &lM4,                     // [4] +H
                &lM5,                     // [5] +S1
                &lM6,                     // [6] +N
                &lM7,                     // [7] +T3
                &lrTsm.mBestFitMatrix,    // [8] final (S2 + cull fixup)
                &lR,                      // [9]
                &lRT,                     // [10]
            };

            TsmDebugRenderInfo& lrBlob = gaTsmDebugRenderInfo[luIndex]; // unk_8300E220 + 640*i

            // The debug depth: vert0 through the FULL (z-intact) world-to-light
            // rows; its light-space z is broadcast into the z lane of every 2D
            // point below (vperm zero + vrlimi128 insert @0x827C1254..0x827C1268).
            const Vector4 lvVert0H = { laVerts[0].x, laVerts[0].y, laVerts[0].z, 1.0f };
            const f32 lfZ = MultRow(lvVert0H, lrTsm.mWorldToLight).z;

            // light -> world (rw::math::vpu::Inverse @ 0x825B2628, bl at
            // 0x827C123C; determinant out is a dead local here).
            Vector4 lvDeterminant; lvDeterminant.SetZero();
            const Matrix44 lLightToWorld = rw::math::vpu::Inverse(lrTsm.mWorldToLight,
                                                                  lvDeterminant);

            std::memcpy(&lrBlob.mFrustum, &lFrustum, sizeof(CgsGeometric::Frustum)); // @0x827C1274

            lrBlob.mFarCentreWorld  = LightToWorldPoint(lLightToWorld, lvPfar.x,  lvPfar.y,  lfZ);
            lrBlob.mNearCentreWorld = LightToWorldPoint(lLightToWorld, lvPnear.x, lvPnear.y, lfZ);

            // Per-corner: (a) corner/w -> world; (b) corner through the selected
            // stage matrix, /w' -> world (loop @0x827C1310..0x827C1428).
            const Matrix44& lrStage = *lapStages[siTsmDebugMatrixStage]; // dword_82F30E24, unchecked index (as on console)
            const Vector4 lavCorners[4] = { lvC0, lvC1, lvC2, lvC3 };
            for (int liCorner = 0; liCorner < 4; ++liCorner)
            {
                const Vector4& lrC = lavCorners[liCorner];
                const f32 lfInvW = 1.0f / lrC.w;   // vrefp + 2x NR (w == 1 here)
                lrBlob.maTrapezoidCornersWorld[liCorner] =
                    LightToWorldPoint(lLightToWorld, lrC.x * lfInvW, lrC.y * lfInvW,
                                      lrC.z * lfInvW + lfZ);

                const Vector4 lvStaged = MultRow(lrC, lrStage);
                const f32 lfInvW2 = 1.0f / lvStaged.w;
                lrBlob.maStageCornersWorld[liCorner] =
                    LightToWorldPoint(lLightToWorld, lvStaged.x * lfInvW2,
                                      lvStaged.y * lfInvW2,
                                      lvStaged.z * lfInvW2 + lfZ);
            }

            // Eye / midpoint / silhouette / extreme points (0x827C142C..0x827C15EC).
            lrBlob.mEyeWorld      = LightToWorldPoint(lLightToWorld, lvEye.x, lvEye.y, lfZ);
            lrBlob.mMidPointWorld = LightToWorldPoint(lLightToWorld, lvMid.x, lvMid.y, lfZ);
            lrBlob.mSilhouetteAWorld    = LightToWorldPoint(lLightToWorld, lvSilA.x, lvSilA.y, lfZ);
            lrBlob.mSilhouetteADirWorld = LightToWorldPoint(lLightToWorld, lvSilA.x + lfDirAx,
                                                            lvSilA.y + lfDirAy, lfZ);
            lrBlob.mSilhouetteBWorld    = LightToWorldPoint(lLightToWorld, lvSilB.x, lvSilB.y, lfZ);
            lrBlob.mSilhouetteBDirWorld = LightToWorldPoint(lLightToWorld, lvSilB.x + lfDirBx,
                                                            lvSilB.y + lfDirBy, lfZ);
            lrBlob.mMaxPointWorld = LightToWorldPoint(lLightToWorld, lvMaxPoint.x, lvMaxPoint.y, lfZ);
            lrBlob.mMaxPerpWorld  = LightToWorldPoint(lLightToWorld, lvMaxPoint.x + lfNx,
                                                      lvMaxPoint.y + lfNy, lfZ);
            lrBlob.mMinPointWorld = LightToWorldPoint(lLightToWorld, lvMinPoint.x, lvMinPoint.y, lfZ);
            lrBlob.mMinPerpWorld  = LightToWorldPoint(lLightToWorld, lvMinPoint.x + lfNx,
                                                      lvMinPoint.y + lfNy, lfZ);

            // Raw world frustum verts + the inverse itself (0x827C15F0..0x827C161C).
            for (int liVert = 0; liVert < 8; ++liVert)
            {
                lrBlob.maFrustumVertsWorld[liVert] = laVerts[liVert];
            }
            lrBlob.mLightToWorld = lLightToWorld;
        }
    }

    // ========================================================================
    // Shadow-camera wave (2026-07-27): CalculateShadowMapCameras @0x827DA820,
    // SetConstants @0x827C16E0, ObjectCSMSelect @0x827C1630, plus the shared
    // per-frame BSS the constant stage reads. Every reconstructed body below
    // was decoded from the raw X360 instruction stream (member offsets resolved
    // against the DWARF layout; the VMX blocks decoded lane-by-lane; the
    // numeric-emulator tooling and per-function analyses live in the session
    // scratchpad vmx_wave_log.md).
    // ========================================================================

    // ------------------------------------------------------------------------
    // File-scope BSS the shadow constant stage shares with the bounding-box
    // solve (the X360 addresses are this TU's .bss block):
    //   unk_8300F470  -- the post-view-projection matrix the c14 upload
    //                    multiplies every cascade VP with (clip -> shadow UV).
    //   unk_8300F9A0  -- the per-map second-stage matrix triple (64B stride).
    //   unk_8300E9F0  -- the matrix substituted for DISABLED maps.
    //   unk_8300FB00  -- the row-3 (translation) add applied to every result.
    // FLAG (writers not recovered): no function in the .ida-exports set writes
    // these blobs -- the writers are expected inside the not-yet-reconstructed
    // ComputeBoundingBoxMatrix/ComputeOptimalViewVolume pair (whose sibling
    // stores hit the adjacent unk_8300F460/unk_8300F5C0/unk_8300F980 slots) or
    // in un-exported setup code. Carried as zero-initialised statics exactly
    // like the console BSS boot state; role names are semantic, not symbol
    // ground truth.
    // ------------------------------------------------------------------------
    static Matrix44 gShadowPostProjectionMatrix;                       // unk_8300F470 -- FLAG: writer not recovered
    static Matrix44 gaShadowConstantMatrix[KU_NUM_SHADOW_MAPS];        // unk_8300F9A0 -- FLAG: writer not recovered
    static Matrix44 gShadowDisabledConstantMatrix;                     // unk_8300E9F0 -- FLAG: writer not recovered
    static Vector4  gShadowConstantRowOffset;                          // unk_8300FB00 -- FLAG: writer not recovered

    namespace
    {
        // --------------------------------------------------------------------
        // The outlined Director camera-utils transform builder @0x8220C960
        // (asserts name its home: GameSource/Director/Camera/Utils/
        // CameraUtils.cpp:779-797). Expressed as a FILE-LOCAL helper because
        // that TU is outside this wave's ownership -- when CameraUtils.cpp is
        // reconstructed, re-home this body there and call it (the semantics
        // below are the full decode of the X360 body; the vpermwi128-0x63
        // pairs are the (y,z,x) cross-product rotate).
        //
        //   Z = IsZero(lrEyePoint - lrFocus) ? {0,0,1,0}   (unk_82181520, the
        //                                       identity z row)
        //                                    : normalize(lrEyePoint - lrFocus)
        //   assert IsValid(Z)  ("Invalid Z Axis after normalisation, ...":779)
        //   X = IsZero(cross(lrUpReference, Z)) ? {1,0,0,0} (gIVector)
        //                                       : normalize(cross(lrUpReference, Z))
        //   assert IsValid(X)  ("IsValid(lXaxis)":793)
        //   Y = cross(Z, X)                     (NOT normalised)
        //   assert !IsZero(Y)  ("!IsZero(lYaxis)":797)
        //   rows out: X, Y, Z, lrFocus
        //
        // FLAG: the IsZero epsilon (flt_82001770) is un-dumped rodata; carried
        // as the flagged constant below (component-abs threshold, matching the
        // vandc/vcmpgtfp shape).
        // --------------------------------------------------------------------
        const f32 KF_ISZERO_EPSILON = 1e-6f;   // flt_82001770 -- FLAG: value not recovered

        inline bool IsZero3(f32 lfX, f32 lfY, f32 lfZ)
        {
            // vandc(sign) + vcmpgtfp vs the epsilon splat, all-lanes-not-greater
            return !(std::fabs(lfX) > KF_ISZERO_EPSILON ||
                     std::fabs(lfY) > KF_ISZERO_EPSILON ||
                     std::fabs(lfZ) > KF_ISZERO_EPSILON);
        }

        inline bool IsValid3(f32 lfX, f32 lfY, f32 lfZ)
        {
            // the vcmpeqfp. self-compare NaN checks on lanes 0..2
            return lfX == lfX && lfY == lfY && lfZ == lfZ;
        }

        void BuildLightCameraTransform(Vector4* lapRowsOut,
                                       const Vector4& lrFocus,
                                       const Vector4& lrEyePoint,
                                       const Vector4& lrUpReference)
        {
            // ---- Z axis (0x8220C990..0x8220CA14) ----------------------------
            const f32 lfDx = lrEyePoint.x - lrFocus.x;
            const f32 lfDy = lrEyePoint.y - lrFocus.y;
            const f32 lfDz = lrEyePoint.z - lrFocus.z;
            Vector4 lvZ;
            if (IsZero3(lfDx, lfDy, lfDz))
            {
                lvZ = Vector4{ 0.0f, 0.0f, 1.0f, 0.0f };   // unk_82181520
            }
            else
            {
                const f32 lfInvLen = 1.0f / std::sqrt(lfDx * lfDx + lfDy * lfDy + lfDz * lfDz); // vrsqrtefp + 2x NR
                lvZ = Vector4{ lfDx * lfInvLen, lfDy * lfInvLen, lfDz * lfInvLen, 0.0f };
            }
            CGS_ASSERT(IsValid3(lvZ.x, lvZ.y, lvZ.z),
                       "Invalid Z Axis after normalisation, pre-normalised: ");   // CameraUtils.cpp:779

            // ---- X axis (0x8220CB00..0x8220CB98) ----------------------------
            const f32 lfCx = lrUpReference.y * lvZ.z - lrUpReference.z * lvZ.y;
            const f32 lfCy = lrUpReference.z * lvZ.x - lrUpReference.x * lvZ.z;
            const f32 lfCz = lrUpReference.x * lvZ.y - lrUpReference.y * lvZ.x;
            Vector4 lvX;
            if (IsZero3(lfCx, lfCy, lfCz))
            {
                lvX = Vector4{ 1.0f, 0.0f, 0.0f, 0.0f };   // w::math::vpu::detail::gIVector
            }
            else
            {
                const f32 lfInvLen = 1.0f / std::sqrt(lfCx * lfCx + lfCy * lfCy + lfCz * lfCz);
                lvX = Vector4{ lfCx * lfInvLen, lfCy * lfInvLen, lfCz * lfInvLen, 0.0f };
            }
            CGS_ASSERT(IsValid3(lvX.x, lvX.y, lvX.z), "IsValid(lXaxis)");        // CameraUtils.cpp:793

            // ---- Y axis (0x8220CC1C..0x8220CC50, unnormalised) --------------
            Vector4 lvY;
            lvY.x = lvZ.y * lvX.z - lvZ.z * lvX.y;
            lvY.y = lvZ.z * lvX.x - lvZ.x * lvX.z;
            lvY.z = lvZ.x * lvX.y - lvZ.y * lvX.x;
            lvY.w = 0.0f;
            CGS_ASSERT(!IsZero3(lvY.x, lvY.y, lvY.z), "!IsZero(lYaxis)");        // CameraUtils.cpp:797

            // ---- row stores (0x8220CC80..0x8220CC9C) ------------------------
            lapRowsOut[0] = lvX;
            lapRowsOut[1] = lvY;
            lapRowsOut[2] = lvZ;
            lapRowsOut[3] = lrFocus;
        }
    }

    // ========================================================================
    // BrnWorld::ShadowMap::CalculateShadowMapCameras @0x827DA820
    //
    // Per-frame shadow camera build. r3 = this, v1 = the key-light direction,
    // r4 = the DIRECTOR render camera (see the header reconcile note).
    //
    //   1. lightOffset = lv3LightDirection * splat(mfEyeOffset)   (vmulfp v127)
    //   2. per map i (loop @0x827DA8B4..0x827DA9E4):
    //        maShadowMapCamera[i] = *lpRenderCamera        (director operator=)
    //        maShadowMapCamera[i].mfFOV = 70.0f            (flt_820051BC @+0x58)
    //        luConfig = (maShadowMapTypes[i] != ORTHO) ? 0 : i
    //        focus = renderCam.At * splat(mafShadowMapAtOffset[luConfig])
    //              + renderCam.Pos                          (vmaddfp)
    //        mShadowMapFocusPoint = focus                  (stvx this+0x14E0;
    //                                                       each map overwrites
    //                                                       it -- last wins,
    //                                                       exactly as on X360)
    //        transform = BuildLightCameraTransform(focus, focus + lightOffset,
    //                                              renderCam.At)
    //        maShadowMapCamera[i].mTransform = transform   (4 stvx rows)
    //        maShadowMapCamera[i].ValidateTransformWithDebugInfo()
    //        maShadowMapCamera[i].CopyToCgsCamera(&maCgsShadowMapCamera[i])
    //        cgs.m_aspectRatio = 1.0f; cgs.SetFovHorizontal(cgs.m_fovHorizontal)
    //        cgs.UpdatePerspectiveProjectionMatrix()
    //        cgs.m_farClipPlane  = mfShadowMapFarPlane;  Update...
    //        cgs.m_nearClipPlane = mfShadowMapNearPlane; Update...
    //        cgs.UpdateOrthogonalProjectionMatrix(mafShadowMapOrthoScale[luConfig])
    //   3. lFrameCamera = CGS copy of the render camera (CopyToCgsCamera)
    //   4. per map i (loop @0x827DAA10..0x827DABFC), switch maShadowMapTypes[i]:
    //        ORTHO       -> maCgsShadowMapCamera[i].GetCgsFrustumParallel(maFrustum[i])
    //        TSM         -> ComputeTSMMatrix(i, lFrameCamera);
    //                       mProjection = mProjection * maTsmBBInfo[i].mBestFitMatrix;
    //                       UpdateViewProjectionMatrix()
    //        BOUNDINGBOX -> ComputeBoundingBoxMatrix(i, lFrameCamera); same
    //                       best-fit post-multiply + VP rebuild
    //        CACHED      -> nothing
    //   5. dynamic far clip (@0x827DAC00.., gated on mbDynamicFarClipPlane &&
    //      maShadowMapTypes[0] == BOUNDINGBOX): clone the frame camera, clamp
    //      its near/far to maTsmBBInfo[0].mfNearClip / maTsmBBInfo[2].mfFarClip
    //      (fsel max/min + projection rebuilds), CalcVertices of its frustum,
    //      run the 8 verts through cascade 0's VP keeping the max clip z,
    //      refit mfShadowMapFarPlane = near + (far-near)*maxZ + offset, then
    //      re-write every cascade's ortho z terms (zAxis.z = 1/(newFar-near),
    //      wAxis.z = -near/(newFar-near), the vrlimi lane-2 inserts) + VP.
    //   6. SetConstants(lpRenderCamera); DebugRender(i) for every map whose
    //      maTsmBBInfo[i].mbDebugRender toggle is set.
    // ========================================================================
    void ShadowMap::CalculateShadowMapCameras(Vector3 lv3LightDirection,
                                              const BrnDirector::Camera::Camera* lpRenderCamera)
    {
        // ---- 1. the light-direction eye offset (vmulfp128 v127, v1, splat) --
        Vector4 lvLightOffset;
        lvLightOffset.x = lv3LightDirection.x * mfEyeOffset;
        lvLightOffset.y = lv3LightDirection.y * mfEyeOffset;
        lvLightOffset.z = lv3LightDirection.z * mfEyeOffset;
        lvLightOffset.w = lv3LightDirection.w * mfEyeOffset;

        const rw::math::vpu::Vector3& lrAt  = lpRenderCamera->mTransform.At();   // r4+0x20
        const rw::math::vpu::Vector3& lrPos = lpRenderCamera->mTransform.Pos();  // r4+0x30

        // ---- 2. per-map director/CGS camera build ---------------------------
        for (u32 luMap = 0; luMap < KU_NUM_SHADOW_MAPS; ++luMap)
        {
            BrnDirector::Camera::Camera& lrDirCamera = maShadowMapCamera[luMap];
            lrDirCamera = *lpRenderCamera;                       // operator= @0x82233A80
            lrDirCamera.mfFOV = 70.0f;                           // flt_820051BC -> +0x58

            const u32 luConfig = (maShadowMapTypes[luMap] != E_SHADOWMAP_TYPE_ORTHO) ? 0u : luMap;

            // focus = At * atOffset + Pos (vmaddfp v0, v0, v13, v12)
            const f32 lfAtOffset = mafShadowMapAtOffset[luConfig];
            Vector4 lvFocus;
            lvFocus.x = lrAt.x * lfAtOffset + lrPos.x;
            lvFocus.y = lrAt.y * lfAtOffset + lrPos.y;
            lvFocus.z = lrAt.z * lfAtOffset + lrPos.z;
            lvFocus.w = lrAt.w * lfAtOffset + lrPos.w;
            mShadowMapFocusPoint.x = lvFocus.x;                  // stvx this+0x14E0
            mShadowMapFocusPoint.y = lvFocus.y;
            mShadowMapFocusPoint.z = lvFocus.z;
            mShadowMapFocusPoint.w = lvFocus.w;

            const Vector4 lvEyePoint{ lvFocus.x + lvLightOffset.x, lvFocus.y + lvLightOffset.y,
                                      lvFocus.z + lvLightOffset.z, lvFocus.w + lvLightOffset.w };
            const Vector4 lvUpReference{ lrAt.x, lrAt.y, lrAt.z, lrAt.w };

            // the outlined CameraUtils builder (bl sub_8220C960) -> mTransform rows
            Vector4 laRows[4];
            BuildLightCameraTransform(laRows, lvFocus, lvEyePoint, lvUpReference);
            lrDirCamera.mTransform.xAxis = rw::math::vpu::Vector3{ laRows[0].x, laRows[0].y, laRows[0].z, laRows[0].w };
            lrDirCamera.mTransform.yAxis = rw::math::vpu::Vector3{ laRows[1].x, laRows[1].y, laRows[1].z, laRows[1].w };
            lrDirCamera.mTransform.zAxis = rw::math::vpu::Vector3{ laRows[2].x, laRows[2].y, laRows[2].z, laRows[2].w };
            lrDirCamera.mTransform.wAxis = rw::math::vpu::Vector3{ laRows[3].x, laRows[3].y, laRows[3].z, laRows[3].w };

            lrDirCamera.ValidateTransformWithDebugInfo();        // bl @0x827DA974
            lrDirCamera.CopyToCgsCamera(&maCgsShadowMapCamera[luMap]);

            CgsGraphics::Camera& lrCgsCamera = maCgsShadowMapCamera[luMap];
            const f32 lfFov = lrCgsCamera.maProjectionScalars[0];   // lfs 0x140 (m_fovHorizontal)
            lrCgsCamera.maProjectionScalars[6] = 1.0f;              // stfs 0x158 (m_aspectRatio)
            lrCgsCamera.SetFovHorizontal(lfFov);
            lrCgsCamera.UpdatePerspectiveProjectionMatrix();
            lrCgsCamera.maProjectionScalars[8] = mfShadowMapFarPlane;   // stfs 0x160 <- +0x14D4
            lrCgsCamera.UpdatePerspectiveProjectionMatrix();
            lrCgsCamera.maProjectionScalars[7] = mfShadowMapNearPlane;  // stfs 0x15C <- +0x14D0
            lrCgsCamera.UpdatePerspectiveProjectionMatrix();
            lrCgsCamera.UpdateOrthogonalProjectionMatrix(mafShadowMapOrthoScale[luConfig]);
        }

        // ---- 3. the frame camera in CGS form (stack v73) --------------------
        CgsGraphics::Camera lFrameCamera;
        lpRenderCamera->CopyToCgsCamera(&lFrameCamera);

        // ---- 4. per-map projection solve ------------------------------------
        for (u32 luMap = 0; luMap < KU_NUM_SHADOW_MAPS; ++luMap)
        {
            CgsGraphics::Camera& lrCgsCamera = maCgsShadowMapCamera[luMap];
            switch (maShadowMapTypes[luMap])
            {
            case E_SHADOWMAP_TYPE_ORTHO:
                lrCgsCamera.GetCgsFrustumParallel(maFrustum[luMap]);
                break;

            case E_SHADOWMAP_TYPE_TSM:
                ComputeTSMMatrix(luMap, lFrameCamera);
                // mProjection = mProjection * mBestFitMatrix (the inlined
                // row-by-row vmaddfp block @0x827DAB10..0x827DABCC)
                lrCgsCamera.mProjection = MultMatrix(lrCgsCamera.mProjection,
                                                     maTsmBBInfo[luMap].mBestFitMatrix);
                lrCgsCamera.UpdateViewProjectionMatrix();
                break;

            case E_SHADOWMAP_TYPE_BOUNDINGBOX:
                ComputeBoundingBoxMatrix(luMap, lFrameCamera);
                // same best-fit post-multiply, own inlined copy @0x827DAA38..
                lrCgsCamera.mProjection = MultMatrix(lrCgsCamera.mProjection,
                                                     maTsmBBInfo[luMap].mBestFitMatrix);
                lrCgsCamera.UpdateViewProjectionMatrix();
                break;

            default:   // E_SHADOWMAP_TYPE_CACHED -- nothing (@0x827DAA20 bge)
                break;
            }
        }

        // ---- 5. dynamic far-clip refit (@0x827DAC00..0x827DADA0) ------------
        if (mbDynamicFarClipPlane && maShadowMapTypes[0] == E_SHADOWMAP_TYPE_BOUNDINGBOX)
        {
            // cascade 0's VP rows (lvx v127..v124 @ this+0x4B0)
            const Matrix44 lCascadeVP = maCgsShadowMapCamera[0].mViewProjection;

            CgsGraphics::Camera lClampedCamera;
            lFrameCamera.Clone(&lClampedCamera);
            {
                // near = max(maTsmBBInfo[0].mfNearClip, clone near) -- fsel @0x827DAC48
                f32& lrfNear = lClampedCamera.maProjectionScalars[7];
                lrfNear = (maTsmBBInfo[0].mfNearClip - lrfNear >= 0.0f) ? maTsmBBInfo[0].mfNearClip : lrfNear;
                lClampedCamera.UpdatePerspectiveProjectionMatrix();
            }
            {
                // far = min(clone far, maTsmBBInfo[2].mfFarClip) -- fsel @0x827DAC64
                f32& lrfFar = lClampedCamera.maProjectionScalars[8];
                lrfFar = (maTsmBBInfo[2].mfFarClip - lrfFar >= 0.0f) ? lrfFar : maTsmBBInfo[2].mfFarClip;
                lClampedCamera.UpdatePerspectiveProjectionMatrix();
            }

            CgsGeometric::Frustum lClampedFrustum;
            Vector4 laVerts[8];
            lClampedCamera.GetCgsFrustum(lClampedFrustum);
            lClampedFrustum.CalcVertices(laVerts);

            // max clip z of the 8 verts through cascade 0's VP (loop
            // @0x827DAC9C..0x827DACD8: vrlimi w<-1, row combine, vmaxfp)
            f32 lfMaxZ = 0.0f;                                  // vspltisw v12, 0
            for (int liVert = 0; liVert < 8; ++liVert)
            {
                Vector4 lvVert = laVerts[liVert];
                lvVert.w = 1.0f;                                 // vrlimi128 mask=1
                const f32 lfClipZ = MultRow(lvVert, lCascadeVP).z;
                lfMaxZ = (lfClipZ > lfMaxZ) ? lfClipZ : lfMaxZ;  // vmaxfp
            }

            // refit the shared far plane (fmadds + fadds @0x827DAD18)
            const f32 lfNear   = mfShadowMapNearPlane;
            const f32 lfNewFar = (mfShadowMapFarPlane - lfNear) * lfMaxZ
                               + mfDynamicFarClipOffset + lfNear;
            mfShadowMapFarPlane = lfNewFar;                      // stfs +0x14D4

            // vrefp + 2x NR reciprocal of the new span; the wAxis term rides
            // the pre-negated near (fneg @0x827DACF4)
            const f32 lfInvSpan  = 1.0f / (lfNewFar - lfNear);
            const f32 lfWzTerm   = lfInvSpan * (-lfNear);

            // per-cascade ortho z rewrite (loop @0x827DAD58..0x827DADA0:
            // vrlimi128 mask=2 lane-z inserts on zAxis/wAxis + VP rebuild)
            for (u32 luMap = 0; luMap < KU_NUM_SHADOW_MAPS; ++luMap)
            {
                CgsGraphics::Camera& lrCgsCamera = maCgsShadowMapCamera[luMap];
                lrCgsCamera.mProjection.zAxis.z = lfInvSpan;
                lrCgsCamera.mProjection.wAxis.z = lfWzTerm;
                lrCgsCamera.UpdateViewProjectionMatrix();
            }
        }

        // ---- 6. constants + debug render (@0x827DADA4..0x827DADD8) ----------
        SetConstants(lpRenderCamera);
        for (u32 luMap = 0; luMap < KU_NUM_SHADOW_MAPS; ++luMap)
        {
            if (maTsmBBInfo[luMap].mbDebugRender)                // lbz tsm+0x314
            {
                DebugRender(luMap);
            }
        }
    }

    // ========================================================================
    // BrnWorld::ShadowMap::SetConstants @0x827C16E0
    //
    // Stage the per-frame shadow shader constants from the (director) render
    // camera and the three cascade cameras:
    //
    //   1. mCachedOffsetWorld = renderCam.At * (mafShadowMapAtOffset[0] -
    //      mafShadowMapAtOffset[1])   (the vmulfp/vsubfp pair -> stvx +0x940)
    //   2. per map i: laMatrices[i] = (cascadeVP_i * gShadowPostProjectionMatrix)
    //                                 * gaShadowConstantMatrix[i]
    //      -- both full 4x4 row-by-row vmaddfp products; if the map is
    //      DISABLED (!mabMapEnabled[i], lbz +0x14CC+i) the product is replaced
    //      by gShadowDisabledConstantMatrix; then the w row gets
    //      gShadowConstantRowOffset added (vaddfp on the +0x30 row).
    //   3. SetShaderConstantArrayData(14, laMatrices)  (bl sub_827BAE60 -- the
    //      Matrix44* overload, CgsShaderConstants.h:526 assert)
    //   4. ObjectCSMSelect(0.0f)
    //   5. c15 = { maTsmBBInfo[0].mfFarClip, [1].mfFarClip, [2].mfFarClip,
    //              fadeScale } where fadeScale = |far2 - mfFadeStartDistance| >
    //              0.001 ? far2 / (far2 - mfFadeStartDistance) : 1.0
    //   6. c16 = { (far0 + far1) * 0.5, mfShadowFadeToValue,
    //              mfBiasFrustumLength / (mfShadowMapFarPlane -
    //              mfShadowMapNearPlane), (1/far2) * fadeScale }
    // ========================================================================
    void ShadowMap::SetConstants(const BrnDirector::Camera::Camera* lpCamera)
    {
        // ---- 1. the cached world offset (0x827C1700..0x827C1750) ------------
        const rw::math::vpu::Vector3& lrAt = lpCamera->mTransform.At();   // lvx r4+0x20
        const f32 lfOffsetDelta = mafShadowMapAtOffset[0] - mafShadowMapAtOffset[1];
        mCachedOffsetWorld.x = lrAt.x * lfOffsetDelta;
        mCachedOffsetWorld.y = lrAt.y * lfOffsetDelta;
        mCachedOffsetWorld.z = lrAt.z * lfOffsetDelta;
        mCachedOffsetWorld.w = lrAt.w * lfOffsetDelta;

        // ---- 2. the c14 matrix triple (loop @0x827C17F0..) ------------------
        Matrix44 laMatrices[KU_NUM_SHADOW_MAPS];
        for (u32 luMap = 0; luMap < KU_NUM_SHADOW_MAPS; ++luMap)
        {
            const Matrix44& lrCascadeVP = maCgsShadowMapCamera[luMap].mViewProjection;
            laMatrices[luMap] = MultMatrix(MultMatrix(lrCascadeVP, gShadowPostProjectionMatrix),
                                           gaShadowConstantMatrix[luMap]);
            if (!mabMapEnabled[luMap])                          // lbz +0x14CC+i
            {
                laMatrices[luMap] = gShadowDisabledConstantMatrix;
            }
            laMatrices[luMap].wAxis.x += gShadowConstantRowOffset.x;   // vaddfp on the w row
            laMatrices[luMap].wAxis.y += gShadowConstantRowOffset.y;
            laMatrices[luMap].wAxis.z += gShadowConstantRowOffset.z;
            laMatrices[luMap].wAxis.w += gShadowConstantRowOffset.w;
        }

        // ---- 3. upload (bl sub_827BAE60 == the Matrix44* array overload) ----
        CgsGraphics::mShaderConstantTable.SetShaderConstantArrayData(14u, laMatrices);

        // ---- 4. the CSM-select seed ----------------------------------------
        ObjectCSMSelect(0.0f);

        // ---- 5. c15 (0x827C1A00..) ------------------------------------------
        const f32 lfFar2 = maTsmBBInfo[2].mfFarClip;             // this+0x1004
        f32 lfFadeScale = 1.0f;
        if (std::fabs(lfFar2 - mfFadeStartDistance) > 0.001f)
        {
            lfFadeScale = lfFar2 / (lfFar2 - mfFadeStartDistance);
        }
        CgsGraphics::mShaderConstantTable.SetShaderConstantData(
            15u, Vector4{ maTsmBBInfo[0].mfFarClip, maTsmBBInfo[1].mfFarClip, lfFar2, lfFadeScale });

        // ---- 6. c16 ---------------------------------------------------------
        CgsGraphics::mShaderConstantTable.SetShaderConstantData(
            16u, Vector4{ (maTsmBBInfo[0].mfFarClip + maTsmBBInfo[1].mfFarClip) * 0.5f,
                          mfShadowFadeToValue,
                          mfBiasFrustumLength / (mfShadowMapFarPlane - mfShadowMapNearPlane),
                          (1.0f / lfFar2) * lfFadeScale });
    }

    // ========================================================================
    // BrnWorld::ShadowMap::ObjectCSMSelect @0x827C1630
    //
    // Select the near/far CSM pair for an object at lfDistance: the split is
    // the midpoint of maTsmBBInfo[1]'s near/far clip pair (fadds + fmuls 0.5);
    // sel = (lfDistance >= mid) ? 1 : 0 (the fcmpu/blt), and c17 =
    // { (f32)sel, (f32)(sel + 1), maTsmBBInfo[sel].mfFarClip, 0 } (the two
    // std/fcfid integer->float conversions + the 0x320-stride far-clip load).
    // ========================================================================
    void ShadowMap::ObjectCSMSelect(f32 lfDistance) const
    {
        const f32 lfMid = (maTsmBBInfo[1].mfFarClip + maTsmBBInfo[1].mfNearClip) * 0.5f;   // flt_82001DA0
        const u32 luSelect = (lfDistance >= lfMid) ? 1u : 0u;

        CgsGraphics::mShaderConstantTable.SetShaderConstantData(
            17u, Vector4{ static_cast<f32>(luSelect), static_cast<f32>(luSelect + 1u),
                          maTsmBBInfo[luSelect].mfFarClip, 0.0f });
    }

    // ========================================================================
    // Bounding-box shadow wave (2026-07-27b): ComputeBoundingBoxMatrix
    // @0x827D91B0 and ComputeOptimalViewVolume @0x827D8980, the two heavy VMX
    // passes the E_SHADOWMAP_TYPE_BOUNDINGBOX cascades run.
    //
    // Every local/parameter/constant name below is the DecFIGS DWARF ground
    // truth (references/DecFIGS/dwarfdump/GameSource/World/ShadowMap/
    // BrnShadowMap.cpp:1230-1540 and :1600-1790); the arithmetic is decoded
    // from the raw X360 instruction stream and was matched lane-for-lane
    // against a numeric emulation of it (session log shadowvmx_wave_log.md --
    // worst observed relative delta on the produced mBestFitMatrix ~3e-8, on
    // the candidate planes ~5e-8).
    // ========================================================================

    // ------------------------------------------------------------------------
    // BrnShadowMap.cpp:330 (DWARF). One candidate plane of the optimal shadow
    // view volume plus the score std::sort orders the candidate set by. 32
    // bytes (the X360 record stride, and the `srawi 5` element size every
    // std::_Sort/_Median/_Unguarded_partition instantiation divides by).
    // ------------------------------------------------------------------------
    struct CandidateViewVolumePlane
    {
        Vector3Plus mPlane;   // BrnShadowMap.cpp:339 -- {normal, D}, dot3(N,p) == D inside
        VecFloat    mScore;   // BrnShadowMap.cpp:340 -- sort key (broadcast lane)

        // BrnShadowMap.cpp:334. The X360 predicate (inlined into
        // _Unguarded_partition @0x827CEED8) is the ALL-LANES `vcmpgtfp.` of the
        // two score vectors -- CR6[0], i.e. every lane of the other score must
        // be greater. The scores are broadcast lanes, so this is the scalar
        // compare in vector clothing; spelled per-lane to keep the semantics.
        bool operator<(const CandidateViewVolumePlane& lrOther) const
        {
            return lrOther.mScore.x > mScore.x && lrOther.mScore.y > mScore.y
                && lrOther.mScore.z > mScore.z && lrOther.mScore.w > mScore.w;
        }
    };

    // BrnShadowMap.cpp:1187 (DWARF). The per-slot bounding-box debug blob
    // (unk_8300EA70, 768-byte stride) ComputeBoundingBoxMatrix fills and the
    // declaration-only DebugRender consumes.
    struct BoundingBoxDebugRenderInfo
    {
        Vector4               maFrustumPoints[8];    // +0x000 (:1189) world sub-frustum verts
        CgsGeometric::Frustum mCameraFrustum;        // +0x080 (:1190) memcpy of mSubFrustum
        Vector4               maBoxPointsWorld[4];   // +0x100 (:1191) corners/w -> light-to-world
        Vector4               maBoxPointsSquare[4];  // +0x140 (:1192) corners through the selected stage
        Vector4               mCentreLineStart;      // +0x180 (:1193)
        Vector4               mCentreLineEnd;        // +0x190 (:1194)
        Matrix44              mLightToWorld;         // +0x1A0 (:1195) Inverse(mWorldToLight)
        Vector4               mSideLineStart[2];     // +0x1E0 (:1196)
        Vector4               mSideLineEnd[2];       // +0x200 (:1197)
        Vector4               mBaseLineStart[2];     // +0x220 (:1198)
        Vector4               mBaseLineEnd[2];       // +0x240 (:1199)
        VecFloat              mZOffset;              // +0x260 (:1200)
        f32                   mfAspectRatio;         // +0x270 (:1201)
        u8                    maPad274[12];          // +0x274 pad to the 0x280 frustum
        CgsGeometric::Frustum mLightFrustum;         // +0x280 (:1202) memcpy of maFrustum[i]
    };
    static_assert(sizeof(BoundingBoxDebugRenderInfo) == 768,
                  "BoundingBoxDebugRenderInfo must be 768 bytes (X360 stride)");

    // BrnShadowMap.cpp:1203 (DWARF) -- unk_8300EA70.
    BoundingBoxDebugRenderInfo gBoundingBoxDebugRenderInfo[KU_NUM_SHADOW_MAPS];

    // BrnShadowMap.cpp:1219 (DWARF) -- unk_8300F630. DERIVED 0.25: the only use
    // is scaling the two four-vertex face sums into the near/far face centres
    // (the same quarter-average ComputeTSMMatrix spells with flt_82004EF4).
    // (The value is in fact unobservable -- it cancels in the Normalize that
    // immediately follows -- so 0.25 is the semantic, not a fitted, choice.)
    const VecFloat K_1_OVER_4 = { 0.25f, 0.25f, 0.25f, 0.25f };

    // BrnShadowMap.cpp:78/79 (DWARF) -- unk_8300F980 / unk_8300F5C0, the two
    // terms of the ideal-aspect-ratio area penalty. DERIVED from the names and
    // the clamp arithmetic the X360 emits, `t = clamp01((err - 1 - A) * B)`:
    // A == 0.25 and B == 1/0.75 make the penalty ramp exactly from t == 0 at an
    // aspect error of 1.25 to t == 1 at an aspect error of 2.0 -- i.e. B is
    // literally "one over (2.0 - 1.25)", which is what the DWARF name spells.
    const VecFloat K_VECFLOAT_ZEROPOINTTWOFIVE            = { 0.25f, 0.25f, 0.25f, 0.25f };
    const VecFloat K_VECFLOAT_ONE_OVER_ZEROPOINTSEVENFIVE = { 1.0f / 0.75f, 1.0f / 0.75f,
                                                              1.0f / 0.75f, 1.0f / 0.75f };

    // BrnShadowMap.cpp:1589-1592 (DWARF) -- unk_8300F990 / unk_8300E9A0 /
    // unk_8300E9D0 / unk_8300F460.
    //
    // FLAG (values not in this dossier -- IDA exports carry no data): K_VERY_SMALL
    //   must be STRICTLY POSITIVE. Both endpoints of every candidate edge plane
    //   evaluate to exactly zero distance in exact arithmetic, so with a zero
    //   tolerance float rounding puts them on both sides and the accept test
    //   rejects all twelve candidates (verified: 0/12 accepted at eps == 0 in the
    //   numeric emulation). Carried as a millimetre in world units.
    // FLAG (K_LOW_PRIORITY / K_HIGH_PRIORITY): values un-dumped. The X360 scores
    //   the first four (near-face) edge planes from one of them and the remaining
    //   eight from the other; which symbol binds to which branch is NOT pinned by
    //   the dossier. Both are carried at the same (zero) value, which makes the
    //   binding inert and keeps every edge plane ahead of the opposing frustum
    //   planes (whose score is 2 - NdotL, i.e. > 2) in the ascending sort -- the
    //   ordering the algorithm plainly intends.
    const VecFloat K_VERY_SMALL       = {  0.001f,  0.001f,  0.001f,  0.001f }; // FLAG: value not recovered
    const VecFloat K_MINUS_VERY_SMALL = { -0.001f, -0.001f, -0.001f, -0.001f }; // == -K_VERY_SMALL
    const VecFloat K_LOW_PRIORITY     = { 0.0f, 0.0f, 0.0f, 0.0f };             // FLAG: value not recovered
    const VecFloat K_HIGH_PRIORITY    = { 0.0f, 0.0f, 0.0f, 0.0f };             // FLAG: value not recovered

    // BrnShadowMap.cpp:256 (DWARF `const CgsGeometric::Frustum::Vertices[24]`).
    // The twelve (start, end) vertex-index pairs of the sub-frustum's edge list.
    // RECOVERED from the fully unrolled gather the X360 emits at
    // 0x827D89E8..0x827D8B78 (each pair is a pinned lvx offset into the two
    // point arrays), NOT guessed: the near face walks 0-1-3-2, the far face
    // 4-5-7-6, then the four connecting edges. NOTE the last pair is {2, 7} and
    // not the {2, 6} a clean box would use -- that is what the shipping table
    // holds (the unrolled load re-uses the vertex-7 register for both edge 10
    // and edge 11), reproduced verbatim.
    // (Typed u32 here: the DWARF element type is a CgsGeometric::Frustum::Vertices
    // enum whose home header is outside this TU's ownership.)
    const u32 KA_FRUSTUM_VERT_LINE_INDICES[24] =
    {
        0, 1,   1, 3,   3, 2,   2, 0,      // near face loop
        4, 5,   5, 7,   7, 6,   6, 4,      // far face loop
        0, 4,   1, 5,   3, 7,   2, 7,      // the connecting edges (last pair per the shipping table)
    };

    namespace
    {
        // --------------------------------------------------------------------
        // The rw::math::vpu shapes these two bodies are built from. Same
        // convention as the MultRow/MultMatrix pair above: row-vector maths, the
        // vrefp/vrsqrtefp + 2x Newton-Raphson chains de-optimised to exact.
        // --------------------------------------------------------------------

        // rw::math::vpu::TransformPoint(Vector3, Matrix44) -- affine seed from row 3.
        Vector4 TransformPoint(const Vector4& lrPoint, const Matrix44& lrMatrix)
        {
            const f32* lapRows[4] = { &lrMatrix.xAxis.x, &lrMatrix.yAxis.x,
                                      &lrMatrix.zAxis.x, &lrMatrix.wAxis.x };
            const f32 lafLanes[3] = { lrPoint.x, lrPoint.y, lrPoint.z };
            Vector4 lvOut;
            f32* lpfOut = &lvOut.x;
            for (int liLane = 0; liLane < 4; ++liLane)
            {
                lpfOut[liLane] = lapRows[3][liLane];
            }
            for (int liRow = 0; liRow < 3; ++liRow)
            {
                for (int liLane = 0; liLane < 4; ++liLane)
                {
                    lpfOut[liLane] += lafLanes[liRow] * lapRows[liRow][liLane];
                }
            }
            return lvOut;
        }

        // rw::math::vpu::TransformVector(Vector3, Matrix44Affine) -- no translation.
        Vector4 TransformVector(const Vector4& lrVector, const Matrix44Affine& lrMatrix)
        {
            const f32* lapRows[3] = { &lrMatrix.xAxis.x, &lrMatrix.yAxis.x, &lrMatrix.zAxis.x };
            const f32 lafLanes[3] = { lrVector.x, lrVector.y, lrVector.z };
            Vector4 lvOut; lvOut.SetZero();
            f32* lpfOut = &lvOut.x;
            for (int liRow = 0; liRow < 3; ++liRow)
            {
                for (int liLane = 0; liLane < 4; ++liLane)
                {
                    lpfOut[liLane] += lafLanes[liRow] * lapRows[liRow][liLane];
                }
            }
            return lvOut;
        }

        // rw::math::vpu::Normalize(Vector3) -- vmsum3fp + vrsqrtefp + 2x NR.
        Vector4 Normalize3(const Vector4& lrVector)
        {
            const f32 lfLenSq = lrVector.x * lrVector.x + lrVector.y * lrVector.y
                              + lrVector.z * lrVector.z;
            const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);
            return Vector4{ lrVector.x * lfInvLen, lrVector.y * lfInvLen,
                            lrVector.z * lfInvLen, lrVector.w * lfInvLen };
        }

        // rw::math::vpu::Magnitude(Vector4) -- vmsum4fp + rsqrt NR, with the
        // vcmpeqfp/vsel guard that returns exactly 0 for a zero-length input.
        f32 Magnitude4(const Vector4& lrVector)
        {
            const f32 lfLenSq = lrVector.x * lrVector.x + lrVector.y * lrVector.y
                              + lrVector.z * lrVector.z + lrVector.w * lrVector.w;
            return (lfLenSq == 0.0f) ? 0.0f : lfLenSq * (1.0f / std::sqrt(lfLenSq));
        }

        f32 Dot3(const Vector4& lrA, const Vector4& lrB)
        {
            return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;
        }

        // rw::math::vpu::Cross(Vector3, Vector3) -- the vpermwi128-0x63 (y,z,x)
        // rotate pair the X360 emits.
        Vector4 Cross3(const Vector4& lrA, const Vector4& lrB)
        {
            return Vector4{ lrA.y * lrB.z - lrA.z * lrB.y,
                            lrA.z * lrB.x - lrA.x * lrB.z,
                            lrA.x * lrB.y - lrA.y * lrB.x,
                            0.0f };
        }

        // BrnShadowMap.cpp:356 (DWARF `BrnWorld::_LineIntersection2d`). The XY
        // intersection of the line through lrLine0Start with direction lrLine0Vec
        // and the line through lrLine1Start with direction lrLine1Vec, returned as
        // the point on line 0 (z kept from line 0's start, w forced to 1 by the
        // vrlimi128 mask=1 insert). The reciprocal is the vrefp + 2x NR chain,
        // de-optimised exact.
        Vector4 _LineIntersection2d(const Vector4& lrLine0Start, const Vector4& lrLine0Vec,
                                    const Vector4& lrLine1Start, const Vector4& lrLine1Vec)
        {
            const f32 lfDenom = lrLine1Vec.y * lrLine0Vec.x - lrLine1Vec.x * lrLine0Vec.y;
            const f32 lfNumer = lrLine1Vec.x * (lrLine0Start.y - lrLine1Start.y)
                              - lrLine1Vec.y * (lrLine0Start.x - lrLine1Start.x);
            const f32 lfS = lfNumer / lfDenom;

            Vector4 lvOut;
            lvOut.x = lrLine0Start.x + lrLine0Vec.x * lfS;
            lvOut.y = lrLine0Start.y + lrLine0Vec.y * lfS;
            lvOut.z = lrLine0Start.z + lrLine0Vec.z * lfS;
            lvOut.w = 1.0f;
            return lvOut;
        }
    }

    // ========================================================================
    // BrnWorld::ShadowMap::ComputeBoundingBoxMatrix @0x827D91B0
    //
    // Fit the tightest oriented 2D box (in the cascade's post-projective light
    // plane) around the map's clamped sub-frustum, and turn that box into the
    // mBestFitMatrix post-projection CalculateShadowMapCameras multiplies the
    // cascade projection by. Pipeline:
    //
    //   1. gated on mbUpdateTsmCamera: snapshot the cascade camera's
    //      mViewProjection -> mWorldToLight and its mView -> mWorldToLightView,
    //      clone lCamera into the slot's TSM camera and clamp its near/far into
    //      [mfNearClip, mfFarClip] (the two fsel max/min + projection rebuilds).
    //   2. GetFrustumPerspective -> mSubFrustum.SetFromRwFrustum -> CalcVertices
    //      -> the 8 world-space sub-frustum corners.
    //   3. Each corner through mWorldToLight; keep the full projective point in
    //      lFrustumPointsLightSpace3d and the flattened (x, y, 0) point in
    //      lFrustumPointsLightSpace -- the whole solve runs in that 2D plane.
    //   4. Near/far face centres (quarter sums), their normalised difference,
    //      and its 2D perpendicular = the seed "base line" direction.
    //   5. KU_NUM_BOUNDING_BOX_ITERATIONS rotations of that direction about Z:
    //      per rotation take the min/max support points along the side vector
    //      (they carry the base and top lines) and along the negated base vector
    //      (they carry the two side lines), and keep the rotation whose
    //      width x height area is smallest (optionally weighted towards
    //      mfIdealAspectRatio).
    //   6. Intersect the four lines -> laBestBoxPoints (top-left, top-right,
    //      base-right, base-left); optionally cycle them by one so the long edge
    //      is the base (mbPreferShortAndFatProjection).
    //   7. Compose lTransformT1 (centre -> origin) * lTransformR (base edge ->
    //      +X) * lTransformS2 (corner 0 -> (1,1)) into mBestFitMatrix, then
    //      mirror X when mbInvertCullMode.
    //   8. Invert mWorldToLightView into mLightToWorldView and hand the whole
    //      thing to ComputeOptimalViewVolume, which writes the cascade's culling
    //      frustum.
    //   9. gated on mbUpdateDebugRender: fill gBoundingBoxDebugRenderInfo.
    // ========================================================================
    void ShadowMap::ComputeBoundingBoxMatrix(u32 luMapIndex, CgsGraphics::Camera& lCamera)
    {
        TsmBBInfo& lTsmBBInfo = maTsmBBInfo[luMapIndex];              // BrnShadowMap.cpp:1230

        // ---- 1. TSM camera refresh (0x827D91F4..0x827D9290, gated) ----------
        if (mbUpdateTsmCamera)                                       // lbz 0x1502(this)
        {
            lTsmBBInfo.mWorldToLight = maCgsShadowMapCamera[luMapIndex].mViewProjection;
            {   // the four lvx/stvx rows of the cascade view matrix
                const Matrix44& lrView = maCgsShadowMapCamera[luMapIndex].mView;
                lTsmBBInfo.mWorldToLightView.xAxis =
                    Vector3{ lrView.xAxis.x, lrView.xAxis.y, lrView.xAxis.z, lrView.xAxis.w };
                lTsmBBInfo.mWorldToLightView.yAxis =
                    Vector3{ lrView.yAxis.x, lrView.yAxis.y, lrView.yAxis.z, lrView.yAxis.w };
                lTsmBBInfo.mWorldToLightView.zAxis =
                    Vector3{ lrView.zAxis.x, lrView.zAxis.y, lrView.zAxis.z, lrView.zAxis.w };
                lTsmBBInfo.mWorldToLightView.wAxis =
                    Vector3{ lrView.wAxis.x, lrView.wAxis.y, lrView.wAxis.z, lrView.wAxis.w };
            }

            lCamera.Clone(&lTsmBBInfo.mCamera);

            {   // near = max(mfNearClip, cloned near) -- fsel @0x827D926C
                f32& lrfCamNear = lTsmBBInfo.mCamera.maProjectionScalars[7];
                lrfCamNear = (lTsmBBInfo.mfNearClip - lrfCamNear >= 0.0f) ? lTsmBBInfo.mfNearClip
                                                                          : lrfCamNear;
                lTsmBBInfo.mCamera.UpdatePerspectiveProjectionMatrix();
            }
            {   // far = min(cloned far, mfFarClip) -- fsel @0x827D9288
                f32& lrfCamFar = lTsmBBInfo.mCamera.maProjectionScalars[8];
                lrfCamFar = (lTsmBBInfo.mfFarClip - lrfCamFar >= 0.0f) ? lrfCamFar
                                                                       : lTsmBBInfo.mfFarClip;
                lTsmBBInfo.mCamera.UpdatePerspectiveProjectionMatrix();
            }
        }

        // ---- 2. sub-frustum -> 8 world corners (0x827D9294..0x827D92BC) -----
        const Matrix44& lWorldToLight = lTsmBBInfo.mWorldToLight;    // BrnShadowMap.cpp:1246
        CgsGraphics::CameraRwFrustum lRwFrustum;                     // BrnShadowMap.cpp:1248
        Vector4 lFrustumPoints[8];                                   // BrnShadowMap.cpp:1259
        lTsmBBInfo.mCamera.GetFrustumPerspective(lRwFrustum, false);
        lTsmBBInfo.mSubFrustum.SetFromRwFrustum(lRwFrustum);
        lTsmBBInfo.mSubFrustum.CalcVertices(lFrustumPoints);

        // ---- 3. into the 2D light plane (0x827D92C0..0x827D940C) ------------
        // The flattened point keeps only (x, y): the perm+vrlimi pair the X360
        // emits writes lane 2 as zero and leaves lane 3 unread by every consumer.
        Vector4 lFrustumPointsLightSpace3d[8];                       // BrnShadowMap.cpp:1263
        Vector4 lFrustumPointsLightSpace[8];                         // BrnShadowMap.cpp:1264
        for (u32 luI = 0; luI < 8; ++luI)                            // BrnShadowMap.cpp:1265
        {
            lFrustumPointsLightSpace3d[luI] = TransformPoint(lFrustumPoints[luI], lWorldToLight);
            lFrustumPointsLightSpace[luI]   = Vector4{ lFrustumPointsLightSpace3d[luI].x,
                                                       lFrustumPointsLightSpace3d[luI].y,
                                                       0.0f, 0.0f };
        }

        // ---- 4. centre line + the seed base direction (0x827D9410..0x827D9548)
        Vector4 lCentreLineStart;                                    // BrnShadowMap.cpp:1278
        Vector4 lCentreLineEnd;                                      // BrnShadowMap.cpp:1284
        {
            Vector4 lvNear = lFrustumPointsLightSpace[0];
            Vector4 lvFar  = lFrustumPointsLightSpace[4];
            for (u32 luI = 1; luI < 4; ++luI)
            {
                lvNear.x += lFrustumPointsLightSpace[luI].x;
                lvNear.y += lFrustumPointsLightSpace[luI].y;
                lvNear.z += lFrustumPointsLightSpace[luI].z;
                lvNear.w += lFrustumPointsLightSpace[luI].w;
                lvFar.x  += lFrustumPointsLightSpace[luI + 4].x;
                lvFar.y  += lFrustumPointsLightSpace[luI + 4].y;
                lvFar.z  += lFrustumPointsLightSpace[luI + 4].z;
                lvFar.w  += lFrustumPointsLightSpace[luI + 4].w;
            }
            lCentreLineStart = Vector4{ lvNear.x * K_1_OVER_4.x, lvNear.y * K_1_OVER_4.y,
                                        lvNear.z * K_1_OVER_4.z, lvNear.w * K_1_OVER_4.w };
            lCentreLineEnd   = Vector4{ lvFar.x * K_1_OVER_4.x, lvFar.y * K_1_OVER_4.y,
                                        lvFar.z * K_1_OVER_4.z, lvFar.w * K_1_OVER_4.w };
        }
        const Vector4 lCentreLineVec = Normalize3(Vector4{                   // :1291
            lCentreLineEnd.x - lCentreLineStart.x, lCentreLineEnd.y - lCentreLineStart.y,
            lCentreLineEnd.z - lCentreLineStart.z, lCentreLineEnd.w - lCentreLineStart.w });
        const Vector4 lOriginalBaseLineVec{ lCentreLineVec.y, -lCentreLineVec.x, 0.0f, 0.0f }; // :1305

        // ---- 5. the rotation search (0x827D95C4..0x827DA004) ----------------
        // FLAG (dword_82F30E64 / flt_82004F64 -- values not in this dossier):
        //   KU_NUM_BOUNDING_BOX_ITERATIONS must be >= 1, otherwise the corner
        //   solve below runs on the zero-initialised support points and divides
        //   by zero. Carried at 1 (evaluate the un-rotated fit only), the
        //   minimal value that reproduces the algorithm; the sweep in degrees is
        //   then unobservable (only rotation 0 is visited) and is carried as the
        //   quarter turn that covers every distinct box orientation.
        static u32 KU_NUM_BOUNDING_BOX_ITERATIONS = 1;                        // :1307 FLAG
        static const f32 KF_BOUNDING_BOX_SWEEP_DEGREES = 90.0f;               // FLAG

        Vector4 laBestBoxPoints[4];                                            // :1296
        Vector4 lBestBoxBaseLineStart;      lBestBoxBaseLineStart.SetZero();   // :1297
        Vector4 lBestBoxTopLineStart;       lBestBoxTopLineStart.SetZero();    // :1298
        Vector4 lBestBoxSideLineRightVec;   lBestBoxSideLineRightVec.SetZero();// :1299
        Vector4 lBestBoxSideLineRightStart; lBestBoxSideLineRightStart.SetZero();//:1300
        Vector4 lBestBoxSideLineLeftVec;    lBestBoxSideLineLeftVec.SetZero(); // :1301
        Vector4 lBestBoxSideLineLeftStart;  lBestBoxSideLineLeftStart.SetZero();//:1302
        Vector4 lBaseLineVec;               lBaseLineVec.SetZero();            // :1306
        f32 lSmallestArea = 0.0f;                                              // :1304

        const f32 lRotateStep = (KF_BOUNDING_BOX_SWEEP_DEGREES * 0.017453292f)  // flt_820CA158
                              / static_cast<f32>(KU_NUM_BOUNDING_BOX_ITERATIONS); // :1308
        f32 lRotation = 0.0f;                                                  // :1309
        const f32 lIdealAspectRatio = mfIdealAspectRatio;                      // :1311

        for (u32 luIteration = 0; luIteration < KU_NUM_BOUNDING_BOX_ITERATIONS; ++luIteration) // :1314
        {
            // FLAG (PC-platform leaf: the X360 inlines the XNAMath XMVectorSinCos
            // minimax polynomial pair; its coefficient tables unk_82000BD0..
            // unk_82000C60 are un-dumped rodata, so the pair is lowered to libm --
            // same treatment as the ICEMath.cpp tan/atan precedent). The 2x2 block
            // is the standard rotation about Z.
            const f32 lfCos = std::cos(lRotation);
            const f32 lfSin = std::sin(lRotation);
            Matrix44Affine lRotationAroundZMatrix;                             // :1310
            lRotationAroundZMatrix.xAxis = Vector3{  lfCos, lfSin, 0.0f, 0.0f };
            lRotationAroundZMatrix.yAxis = Vector3{ -lfSin, lfCos, 0.0f, 0.0f };
            lRotationAroundZMatrix.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
            lRotationAroundZMatrix.wAxis = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

            const Vector4 lvBase = TransformVector(lOriginalBaseLineVec, lRotationAroundZMatrix);
            const Vector4 lvSide{ -lvBase.y, lvBase.x, 0.0f, 0.0f };

            // support points along the side vector -> the base / top lines
            f32 lfSideMin = Dot3(lvSide, lFrustumPointsLightSpace[0]);
            f32 lfSideMax = lfSideMin;
            Vector4 lvBaseStart = lFrustumPointsLightSpace[0];
            Vector4 lvTopStart  = lFrustumPointsLightSpace[0];
            for (u32 luI = 1; luI < 8; ++luI)
            {
                const f32 lfDot = Dot3(lvSide, lFrustumPointsLightSpace[luI]);
                if (lfSideMin > lfDot)                          // vcmpgtfp.
                {
                    lfSideMin = lfDot; lvBaseStart = lFrustumPointsLightSpace[luI];
                }
                if (lfDot > lfSideMax)
                {
                    lfSideMax = lfDot; lvTopStart = lFrustumPointsLightSpace[luI];
                }
            }
            const f32 lHeight = lfSideMax - lfSideMin;

            // support points along the negated base vector -> the two side lines.
            // (Both side lines take the SAME direction vector -- the X360 keeps
            // two copies of it, matching the two named locals.)
            const Vector4 lvSideLineVec{ -lvSide.x, -lvSide.y, -lvSide.z, -lvSide.w };
            const Vector4 lvNegBase{ -lvBase.x, -lvBase.y, -lvBase.z, 0.0f };
            f32 lfBaseMax = Dot3(lvNegBase, lFrustumPointsLightSpace[0]);
            f32 lfBaseMin = lfBaseMax;
            u32 luMaxIndex = 0;
            u32 luMinIndex = 0;
            for (u32 luI = 1; luI < 8; ++luI)                   // the if/else-if chain the asm emits
            {
                const f32 lfDot = Dot3(lvNegBase, lFrustumPointsLightSpace[luI]);
                if (lfDot > lfBaseMax)
                {
                    lfBaseMax = lfDot; luMaxIndex = luI;
                }
                else if (lfBaseMin > lfDot)
                {
                    lfBaseMin = lfDot; luMinIndex = luI;
                }
            }
            const f32 lWidth = lfBaseMax - lfBaseMin;

            f32 lfArea = lHeight * lWidth;
            if (mbOptimiseForIdealAspectRatio)                  // lbz 0x1505(this)
            {
                const f32 lfRatio = ((lWidth > lHeight) ? lWidth : lHeight)
                                  / ((lWidth > lHeight) ? lHeight : lWidth);
                const f32 lfHi = (lfRatio > lIdealAspectRatio) ? lfRatio : lIdealAspectRatio;
                const f32 lfLo = (lfRatio > lIdealAspectRatio) ? lIdealAspectRatio : lfRatio;
                f32 lfWeight = std::fabs(lfHi / lfLo) - 1.0f
                             - K_VECFLOAT_ZEROPOINTTWOFIVE.x;
                lfWeight *= K_VECFLOAT_ONE_OVER_ZEROPOINTSEVENFIVE.x;
                lfWeight = (lfWeight > 0.0f) ? lfWeight : 0.0f;  // vmaxfp 0
                lfWeight = (lfWeight > 1.0f) ? 1.0f : lfWeight;  // vminfp 1
                lfArea *= 1.0f + lfWeight;
            }

            if (luIteration == 0 || lSmallestArea > lfArea)
            {
                lSmallestArea             = lfArea;
                lBaseLineVec              = lvBase;
                lBestBoxTopLineStart      = lvTopStart;
                lBestBoxBaseLineStart     = lvBaseStart;
                lBestBoxSideLineRightStart = lFrustumPointsLightSpace[luMaxIndex];
                lBestBoxSideLineLeftStart  = lFrustumPointsLightSpace[luMinIndex];
                lBestBoxSideLineRightVec   = lvSideLineVec;
                lBestBoxSideLineLeftVec    = lvSideLineVec;
            }
            lRotation += lRotateStep;
        }

        // ---- 6. the four box corners (0x827DA008..0x827D9CB0) ---------------
        laBestBoxPoints[0] = _LineIntersection2d(lBestBoxTopLineStart,  lBaseLineVec,
                                                 lBestBoxSideLineLeftStart,  lBestBoxSideLineLeftVec);
        laBestBoxPoints[1] = _LineIntersection2d(lBestBoxTopLineStart,  lBaseLineVec,
                                                 lBestBoxSideLineRightStart, lBestBoxSideLineRightVec);
        laBestBoxPoints[2] = _LineIntersection2d(lBestBoxBaseLineStart, lBaseLineVec,
                                                 lBestBoxSideLineRightStart, lBestBoxSideLineRightVec);
        laBestBoxPoints[3] = _LineIntersection2d(lBestBoxBaseLineStart, lBaseLineVec,
                                                 lBestBoxSideLineLeftStart,  lBestBoxSideLineLeftVec);

        const f32 lHeight = Magnitude4(Vector4{                                // :1436
            laBestBoxPoints[0].x - laBestBoxPoints[1].x, laBestBoxPoints[0].y - laBestBoxPoints[1].y,
            laBestBoxPoints[0].z - laBestBoxPoints[1].z, laBestBoxPoints[0].w - laBestBoxPoints[1].w });
        const f32 lWidth = Magnitude4(Vector4{                                 // :1437
            laBestBoxPoints[0].x - laBestBoxPoints[3].x, laBestBoxPoints[0].y - laBestBoxPoints[3].y,
            laBestBoxPoints[0].z - laBestBoxPoints[3].z, laBestBoxPoints[0].w - laBestBoxPoints[3].w });

        if (mbPreferShortAndFatProjection && lWidth > lHeight)                 // lbz 0x1503(this)
        {
            const Vector4 lvFirst = laBestBoxPoints[0];        // cycle the quad by one
            laBestBoxPoints[0] = laBestBoxPoints[1];
            laBestBoxPoints[1] = laBestBoxPoints[2];
            laBestBoxPoints[2] = laBestBoxPoints[3];
            laBestBoxPoints[3] = lvFirst;
        }

        // ---- 7. the best-fit transform (0x827D9CB4..0x827DA10C) -------------
        // FLAG (byte_82F30E60 -- value not in this dossier): with sbCentreBox the
        //   box is centred on its own centroid (the quad maps onto [-1,1]^2);
        //   without it the base edge's midpoint becomes the origin (the quad maps
        //   onto x in [-1,1], y in [0,1]). Carried false -- the base-edge origin
        //   is the trapezoidal-shadow convention this projection feeds.
        static const bool sbCentreBox = false;                                  // :1463 FLAG
        Vector4 lvCentre;
        if (sbCentreBox)
        {
            const f32 lfQuarter = 0.25f;                       // flt_82003F40 (centroid of 4)
            lvCentre.x = (laBestBoxPoints[0].x + laBestBoxPoints[1].x
                        + laBestBoxPoints[2].x + laBestBoxPoints[3].x) * lfQuarter;
            lvCentre.y = (laBestBoxPoints[0].y + laBestBoxPoints[1].y
                        + laBestBoxPoints[2].y + laBestBoxPoints[3].y) * lfQuarter;
        }
        else
        {
            const f32 lfHalf = 0.5f;                           // flt_82001DA0
            lvCentre.x = (laBestBoxPoints[2].x + laBestBoxPoints[3].x) * lfHalf;
            lvCentre.y = (laBestBoxPoints[2].y + laBestBoxPoints[3].y) * lfHalf;
        }
        lvCentre.z = 0.0f; lvCentre.w = 0.0f;

        const Vector4 lVecU = Normalize3(Vector4{                              // :1460
            laBestBoxPoints[2].x - laBestBoxPoints[3].x, laBestBoxPoints[2].y - laBestBoxPoints[3].y,
            laBestBoxPoints[2].z - laBestBoxPoints[3].z, laBestBoxPoints[2].w - laBestBoxPoints[3].w });

        Matrix44 lTransformT1;                                                 // :1472
        lTransformT1.xAxis = Vector4{ 1.0f, 0.0f, 0.0f, 0.0f };
        lTransformT1.yAxis = Vector4{ 0.0f, 1.0f, 0.0f, 0.0f };
        lTransformT1.zAxis = Vector4{ 0.0f, 0.0f, 1.0f, 0.0f };
        lTransformT1.wAxis = Vector4{ -lvCentre.x, -lvCentre.y, 0.0f, 1.0f };

        // FLAG (flt_82F30E50 / flt_82F30E40 -- the two static coefficient rows
        //   are un-dumped .data). DERIVED, not guessed: the composed transform
        //   must map the fitted quad onto the axis-aligned unit box, and only
        //   XMult == {1,0,0,1} / YMult == {0,-1,1,0} does so -- i.e. lTransformR
        //   is the rotation that carries lVecU onto +X. Checked against the
        //   numeric emulation of the X360 stream over randomised quads: the two
        //   alternative sign/transpose fillings put the mapped corners at
        //   arbitrary positions (up to 65 units off the unit box) while this one
        //   lands them exactly on (+-1, 1) / (+-1, 0) -- and on (+-1, +-1) with
        //   sbCentreBox.
        static const f32 XMult[4] = { 1.0f, 0.0f, 0.0f, 1.0f };                // :1482 FLAG
        static const f32 YMult[4] = { 0.0f, -1.0f, 1.0f, 0.0f };               // :1483 FLAG

        Matrix44 lTransformR;                                                  // :1485
        lTransformR.xAxis = Vector4{ lVecU.x * XMult[0] + lVecU.y * YMult[0],
                                     lVecU.x * XMult[1] + lVecU.y * YMult[1], 0.0f, 0.0f };
        lTransformR.yAxis = Vector4{ lVecU.x * XMult[2] + lVecU.y * YMult[2],
                                     lVecU.x * XMult[3] + lVecU.y * YMult[3], 0.0f, 0.0f };
        lTransformR.zAxis = Vector4{ 0.0f, 0.0f, 1.0f, 0.0f };
        lTransformR.wAxis = Vector4{ 0.0f, 0.0f, 0.0f, 1.0f };

        const Matrix44 lTransformT1xR = MultMatrix(lTransformT1, lTransformR); // :1492
        const Vector4  lvCorner       = MultRow(laBestBoxPoints[0], lTransformT1xR);

        Matrix44 lTransformS2;                                                 // :1496
        lTransformS2.xAxis = Vector4{ 1.0f / lvCorner.x, 0.0f, 0.0f, 0.0f };
        lTransformS2.yAxis = Vector4{ 0.0f, 1.0f / lvCorner.y, 0.0f, 0.0f };
        lTransformS2.zAxis = Vector4{ 0.0f, 0.0f, 1.0f, 0.0f };
        lTransformS2.wAxis = Vector4{ 0.0f, 0.0f, 0.0f, 1.0f };

        lTsmBBInfo.mBestFitMatrix = MultMatrix(lTransformT1xR, lTransformS2);

        if (lTsmBBInfo.mbInvertCullMode)                                       // lbz 0x240(tsm)
        {
            Matrix44 lMirrorX;
            lMirrorX.xAxis = Vector4{ -1.0f, 0.0f, 0.0f, 0.0f };
            lMirrorX.yAxis = Vector4{ 0.0f, 1.0f, 0.0f, 0.0f };
            lMirrorX.zAxis = Vector4{ 0.0f, 0.0f, 1.0f, 0.0f };
            lMirrorX.wAxis = Vector4{ 0.0f, 0.0f, 0.0f, 1.0f };
            lTsmBBInfo.mBestFitMatrix = MultMatrix(lTsmBBInfo.mBestFitMatrix, lMirrorX);
        }

        // ---- 8. light-view inverse + the view-volume solve (0x827DA1F4..) ---
        // The inlined affine inverse: the three adjugate crosses (vpermwi128
        // 0x63 rotates) transposed into rows and scaled by 1/det (vrefp + 2x NR),
        // then the translation row = -wAxis through that basis.
        {
            const Matrix44Affine& lrView = lTsmBBInfo.mWorldToLightView;
            const Vector4 lvX{ lrView.xAxis.x, lrView.xAxis.y, lrView.xAxis.z, lrView.xAxis.w };
            const Vector4 lvY{ lrView.yAxis.x, lrView.yAxis.y, lrView.yAxis.z, lrView.yAxis.w };
            const Vector4 lvZ{ lrView.zAxis.x, lrView.zAxis.y, lrView.zAxis.z, lrView.zAxis.w };
            const Vector4 lvCrossYZ = Cross3(lvY, lvZ);
            const Vector4 lvCrossXY = Cross3(lvX, lvY);
            const Vector4 lvCrossZX = Cross3(lvZ, lvX);
            const f32     lfInvDet  = 1.0f / Dot3(lvX, lvCrossYZ);

            Matrix44Affine& lrInv = lTsmBBInfo.mLightToWorldView;
            lrInv.xAxis = Vector3{ lvCrossYZ.x * lfInvDet, lvCrossZX.x * lfInvDet,
                                   lvCrossXY.x * lfInvDet, lvCrossZX.x * lfInvDet };
            lrInv.yAxis = Vector3{ lvCrossYZ.y * lfInvDet, lvCrossZX.y * lfInvDet,
                                   lvCrossXY.y * lfInvDet, lvCrossZX.y * lfInvDet };
            lrInv.zAxis = Vector3{ lvCrossYZ.z * lfInvDet, lvCrossZX.z * lfInvDet,
                                   lvCrossXY.z * lfInvDet, lvCrossZX.z * lfInvDet };
            // translation = -mWorldToLightView.wAxis through the inverse basis
            const f32* lpfNegPos = &lrView.wAxis.x;
            const f32* lapInvRows[3] = { &lrInv.xAxis.x, &lrInv.yAxis.x, &lrInv.zAxis.x };
            f32* lpfOut = &lrInv.wAxis.x;
            for (int liLane = 0; liLane < 4; ++liLane)
            {
                lpfOut[liLane] = -lpfNegPos[0] * lapInvRows[0][liLane]
                               - lpfNegPos[1] * lapInvRows[1][liLane]
                               - lpfNegPos[2] * lapInvRows[2][liLane];
            }
        }

        ComputeOptimalViewVolume(lTsmBBInfo.mSubFrustum, lTsmBBInfo.mLightToWorldView,
                                 maFrustum[luMapIndex], lFrustumPoints, lFrustumPointsLightSpace);

        // ---- 9. debug stage-matrix table + blob fill (0x827DA2F8..0x827DA80C)
        Matrix44 lIdentity; lIdentity.SetIdentity();                           // :1523
        const Matrix44 lTestMatrixRxT1 = MultMatrix(lTransformR, lTransformT1); // :1524
        const Matrix44* lpMatrices[6] =                                        // :1525
        {
            &lIdentity, &lTransformT1, &lTransformT1xR,
            &lTsmBBInfo.mBestFitMatrix, &lTransformR, &lTestMatrixRxT1,
        };

        if (mbUpdateDebugRender)                                               // lbz 0x1501(this)
        {
            // FLAG (dword_82F30E3C -- value not in this dossier): selects which
            //   composition stage the debug quad is mapped through. 0 (identity)
            //   is the inert default, same treatment as ComputeTSMMatrix's
            //   siTsmDebugMatrixStage.
            static s32 siBoxMatrixIndex = 0;                                   // :1534 FLAG

            Vector4 lvDeterminant;
            const Matrix44 lLightToWorld =                                     // :1538
                rw::math::vpu::Inverse(lWorldToLight, lvDeterminant);
            const Vector4 lZOffset{ 0.0f, 0.0f, lFrustumPointsLightSpace3d[0].z, 0.0f }; // :1539
            BoundingBoxDebugRenderInfo& lDebugInfo = gBoundingBoxDebugRenderInfo[luMapIndex]; // :1540

            std::memcpy(&lDebugInfo.mLightFrustum, &maFrustum[luMapIndex],
                        sizeof(CgsGeometric::Frustum));
            std::memcpy(&lDebugInfo.mCameraFrustum, &lTsmBBInfo.mSubFrustum,
                        sizeof(CgsGeometric::Frustum));

            for (u32 luI = 0; luI < 4; ++luI)
            {
                const Vector4& lrCorner = laBestBoxPoints[luI];
                const f32 lfInvW = 1.0f / lrCorner.w;
                lDebugInfo.maBoxPointsWorld[luI] = TransformPoint(
                    Vector4{ lrCorner.x * lfInvW, lrCorner.y * lfInvW,
                             lrCorner.z * lfInvW, lrCorner.w * lfInvW }, lLightToWorld);

                const Vector4 lvStage = MultRow(lrCorner, *lpMatrices[siBoxMatrixIndex]);
                const f32 lfInvStageW = 1.0f / lvStage.w;
                lDebugInfo.maBoxPointsSquare[luI] = TransformPoint(
                    Vector4{ lvStage.x * lfInvStageW, lvStage.y * lfInvStageW,
                             lvStage.z * lfInvStageW, lvStage.w * lfInvStageW }, lLightToWorld);
            }

            // The side / base line pairs, each a light-space point offset by the
            // stored line vector, mapped back to world.
            const Vector4 laLinePoints[8] =
            {
                Vector4{ lBestBoxSideLineLeftStart.x + lBestBoxSideLineLeftVec.x,
                         lBestBoxSideLineLeftStart.y + lBestBoxSideLineLeftVec.y,
                         lZOffset.z, 0.0f },
                Vector4{ lBestBoxSideLineLeftStart.x, lBestBoxSideLineLeftStart.y, lZOffset.z, 0.0f },
                Vector4{ lBestBoxSideLineRightStart.x + lBestBoxSideLineRightVec.x,
                         lBestBoxSideLineRightStart.y + lBestBoxSideLineRightVec.y,
                         lZOffset.z, 0.0f },
                Vector4{ lBestBoxSideLineRightStart.x, lBestBoxSideLineRightStart.y, lZOffset.z, 0.0f },
                // the base-parallel pair: slot 0 is the TOP line, slot 1 the BASE line
                Vector4{ lBestBoxTopLineStart.x, lBestBoxTopLineStart.y, lZOffset.z, 0.0f },
                Vector4{ lBestBoxTopLineStart.x + lBaseLineVec.x,
                         lBestBoxTopLineStart.y + lBaseLineVec.y, lZOffset.z, 0.0f },
                Vector4{ lBestBoxBaseLineStart.x, lBestBoxBaseLineStart.y, lZOffset.z, 0.0f },
                Vector4{ lBestBoxBaseLineStart.x + lBaseLineVec.x,
                         lBestBoxBaseLineStart.y + lBaseLineVec.y, lZOffset.z, 0.0f },
            };
            lDebugInfo.mSideLineEnd[0]   = TransformPoint(laLinePoints[0], lLightToWorld);
            lDebugInfo.mSideLineStart[0] = TransformPoint(laLinePoints[1], lLightToWorld);
            lDebugInfo.mSideLineEnd[1]   = TransformPoint(laLinePoints[2], lLightToWorld);
            lDebugInfo.mSideLineStart[1] = TransformPoint(laLinePoints[3], lLightToWorld);
            lDebugInfo.mBaseLineStart[0] = TransformPoint(laLinePoints[4], lLightToWorld);
            lDebugInfo.mBaseLineEnd[0]   = TransformPoint(laLinePoints[5], lLightToWorld);
            lDebugInfo.mBaseLineStart[1] = TransformPoint(laLinePoints[6], lLightToWorld);
            lDebugInfo.mBaseLineEnd[1]   = TransformPoint(laLinePoints[7], lLightToWorld);

            lDebugInfo.mZOffset = VecFloat{ lZOffset.z, lZOffset.z, lZOffset.z, lZOffset.z };

            for (u32 luI = 0; luI < 8; ++luI)
            {
                lDebugInfo.maFrustumPoints[luI] = lFrustumPoints[luI];
            }
            lDebugInfo.mLightToWorld = lLightToWorld;
            // NOTE: mCentreLineStart / mCentreLineEnd (+0x180 / +0x190) are the
            // only two fields this body never writes -- there is no store to
            // them anywhere in @0x827D91B0, so they keep whatever the previous
            // frame (or ComputeTSMMatrix's sibling blob) left there. Reproduced
            // by omission rather than by inventing a fill.

            lDebugInfo.mfAspectRatio = ((lHeight - lWidth >= 0.0f) ? lHeight : lWidth)
                                     / ((lHeight - lWidth >= 0.0f) ? lWidth : lHeight);
        }
    }

    // ========================================================================
    // BrnWorld::ShadowMap::ComputeOptimalViewVolume @0x827D8980
    //
    // Build the cascade's culling frustum as the tightest convex volume that
    // still contains every shadow receiver of lCameraFrustum, seen from the
    // light. Candidates, in the priority order the sort keeps:
    //
    //   1. sbAddLinePlanes: for each of the twelve sub-frustum edges, the plane
    //      through that edge containing the light direction (its light-space
    //      normal is cross(+Z, edge)). Accept only the supporting ones -- the
    //      whole point set must lie on ONE side (negating the plane when that
    //      side is the back one); a plane that cuts the set, or that every point
    //      is coplanar with, is dropped.
    //   2. sbAddOpposingPlanes: every source-frustum plane facing away from the
    //      light (NdotL < 0), scored 2 - NdotL so they sort behind the edges.
    //   3. sbAddNearClipPlane: the light's own near plane, scored 0.
    //   4. sbSortPlanes -> std::sort, then the first Min(count, 8) survive.
    //   5. sbClearPlanes: pad any unused slot with a never-culling plane.
    // ========================================================================
    void ShadowMap::ComputeOptimalViewVolume(const CgsGeometric::Frustum& lCameraFrustum,
                                             const Matrix44Affine& lLightToWorld,
                                             CgsGeometric::Frustum& lViewVolumeOut,
                                             const Vector4* laFrustumPoints,
                                             const Vector4* laFrustumPointsLightSpace)
    {
        // FLAG (byte_82F30E39/38/37/36/35 -- values not in this dossier). All
        //   five are carried TRUE: they are the algorithm's own stage switches,
        //   and with them false the function emits no candidate at all and then
        //   writes one plane straight out of the uninitialised candidate array,
        //   so the shipping defaults cannot be false.
        static const bool sbAddLinePlanes     = true;                 // :1610 FLAG
        static const bool sbAddOpposingPlanes = true;                 // :1708 FLAG
        static const bool sbAddNearClipPlane  = true;                 // :1729 FLAG
        static const bool sbSortPlanes        = true;                 // :1745 FLAG
        static const bool sbClearPlanes       = true;                 // :1754 FLAG

        CandidateViewVolumePlane laCandidatePlanes[32];               // :1604
        u32 luNumCandidatePlanes = 0;                                 // :1605

        // ---- 1. the twelve edge planes (0x827D89E8..0x827D8DF4) -------------
        if (sbAddLinePlanes)
        {
            Vector4 lLineStartLight[12];                              // :1614
            Vector4 lLineEndLight[12];                                // :1615
            Vector4 lLineStartWorld[12];                              // :1616
            for (u32 luI = 0; luI < 12; ++luI)                        // :1618
            {
                const u32 luStart = KA_FRUSTUM_VERT_LINE_INDICES[2 * luI];
                const u32 luEnd   = KA_FRUSTUM_VERT_LINE_INDICES[2 * luI + 1];
                lLineStartLight[luI] = laFrustumPointsLightSpace[luStart];
                lLineEndLight[luI]   = laFrustumPointsLightSpace[luEnd];
                lLineStartWorld[luI] = laFrustumPoints[luStart];
            }

            Vector3Plus laPlanes[12];                                 // :1627
            for (u32 luI = 0; luI < 12; ++luI)                        // :1628
            {
                const Vector4 lLineVec{ lLineEndLight[luI].x - lLineStartLight[luI].x,   // :1630
                                        lLineEndLight[luI].y - lLineStartLight[luI].y,
                                        lLineEndLight[luI].z - lLineStartLight[luI].z,
                                        lLineEndLight[luI].w - lLineStartLight[luI].w };
                const Vector4 lLineVecNormalised = Normalize3(lLineVec);                 // :1631
                // the light points down +Z in light space
                const Vector4 lPlaneNormalLight = Cross3(Vector4{ 0.0f, 0.0f, 1.0f, 0.0f },
                                                         lLineVecNormalised);            // :1632
                const Vector4 lPlaneNormalWorld = TransformVector(lPlaneNormalLight,
                                                                  lLightToWorld);        // :1636
                laPlanes[luI].SetVector3(Vector3{ lPlaneNormalWorld.x, lPlaneNormalWorld.y,
                                                  lPlaneNormalWorld.z, 0.0f });
                laPlanes[luI].SetPlus(Dot3(lPlaneNormalWorld, lLineStartWorld[luI]));
            }

            for (u32 luI = 0; luI < 12; ++luI)                        // :1644
            {
                bool lbPointsBehind  = false;                         // :1646
                bool lbPointsInFront = false;                         // :1647
                Vector3Plus& lPlane  = laPlanes[luI];                 // :1648
                const Vector4 lPlaneVec4{ lPlane.x, lPlane.y, lPlane.z, lPlane.w };  // :1649

                f32 laDistances[8];                                   // :1652
                for (u32 luJ = 0; luJ < 8; ++luJ)
                {
                    laDistances[luJ] = Dot3(lPlaneVec4, laFrustumPoints[luJ]) - lPlane.w;
                }
                f32 lMinDist = laDistances[0];                        // :1662
                f32 lMaxDist = laDistances[0];                        // :1670
                for (u32 luJ = 1; luJ < 8; ++luJ)
                {
                    lMinDist = (laDistances[luJ] < lMinDist) ? laDistances[luJ] : lMinDist;
                    lMaxDist = (laDistances[luJ] > lMaxDist) ? laDistances[luJ] : lMaxDist;
                }
                lbPointsInFront = lMaxDist > K_VERY_SMALL.x;
                lbPointsBehind  = K_MINUS_VERY_SMALL.x > lMinDist;

                if (lbPointsBehind)
                {
                    if (lbPointsInFront)
                    {
                        continue;      // the plane cuts the volume -- not a supporting plane
                    }
                    // flip it so the whole set is in front (the vxor sign-bit
                    // negation, written back through the laPlanes reference)
                    lPlane.x = -lPlane.x; lPlane.y = -lPlane.y;
                    lPlane.z = -lPlane.z; lPlane.w = -lPlane.w;
                }
                else if (!lbPointsInFront)
                {
                    continue;          // degenerate -- every point is on the plane
                }

                laCandidatePlanes[luNumCandidatePlanes].mPlane = lPlane;
                laCandidatePlanes[luNumCandidatePlanes].mScore =
                    (luI < 4) ? K_LOW_PRIORITY : K_HIGH_PRIORITY;
                ++luNumCandidatePlanes;
            }
        }

        // ---- 2. the light vector + the opposing source planes ---------------
        const Vector4 lLightVector = TransformVector(Vector4{ 0.0f, 0.0f, 1.0f, 0.0f },
                                                     lLightToWorld);           // :1707
        if (sbAddOpposingPlanes)
        {
            for (u32 luI = 0; luI < 6; ++luI)                                  // :1712
            {
                const rw::collision::Plane lPlane = lCameraFrustum.GetPlaneByIndex(luI); // :1715
                const f32 lNDotL = Dot3(Vector4{ lPlane.x, lPlane.y, lPlane.z, lPlane.w },
                                        lLightVector);                         // :1716
                if (0.0f > lNDotL)
                {
                    laCandidatePlanes[luNumCandidatePlanes].mPlane =
                        Vector3Plus{ lPlane.x, lPlane.y, lPlane.z, lPlane.w };
                    const f32 lfScore = 2.0f - lNDotL;    // vspltisw 2 + vcfsx
                    laCandidatePlanes[luNumCandidatePlanes].mScore =
                        VecFloat{ lfScore, lfScore, lfScore, lfScore };
                    ++luNumCandidatePlanes;
                }
            }
        }

        // ---- 3. the light's own near clip plane (0x827D8EC0..0x827D8F14) ----
        if (sbAddNearClipPlane)
        {
            Vector3Plus& lNearPlane = laCandidatePlanes[luNumCandidatePlanes].mPlane; // :1732
            const Vector4 lLightPosWorldSpace{ lLightToWorld.wAxis.x, lLightToWorld.wAxis.y,
                                               lLightToWorld.wAxis.z, lLightToWorld.wAxis.w }; // :1735
            const f32 lNearClip = mfShadowMapNearPlane;                        // :1736
            lNearPlane.SetVector3(Vector3{ lLightVector.x, lLightVector.y, lLightVector.z, 0.0f });
            lNearPlane.SetPlus(Dot3(lLightPosWorldSpace, lLightVector) + lNearClip);
            laCandidatePlanes[luNumCandidatePlanes].mScore = VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };
            ++luNumCandidatePlanes;
        }

        // ---- 4. sort + truncate to the frustum's plane budget ---------------
        if (sbSortPlanes)                                                      // :1745
        {
            std::sort(laCandidatePlanes, laCandidatePlanes + luNumCandidatePlanes);
        }
        u32 luNumPlanes = CgsNumeric::Min(luNumCandidatePlanes, 8u);           // :1750

        // ---- 5. pad the unused slots (0x827D8F9C..0x827D9174) ---------------
        // The eight never-culling defaults, built once (the X360 dword_8300FD40
        // guard word is exactly the C++ function-local-static init guard).
        static const Vector3Plus saClearPlanes[8] =                            // :1755
        {
            {  0.0f,  1.0f,  0.0f, -1000000.0f },
            {  0.0f, -1.0f,  0.0f, -1000000.0f },
            {  1.0f,  0.0f,  0.0f, -1000000.0f },
            { -1.0f,  0.0f,  0.0f, -1000000.0f },
            {  0.0f,  0.0f,  1.0f, -1000000.0f },
            {  0.0f,  0.0f, -1.0f, -1000000.0f },
            {  0.0f,  1.0f,  0.0f, -1000000.0f },
            {  0.0f, -1.0f,  0.0f, -1000000.0f },
        };

        u32 luNumPlanesOut = luNumPlanes;
        if (sbClearPlanes)
        {
            for (u32 luI = luNumPlanes; luI < 8; ++luI)                        // :1768
            {
                laCandidatePlanes[luI].mPlane = saClearPlanes[luI];
            }
            luNumPlanesOut = 8;
        }
        else if (luNumPlanes == 0)
        {
            luNumPlanesOut = 1;
        }

        // Repeat the last accepted plane over the remaining slots. (Inert with
        // sbClearPlanes set -- luNumPlanesOut is already 8 -- but it is what the
        // X360 emits, so it is reproduced rather than dropped.)
        for (u32 luI = luNumPlanesOut; luI < 8; ++luI)
        {
            laCandidatePlanes[luI].mPlane = laCandidatePlanes[luNumPlanesOut - 1].mPlane;
        }

        for (u32 luI = 0; luI < luNumPlanesOut; ++luI)                         // :1789
        {
            const Vector3Plus& lrPlane = laCandidatePlanes[luI].mPlane;
            lViewVolumeOut.SetPlaneByIndex(luI, rw::collision::Plane(
                Vector4{ lrPlane.x, lrPlane.y, lrPlane.z, lrPlane.w }));
        }
    }

    // @0x827C1BB8 (~1650 pseudocode lines): the immediate-mode debug renderer
    // over gaTsmDebugRenderInfo / the bounding-box debug blobs. Dormant unless
    // a per-map mbDebugRender toggle is set (Construct defaults all false).
    void ShadowMap::DebugRender(u32 /*luIndex*/) const
    {
        CGS_ASSERT(false,
                   "ShadowMap::DebugRender: FLAG -- debug-only VMX/IM pipeline @0x827C1BB8 not yet "
                   "reconstructed; set no maTsmBBInfo[].mbDebugRender toggle until it lands");
    }
}
