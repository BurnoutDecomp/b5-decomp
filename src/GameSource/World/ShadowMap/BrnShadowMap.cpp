#include "GameSource/World/ShadowMap/BrnShadowMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"            // CgsCore::SnPrintf (Construct's per-CSM debug paths)
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h" // CgsNumeric::Min (CalcOptimisedLod)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h" // Construct's debug-variable registration
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"           // CgsDev::DebugUI::StringList (type options)
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"    // ShaderConstantTable (SetConstantsForEnvmap)

#include <cmath>     // std::sqrt (ComputeTSMMatrix -- vrsqrtefp + 2x NR de-optimised exact)
#include <cstring>   // memcpy (the 128-byte frustum copy at 0x827C1274)

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
    // FLAG assert-trap bodies (shadow-camera wave 2026-07-27) -- the three
    // remaining VMX pipelines of this TU. Each is documented in the header;
    // the full recovered analysis (entry structure, member roles, shared BSS,
    // callee sets, emulator tooling) is in the session vmx_wave_log.md so the
    // next wave can pick them up without re-deriving.
    //
    // Reachability with the shipping defaults: KA_SHADOWMAPTYPE is un-dumped
    // rodata carried as ORTHO, which routes CalculateShadowMapCameras through
    // GetCgsFrustumParallel -- none of these traps is on that path; they arm
    // only when a map is switched to TSM/BOUNDINGBOX (debug menu or the real
    // rodata values once recovered) or the per-map DebugRender toggle is set.
    // ========================================================================

    // @0x827D91B0 (~1450 instructions). Recovered entry structure: gated on
    // mbUpdateTsmCamera it snapshots the cascade's mViewProjection/mView into
    // maTsmBBInfo[i].mWorldToLight/mWorldToLightView, clones lrCamera into
    // maTsmBBInfo[i].mCamera and clamps its near/far to the TSM sub-frustum
    // range; then GetFrustumPerspective -> mSubFrustum.SetFromRwFrustum ->
    // CalcVertices, transforms the 8 verts into light space and runs the
    // extent/optimal-view-volume reduction that fills mBestFitMatrix and the
    // shared constant-matrix BSS (unk_8300F5C0/F630/F980 block).
    void ShadowMap::ComputeBoundingBoxMatrix(u32 /*luIndex*/, CgsGraphics::Camera& /*lrCamera*/)
    {
        CGS_ASSERT(false,
                   "ShadowMap::ComputeBoundingBoxMatrix: FLAG -- VMX pass @0x827D91B0 not yet "
                   "faithfully decoded (see BrnShadowMap.h + vmx_wave_log.md); reachable only for "
                   "E_SHADOWMAP_TYPE_BOUNDINGBOX maps");
    }

    // @0x827D8980 (~520 instructions): the candidate-view-volume solve --
    // walks the 6 source planes (GetPlaneByIndex), builds
    // BrnWorld::CandidateViewVolumePlane records, std::_Sorts them, and
    // intersects the winning silhouette set into lrOutFrustum.
    void ShadowMap::ComputeOptimalViewVolume(const CgsGeometric::Frustum& /*lrFrustum*/,
                                             Matrix44Affine /*lm44WorldToLight*/,
                                             CgsGeometric::Frustum& /*lrOutFrustum*/,
                                             const rw::math::vpu::Vector3* /*lpArg4*/,
                                             const rw::math::vpu::Vector3* /*lpArg5*/)
    {
        CGS_ASSERT(false,
                   "ShadowMap::ComputeOptimalViewVolume: FLAG -- VMX pass @0x827D8980 not yet "
                   "faithfully decoded (see BrnShadowMap.h + vmx_wave_log.md); reachable only "
                   "through ComputeBoundingBoxMatrix");
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
