#include "GameSource/World/ShadowMap/BrnShadowMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h" // CgsNumeric::Min (CalcOptimisedLod)

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
        // 0x80*i (maFrustum[i]).
        maCgsShadowMapCamera[luIndex].GetCgsFrustumParallel(&maFrustum[luIndex]);

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
}
