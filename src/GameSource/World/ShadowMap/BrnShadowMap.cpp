#include "GameSource/World/ShadowMap/BrnShadowMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h" // CgsNumeric::Min (CalcOptimisedLod)

// ============================================================================
// GameSource/Unity/../World/ShadowMap/BrnShadowMap.cpp
//
// BrnWorld::ShadowMap -- the shadow-map manager (see BrnShadowMap.h). This wave
// bodies only the two tractable, VMX-light functions of the 7-function class TU:
//
//   CalcOptimisedLod          @ 0x827B42E8  (RECONSTRUCTED below -- pure scalar int)
//   CalcLodDistanceModifier   @ 0x827B43A8  (RECONSTRUCTED below -- trivial branch + splat)
//
// DECLARATION-ONLY (see BrnShadowMap.h for the per-function FLAG comments): Construct,
// CalculateShadowMapCameras, ComputeBoundingBoxMatrix, ComputeOptimalViewVolume,
// DebugRender. Each is a heavily VMX-laden pipeline (candidate-view-volume-plane
// sort/intersection math, trapezoidal-shadow-map best-fit solve, debug line-list
// building) that the reconstruction rules forbid paraphrasing to scalar C++; they stay
// declared in the header until their own dependency TUs (CgsGeometric::Frustum's
// CalcVertices/SetFromRwFrustum, the matrix33/44 operation platform inlines,
// BrnDirector::Camera::CopyToCgsCamera, CgsDev::DebugInterface, ...) land.
// ============================================================================

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
}
