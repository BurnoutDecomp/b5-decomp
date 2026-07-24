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
        // shadow-map manager: seed the per-map cameras/frustums/matrices, the TSM info
        // table, the debug-render options and the texture-state resource.
        // DECLARATION-ONLY + FLAG: the body is a long VMX-laden field-splat/zero-fill
        //   sequence over the entire aggregate plus CgsDev::DebugInterface variable
        //   registration calls; the reconstruction rules forbid paraphrasing the VMX
        //   pipeline to scalar. Left declaration-only.
        void Construct();

        // BrnShadowMap.h:93. Per-frame update entry point.
        // DECLARATION-ONLY + FLAG: not attested by a standalone X360 body in this dossier
        //   (folds into the VMX-dominated per-frame pipeline); declared for shape only.
        void Update(Vector3 lv3Arg);

        // BrnShadowMap.h:98, X360 0x827DA820. Compute the per-map shadow cameras (view
        // volumes, best-fit matrices, TSM trapezoids) for this frame from the light
        // direction and the render camera.
        // DECLARATION-ONLY + FLAG: a very large VMX/float pipeline (candidate view-volume
        //   plane sorting, line-intersection math, best-fit matrix solve) the rules forbid
        //   paraphrasing to scalar. Calls the also-declaration-only ComputeBoundingBoxMatrix/
        //   ComputeOptimalViewVolume/DebugRender.
        void CalculateShadowMapCameras(Vector3 lv3LightDirection, const CgsGraphics::Camera* lpRenderCamera);

        // BrnShadowMap.h:102. Update the focus point the shadow maps are centred on.
        // DECLARATION-ONLY (not attested by a standalone X360 body in this dossier).
        void UpdateShadowMapFocusPoint(Vector3 lv3Arg);

        // ---- simple accessors (DECLARATION-ONLY: no standalone X360 body in this dossier;
        //      trivial member getters/setters the DWARF attests but this wave's boot trace
        //      did not execute / this dossier does not carry a body for) -------------------

        f32                  GetFarPlane() const;                          // BrnShadowMap.h:105
        // ---- ADDITIVE (WorldModule::GenerateFrustumQueries @0x827DADF8 gates the
        //      whole shadow path on this, and reads the per-cascade cameras for the
        //      cascade frustum queries). Declaration-only; bodies with this TU.
        bool                 IsEnabled() const;
        const CgsGraphics::Camera* GetCascadeCamera( s32 liCascade ) const;

        bool                 GetRenderShadowMapView() const;               // BrnShadowMap.h:108
        bool                 GetRenderWorldIntoShadowMap() const;          // BrnShadowMap.h:111
        bool                 GetRenderRaceCarsIntoShadowMap() const;       // BrnShadowMap.h:114
        bool                 GetRenderTrafficIntoShadowMap() const;        // BrnShadowMap.h:117
        bool                 GetRenderMultipleShadowMaps() const;          // BrnShadowMap.h:120
        bool                 GetRenderPropsIntoShadowMap() const;          // BrnShadowMap.h:123
        CgsGraphics::Camera* GetShadowMapCamera(s32 liIndex);              // BrnShadowMap.h:126
        bool                 GetRenderingShadowMap();                     // BrnShadowMap.h:129
        bool                 UseZOnlyRenderingPath();                     // BrnShadowMap.h:135
        void                 SetRenderingShadowMap(bool lbValue);          // BrnShadowMap.h:141
        CgsGraphics::Camera* GetCgsShadowMapCamera(s32 liIndex);           // BrnShadowMap.h:147
        bool                 GetRenderRaceCarsNearOnly() const;            // BrnShadowMap.h:150
        bool                 GetRenderTrafficNearOnly() const;             // BrnShadowMap.h:153
        bool                 GetRenderPropsNearOnly() const;               // BrnShadowMap.h:156

        // BrnShadowMap.h:161, X360 0x827C1BB8. Debug-render the shadow-map frustums/
        // trapezoids/cameras for a given map index.
        // DECLARATION-ONLY + FLAG: a large immediate-mode-renderer VMX pipeline (line-list
        //   building from gTsmDebugRenderInfo / gBoundingBoxDebugRenderInfo); the rules
        //   forbid paraphrasing it. Called (declaration-only) by CalculateShadowMapCameras.
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
        void     ObjectCSMSelect(f32 lfArg) const;                        // BrnShadowMap.h:188
        void     SetConstantsForEnvmap();
        // ---- ADDITIVE (WorldModule::GenerateShadowMapDispatchLists @0x827C96D8
        //      drives the per-cascade state: the cascade selector @X360 +5364 and
        //      the rendering latch, the map's FIRST byte, which the world entity
        //      module's dispatch feed reads back through GetRenderingShadowMap). ----
        void     SetCurrentCascadeIndex( u32 luCascade );                                 // BrnShadowMap.h:191

    protected:
        void              SetConstants(const CgsGraphics::Camera* lpCamera);                 // BrnShadowMap.h:196
        bool              CameraMatrixNeedsUpdate(Matrix44Affine lm44Matrix, u32 luIndex);    // BrnShadowMap.h:201
        void              ComputeTSMMatrix(u32 luIndex, CgsGraphics::Camera& lrCamera);       // BrnShadowMap.h:206

        // BrnShadowMap.h:211, X360 0x827D91B0. Compute the axis-aligned-bounding-box shadow
        // map's ortho projection matrix for a given map slot.
        // DECLARATION-ONLY + FLAG: a VMX min/max-reduction + matrix-build pipeline; the
        //   rules forbid paraphrasing it. Called (declaration-only) by CalculateShadowMapCameras.
        void ComputeBoundingBoxMatrix(u32 luIndex, CgsGraphics::Camera& lrCamera);

        BrnWorld::ShadowMapType GetShadowMapType(u32 luIndex);                                // BrnShadowMap.h:215

        // BrnShadowMap.h:223, X360 0x827D8980. Compute the optimal (minimal) view volume
        // enclosing the render frustum from the light's point of view.
        // DECLARATION-ONLY + FLAG: a large candidate-plane / view-volume-intersection VMX
        //   pipeline (CandidateViewVolumePlane sort, line-intersection tests); the rules
        //   forbid paraphrasing it. Called (declaration-only) by ComputeBoundingBoxMatrix.
        void ComputeOptimalViewVolume(const CgsGeometric::Frustum& lrFrustum, Matrix44Affine lm44WorldToLight,
                                       CgsGeometric::Frustum& lrOutFrustum,
                                       const rw::math::vpu::Vector3* lpArg4, const rw::math::vpu::Vector3* lpArg5);

    private:
        // ---- data (BrnShadowMap.h:226+ DWARF) ------------------------------------------
        bool   mbRenderingShadowMap;                              // :226
        bool   mbUseZOnlyRenderingPath;                           // :227

        CgsGraphics::Camera    maShadowMapCamera[KU_NUM_SHADOW_MAPS];      // :229
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
