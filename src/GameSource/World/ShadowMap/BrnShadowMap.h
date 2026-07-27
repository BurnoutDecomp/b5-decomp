#pragma once

// GameSource/World/ShadowMap/BrnShadowMap.h
//
// BrnWorld::ShadowMap -- the shadow-map manager: it owns the (up to
// BrnWorld::KU_NUM_SHADOW_MAPS == 3) shadow-map cameras / cached matrices / per-map
// trapezoidal-shadow-map (TSM) bounding info, and drives the per-frame view-volume /
// camera-matrix computation the renderer consumes when rendering into the shadow maps.
//
// HOME for the 7-function BrnShadowMap.cpp TU (compilation home path is
// "GameSource/Unity/../World/ShadowMap/BrnShadowMap.cpp" per the X360 unity build).
//
// LAYOUT: recovered from the DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/World/
// ShadowMap/BrnShadowMap.h), which attests the full named member list. Per the project
// convention (semantic parity by named member, not byte offset -- the x64 gate rule), the
// members are declared by name/logical type; only the two fields the RECONSTRUCTED bodies
// in the .cpp actually touch (muCurrentShadowMap, KU_NUM_SHADOW_MAPS-bounded arrays) have
// asm-attested provenance below. Most member functions are DECLARATION-ONLY (see the .cpp
// header comment) -- the class is heavily VMX-dominated and this wave only recovers the two
// tractable scalar/branchless helpers.
//
// FLAG (ambiguous member types, forward/opaque per AGENTS.md "forward-declaration is the
//   exception (c) no reference exists to reconstruct the type"):
//   * TsmBBInfo::mYExtents -- the DWARF dump resolves this field's type to THREE DIFFERENT
//     bogus namespaces across its duplicated CU entries (SmoothStep::Vector2,
//     AISection::Vector2, Basic2dColouredVertex::Vector2), none of which is a real Vector2
//     home in this codebase -- a DWARF cross-CU merge artifact (see the project's
//     wiki-name-retrofit lesson re: DWARF variant misses), not a real nested type. Modelled
//     as the repo-canonical rw::math::vpu Vector2 (BrnCommonTypes.h), which every other
//     "Vector2" field in the codebase resolves to.
//   * mTextureStateResource -- DWARF names its type only "Resource" (unqualified). The only
//     candidate, rw::Resource, is forward-declared project-wide with NO reconstructed
//     complete-type home yet (no rwcore_structs.h in this tree) -- left as an opaque
//     byte-sized placeholder (rw::Resource is a fixed-size POD header on X360) rather than
//     fabricating a layout.
//
// None of the DECLARATION-ONLY functions below are called by the two bodied functions
// (CalcOptimisedLod / CalcLodDistanceModifier), which only touch muCurrentShadowMap.

#include "types.hpp"
#include "BrnCommonTypes.h"                                             // Vector2/Vector3/Matrix44/Matrix44Affine/VecFloat
#include "GameShared/GameClasses/Graphics/CgsCamera.h"                  // CgsGraphics::Camera
#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"     // CgsGeometric::Frustum
#include "GameShared/GameClasses/Graphics/CgsModel.h"                   // CgsGraphics::Model::State (CalcOptimisedLod param/return)
#include "GameSource/Director/Camera/Camera.h"                          // BrnDirector::Camera::Camera (maShadowMapCamera / the render-camera params)
#include "pc/gcm/renderengine/renderstates.h"                           // renderengine::TextureState (+ its Parameters)

namespace rw { struct Resource; }   // opaque -- see FLAG above; no reconstructed complete-type home yet

namespace BrnWorld
{
    // BrnShadowMap.h:41 (DWARF). The kind of shadow map a given slot is configured as.
    enum ShadowMapType
    {
        E_SHADOWMAP_TYPE_ORTHO       = 0,
        E_SHADOWMAP_TYPE_TSM         = 1,
        E_SHADOWMAP_TYPE_BOUNDINGBOX = 2,
        E_SHADOWMAP_TYPE_CACHED      = 3,
    };

    // BrnShadowMap.h:60 (DWARF). The number of concurrently-live shadow map slots.
    const u32 KU_NUM_SHADOW_MAPS = 3;

    // BrnShadowMap.h:67 (DWARF).
    struct ShadowMap
    {
        // BrnShadowMap.h:69 (DWARF). Per-shadow-map trapezoidal-shadow-map (TSM) bookkeeping.
        struct TsmBBInfo
        {
            f32                      mfNearClip;           // BrnShadowMap.h:71
            f32                      mfFarClip;             // BrnShadowMap.h:72
            CgsGraphics::Camera      mCamera;               // BrnShadowMap.h:73
            Matrix44                 mWorldToLight;         // BrnShadowMap.h:74
            Matrix44Affine           mWorldToLightView;      // BrnShadowMap.h:75
            Matrix44Affine           mLightToWorldView;      // BrnShadowMap.h:76
            bool                     mbInvertCullMode;       // BrnShadowMap.h:77
            Matrix44                 mBestFitMatrix;         // BrnShadowMap.h:78
            CgsGeometric::Frustum    mSubFrustum;            // BrnShadowMap.h:79
            f32                      mfTsmSlideBack;         // BrnShadowMap.h:81
            bool                     mbDebugRender;          // BrnShadowMap.h:82
            bool                     mbSceneOptimised;       // BrnShadowMap.h:84
            Vector2                  mYExtents;              // BrnShadowMap.h:85 -- see FLAG above
        };

        // ---- lifecycle / per-frame update (all DECLARATION-ONLY, VMX-dominated) --------

        // BrnShadowMap.h:89, X360 0x827B43E8 (EXECUTED in goal trace). Initialise the
        // shadow-map manager: seed the render toggles, per-slot config (ortho scale /
        // at-offset / type / enabled / cached-matrix identity), the TSM sub-frustum
        // table, the fade/bias scalars, then register the debug-variable surface.
        // RECONSTRUCTED in the .cpp (destub wave 2026-07-26): every store is a named-
        // member splat (the stvx128s are 16-byte zero/identity-row stores, rendered as
        // member assignments per the x64 semantic-parity rule) + the DebugInterface
        // automatic-handle registration idiom.
        void Construct();

        // BrnShadowMap.h:93. Per-frame update entry point.
        // DECLARATION-ONLY + FLAG: not attested by a standalone X360 body in this dossier
        //   (folds into the VMX-dominated per-frame pipeline); declared for shape only.
        void Update(Vector3 lv3Arg);

        // BrnShadowMap.h:98, X360 0x827DA820. Compute the per-map shadow cameras (light
        // transforms + projections, per-type frustum/TSM/bounding-box solves, the dynamic
        // far-clip refit, the shader constants) for this frame from the light direction
        // and the render camera. RECONSTRUCTED in the .cpp (shadow-camera wave 2026-07-27).
        //
        // RECONCILE (DWARF BrnShadowMap.h:98 `CalculateShadowMapCameras(Vector3, const
        // Camera*)`): the unqualified `Camera` resolves to the DIRECTOR camera -- the body
        // copies r4 with BrnDirector::Camera::Camera::operator= into maShadowMapCamera
        // (352-byte stride), pokes its mfFOV (+0x58) and hands it to CopyToCgsCamera /
        // ValidateTransformWithDebugInfo / SetConstants. This is the real overload:
        void CalculateShadowMapCameras(Vector3 lv3LightDirection, const BrnDirector::Camera::Camera* lpRenderCamera);

        // BrnShadowMap.h:102. Update the focus point the shadow maps are centred on.
        // DECLARATION-ONLY (not attested by a standalone X360 body in this dossier).
        void UpdateShadowMapFocusPoint(Vector3 lv3Arg);

        // ---- simple accessors (inlined on console -- no standalone X360 bodies; the
        //      ones the WorldModule render feed reads are BODIED in the .cpp, destub
        //      wave 2026-07-26, semantics attested at the @0x827DADF8 / @0x827C96D8
        //      call sites; the rest stay declaration-only) ---------------------------

        f32                  GetFarPlane() const;                          // BrnShadowMap.h:105
        // ---- ADDITIVE (WorldModule::GenerateFrustumQueries @0x827DADF8 gates the
        //      whole shadow path on this, and reads the per-cascade cameras for the
        //      cascade frustum queries). Bodied in the .cpp.
        bool                 IsEnabled() const;
        const CgsGraphics::Camera* GetCascadeCamera( s32 liCascade ) const;

        bool                 GetRenderShadowMapView() const;               // BrnShadowMap.h:108
        bool                 GetRenderWorldIntoShadowMap() const;          // BrnShadowMap.h:111
        bool                 GetRenderRaceCarsIntoShadowMap() const;       // BrnShadowMap.h:114
        bool                 GetRenderTrafficIntoShadowMap() const;        // BrnShadowMap.h:117
        bool                 GetRenderMultipleShadowMaps() const;          // BrnShadowMap.h:120
        bool                 GetRenderPropsIntoShadowMap() const;          // BrnShadowMap.h:123
        // RECONCILE 2026-07-27: returns the DIRECTOR camera slot (maShadowMapCamera
        // retype below); no committed caller existed.
        BrnDirector::Camera::Camera* GetShadowMapCamera(s32 liIndex);      // BrnShadowMap.h:126
        bool                 GetRenderingShadowMap();                     // BrnShadowMap.h:129
        bool                 UseZOnlyRenderingPath();                     // BrnShadowMap.h:135
        void                 SetRenderingShadowMap(bool lbValue);          // BrnShadowMap.h:141
        CgsGraphics::Camera* GetCgsShadowMapCamera(s32 liIndex);           // BrnShadowMap.h:147
        bool                 GetRenderRaceCarsNearOnly() const;            // BrnShadowMap.h:150
        bool                 GetRenderTrafficNearOnly() const;             // BrnShadowMap.h:153
        bool                 GetRenderPropsNearOnly() const;               // BrnShadowMap.h:156

        // BrnShadowMap.h:161, X360 0x827C1BB8. Debug-render the shadow-map frustums/
        // trapezoids/cameras for a given map index.
        // FLAG (assert-trap body in the .cpp, 2026-07-27): a ~1650-line immediate-mode-
        //   renderer VMX pipeline (line-list building from gTsmDebugRenderInfo /
        //   gBoundingBoxDebugRenderInfo); the rules forbid paraphrasing it. Reached ONLY
        //   when a per-map maTsmBBInfo[i].mbDebugRender debug toggle is set (Construct
        //   defaults all three false), so the trap is dormant on the shipping path.
        void DebugRender(u32 luIndex) const;

        const CgsGeometric::Frustum& GetFrustum(u32 luIndex) const;        // BrnShadowMap.h:165

        // BrnShadowMap.h:170, X360 0x827B42E8 (RECONSTRUCTED in the .cpp). If shadow-LOD
        // optimisation is enabled, clamp the requested LOD state (leHigherDetailState) to
        // no more detailed than the current shadow map's configured LOD modifier offset
        // from leLowerDetailState, via the branchless CgsNumeric::Min (asserted against the
        // branchy reference, matching the CgsBranchlessOperations.h::Min inlining).
        CgsGraphics::Model::State CalcOptimisedLod(CgsGraphics::Model::State leLowerDetailState,
                                                    CgsGraphics::Model::State leHigherDetailState);

        // BrnShadowMap.h:173, X360 0x827B43A8 (RECONSTRUCTED in the .cpp). Returns the
        // broadcast VecFloat LOD-distance modifier for the current shadow map (or an
        // all-zero VecFloat while the shadow-LOD-distance optimisation is disabled).
        VecFloat CalcLodDistanceModifier();

        // WorldEntityModule::GenerateDispatchLists @0x822D5AB0 reads the leading
        // member pair {mbRenderingShadowMap, mbUseZOnlyRenderingPath} (DWARF :226/:227)
        // to select the shadow pass. Additive read accessors for that consumer.
        bool IsRenderingShadowMap() const     { return mbRenderingShadowMap; }
        bool IsUsingZOnlyRenderingPath() const { return mbUseZOnlyRenderingPath; }

        void     SetCurrentShadowMap(u32 luIndex);                        // BrnShadowMap.h:176
        u32      GetCurrentShadowMap();                                   // BrnShadowMap.h:182
        // BrnShadowMap.h:188, X360 0x827C1630 (RECONSTRUCTED in the .cpp 2026-07-27):
        // select the near/far CSM pair for an object at lfDistance and stage shader
        // constant c17 = { (f32)sel, (f32)(sel+1), maTsmBBInfo[sel].mfFarClip, 0 }.
        void     ObjectCSMSelect(f32 lfDistance) const;
        void     SetConstantsForEnvmap();
        // ---- ADDITIVE (WorldModule::GenerateShadowMapDispatchLists @0x827C96D8
        //      drives the per-cascade state: the cascade selector @X360 +5364 and
        //      the rendering latch, the map's FIRST byte, which the world entity
        //      module's dispatch feed reads back through GetRenderingShadowMap). ----
        void     SetCurrentCascadeIndex( u32 luCascade );                                 // BrnShadowMap.h:191

    protected:
        // BrnShadowMap.h:196, X360 0x827C16E0 (RECONSTRUCTED in the .cpp 2026-07-27):
        // stage the per-frame shadow shader constants (mCachedOffsetWorld, the c14
        // world-to-shadow matrix triple, c15/c16 fade+bias vectors, the c17 seed).
        // RECONCILE: the DWARF `const Camera*` is the DIRECTOR camera (the caller
        // passes CalculateShadowMapCameras' r4 straight through; the body reads its
        // mTransform At row).
        void              SetConstants(const BrnDirector::Camera::Camera* lpCamera);
        bool              CameraMatrixNeedsUpdate(Matrix44Affine lm44Matrix, u32 luIndex);    // BrnShadowMap.h:201
        void              ComputeTSMMatrix(u32 luIndex, CgsGraphics::Camera& lrCamera);       // BrnShadowMap.h:206

        // BrnShadowMap.h:211, X360 0x827D91B0. Compute the bounding-box shadow map's
        // best-fit post-projection for a given map slot: clamp the slot's sub-frustum,
        // flatten its eight corners into the cascade's post-projective light plane, fit
        // the minimum-area oriented 2D box around them (an optional rotation search
        // weighted by mfIdealAspectRatio), and turn that box into
        // maTsmBBInfo[luMapIndex].mBestFitMatrix. Also inverts the cascade view matrix
        // into mLightToWorldView and drives ComputeOptimalViewVolume for the culling
        // frustum. RECONSTRUCTED in the .cpp (bounding-box shadow wave 2026-07-27b);
        // every local/constant name is the DecFIGS DWARF ground truth.
        // Reached for maps configured E_SHADOWMAP_TYPE_BOUNDINGBOX (the KA_SHADOWMAPTYPE
        // rodata is un-dumped and carried as ORTHO, but the dynamic-far-clip gate in
        // CalculateShadowMapCameras tests maShadowMapTypes[0] == BOUNDINGBOX, so this is
        // a real runtime path once that table is recovered).
        void ComputeBoundingBoxMatrix(u32 luMapIndex, CgsGraphics::Camera& lCamera);

        BrnWorld::ShadowMapType GetShadowMapType(u32 luIndex);                                // BrnShadowMap.h:215

        // BrnShadowMap.h:223, X360 0x827D8980. Compute the optimal (minimal) view volume
        // enclosing the render frustum from the light's point of view: the supporting
        // edge planes of the sub-frustum silhouette, the source planes facing away from
        // the light, and the light's own near plane, ranked by score and truncated to the
        // frustum's eight plane slots. RECONSTRUCTED in the .cpp (2026-07-27b).
        //
        // RECONCILE (DWARF BrnShadowMap.cpp:1600 parameter names): the matrix argument is
        // the LIGHT-TO-WORLD view matrix (`lLightToWorld` -- the caller passes
        // maTsmBBInfo[i].mLightToWorldView), not a world-to-light one as the earlier
        // placeholder decl spelled it; it is taken by const reference (the definition-site
        // DWARF and the X360 r5 pointer argument agree). The two point arrays are the
        // world-space and flattened light-space sub-frustum corners.
        // RECONCILE (element type): the DWARF spells both arrays `const
        // rw::math::vpu::Vector3 *`; they are produced by CgsGeometric::Frustum::
        // CalcVertices, whose repo out-param is Vector4* -- the same 16-byte lane, and only
        // lanes 0..2 are read. Kept as Vector4* so the call needs no cast or copy.
        void ComputeOptimalViewVolume(const CgsGeometric::Frustum& lCameraFrustum,
                                      const Matrix44Affine& lLightToWorld,
                                      CgsGeometric::Frustum& lViewVolumeOut,
                                      const Vector4* laFrustumPoints,
                                      const Vector4* laFrustumPointsLightSpace);

    private:
        // ---- data (BrnShadowMap.h:226+ DWARF) ------------------------------------------
        bool   mbRenderingShadowMap;                              // :226
        bool   mbUseZOnlyRenderingPath;                           // :227

        // :229 -- RECONCILE 2026-07-27: the DWARF's unqualified `Camera[3]` is the
        // DIRECTOR camera (X360 CalculateShadowMapCameras copies into it with
        // BrnDirector::Camera::Camera::operator= at 352-byte stride from this+0x10,
        // writes mfFOV @slot+0x58 and mTransform @slot+0x00; the sibling
        // maCgsShadowMapCamera strides 368 from this+0x430 == 0x10 + 3*352).
        BrnDirector::Camera::Camera maShadowMapCamera[KU_NUM_SHADOW_MAPS]; // :229
        CgsGraphics::Camera    maCgsShadowMapCamera[KU_NUM_SHADOW_MAPS];   // :230

        Matrix44Affine maCachedMatrix[KU_NUM_SHADOW_MAPS];        // :232
        Vector3        mCachedOffsetWorld;                        // :233

        renderengine::TextureState*            mpTextureState;          // :237
        renderengine::TextureState::Parameters mTextureStateParams;     // :238

        // :239 -- opaque placeholder; see the class-header FLAG re: rw::Resource having no
        // reconstructed complete-type home yet. Sized/aligned generously (rw::Resource is a
        // small fixed POD descriptor on X360; this is provenance-honest storage, not a cast).
        struct OpaqueResourcePlaceholder { alignas(8) u8 maBytes[16]; } mTextureStateResource; // :239

        TsmBBInfo maTsmBBInfo[KU_NUM_SHADOW_MAPS];                // :242

        f32  mafShadowMapOrthoScale[KU_NUM_SHADOW_MAPS];          // :244
        f32  mafShadowMapAtOffset[KU_NUM_SHADOW_MAPS];            // :245
        CgsGeometric::Frustum maFrustum[KU_NUM_SHADOW_MAPS];      // :246

        BrnWorld::ShadowMapType maShadowMapTypes[KU_NUM_SHADOW_MAPS]; // :248
        bool mabMapEnabled[KU_NUM_SHADOW_MAPS];                   // :249

        f32  mfShadowMapNearPlane;                                // :253
        f32  mfShadowMapFarPlane;                                 // :254
        f32  mfEyeOffset;                                         // :255
        Vector3 mShadowMapFocusPoint;                             // :256

        f32  mfFadeStartDistance;                                 // :258

        // :260 (DWARF). The currently-active shadow map slot index (0..KU_NUM_SHADOW_MAPS-1).
        // X360-attested: CalcOptimisedLod / CalcLodDistanceModifier read it at CONSOLE +0x14F4
        // (`lwz r10, 0x14F4(this)`) to index KA_SHADOWMAP_LOD_MODIFIER / KA_SHADOWMAP_LOD_DISTANCE_MODIFIER.
        u32 muCurrentShadowMap;

        bool mbRenderShadowMapView;                               // :263
        bool mbRenderWorldIntoShadowMap;                          // :264
        bool mbRenderRaceCarsIntoShadowMap;                       // :265
        bool mbRenderTrafficIntoShadowMap;                        // :266
        bool mbRenderMultipleShadowMaps;                          // :267
        bool mbRenderPropsIntoShadowMap;                          // :268

        bool mbRenderPropsNearOnly;                               // :270
        bool mbRenderTrafficNearOnly;                             // :271
        bool mbRenderRaceCarsNearOnly;                            // :272

        bool mbUpdateDebugRender;                                 // :274
        bool mbUpdateTsmCamera;                                   // :275
        bool mbPreferShortAndFatProjection;                       // :276
        bool mbShowLightFrustumInDebugRender;                     // :277

        bool mbOptimiseForIdealAspectRatio;                       // :279
        f32  mfIdealAspectRatio;                                  // :280

        f32  mfNDotLFallOffCutoffWorld;                           // :282
        f32  mfNDotLFallOffScaleWorld;                            // :283

        f32  mfDynamicFarClipOffset;                              // :285

        f32  mfShadowFadeToValue;                                 // :287
        bool mbDynamicFarClipPlane;                               // :288

        f32  mfVariableBiasMin;                                   // :290
        f32  mfBiasFrustumLength;                                 // :291
    };
}
