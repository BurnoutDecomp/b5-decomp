#pragma once

// ============================================================================================
// GameSource/Effects/Particles/Native/BrnLionBlendRenderer.h
//
// BrnGraphics::LionBlendRenderer -- the concrete immediate-mode blend renderer that
// BrnParticle::LionParticleRender drives. It OWNS a BrnGraphics::Im3dBlend by value (NOT by
// inheritance -- see below) plus the four camera matrices, and emits the actual particle
// geometry (sprites / quads / tilts).
//
// ============================================================================================
// THE MEMBER LAYOUT (this file's reason to exist). Until 2026-09-04 this class modelled NO
// members at all while three separate bodies wrote at this+224/288/336/352/416 -- which is why
// the three draw halves could not be written: there was nothing to write THROUGH.
//
// DECLARATION AUTHORITY -- DecFIGS DWARF BrnLionBlendRenderer.h:38 gives the member SET and
// ORDER verbatim:
//     struct BrnGraphics::LionBlendRenderer {
//       private:
//         BrnGraphics::Im3dBlend mRenderer;        // :140
//         cMatrix                mCameraTransform; // :142
//         Matrix44Affine         mBackMat;         // :143
//         Matrix44Affine         mViewMat;         // :144
//         Matrix44               mViewProjection;  // :145
//     };
// ⭐ COMPOSITION, NOT INHERITANCE. mRenderer is a MEMBER at offset 0. That is why
// LionBlendRenderer::BeginRendering / ::SetState / ::GetStateLibrary have no bodies of their
// own in the image: each is a forward whose code is byte-identical to the callee's, so the
// X360 linker ICF-folded them onto Im3dBlend::BeginRendering @0x82282060 and
// ImRendererBase::SetState @0x82276D08. A missing ledger row here is a FOLD, not a hole.
//
// CONSOLE BYTE TABLE (32-bit pointers). Every offset is a literal immediate in the image:
//     +0x000  Im3dBlend      mRenderer           sizeof 0xE0 -- see BrnLionBlendIm3d.h for its
//                                                own eight-handle proof; EndRendering
//                                                @0x8227E610 word 7 (`addi r30, r31, 4`)
//                                                reaches its ImRendererBase base at +4
//     +0x0E0  cMatrix        mCameraTransform    SetCameraData @0x822824F8 asm word 8
//                                                (0x82282518 `stfs f10, 0xE0(r3)`) .. word 40
//                                                (0x8228259C `stfs f0, 0x11C(r3)`)
//     +0x120  Matrix44Affine mBackMat            SetCameraData asm word 15
//                                                (0x82282534 `addi r11, r3, 0x120`);
//                                                corroborated by RenderSprites @0x82282608
//                                                (`this + 288`) and its `this + 336` == the
//                                                translation row mBackMat.wa at +0x150
//     +0x160  Matrix44Affine mViewMat            SetCameraData asm word 38
//                                                (0x82282590 `addi r10, r3, 0x160`)
//     +0x1A0  Matrix44       mViewProjection     SetCameraData asm word 22
//                                                (0x8228254C `addi r9, r3, 0x1A0`)
//     ============ sizeof == 0x1E0 (480) ============
//
// ⭐⭐ THE SIZE IS MEASURED, NOT ASSUMED -- the container bounds it exactly. ParticleModule
// embeds this class BY VALUE between two attested siblings (DWARF ParticleModule.h:773/:775):
//     ParticleModule::Prepare @0x8229BEA0 word 55 (0x8229C0D4 `addis r30,r31,1` +
//        0x8229C0DC `addi r30,r30,-0x6D20`)  => &mLionImmediateModeRenderer == this + 0x92E0
//     ParticleModule::Prepare        word 79 (0x8229C13C `ori r11,r11,0x94C0` +
//        0x8229C150 `stwx r27, r31, r11`)    => mSparkRenderer's first word  == this + 0x94C0
//     0x94C0 - 0x92E0 == 0x1E0 == 0xE0 (Im3dBlend) + 4 * 0x40 (the matrices). No slack.
// The same Prepare then stores that pointer into the Lion renderer:
//     0x8229C0F0 `stw r30, 0x53D0(r31)` == mLionRenderer(+0x5270)::mpRenderer(+0x160).
//
// ⚠ THE ABSOLUTE OFFSETS ARE NOT HOST FACTS. mRenderer's prefix carries console 32-bit
// pointers (the vertex descriptor + two 8-entry program tables), which widen on the x64 gate,
// so only the RELATIVE geometry of the matrix block is asserted below -- that part IS
// LLP64-invariant (four 16-byte-row matrices, 0x40 bytes each) and is exactly what the draw
// halves index. Members are reached BY NAME, never by offset.
// ============================================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/ParticleRender.h"
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h"   // cMatrix -- mCameraTransform
#include "rw/math/vpu/types.h"                                  // rw::math::vpu::Matrix44 / Matrix44Affine
#include "GameSource/Effects/Particles/Native/BrnLionBlendIm3d.h"  // BrnGraphics::Im3dBlend (mRenderer, BY VALUE)

// ⚠ THE MATH HOME IS `rw/math/vpu/types.h` (vendor/renderware), NOT
// `SDKs/EATech/include/rw/math/vpu/matrix44.h`. Both spell `rw::math::vpu::Matrix44` and the
// tree carries both; the vendor one is what BrnCommonTypes.h aliases and what
// LionParticleRender.h stores its own mBackMat/mViewMat/mViewProjection as, so it is the type
// that actually reaches SetCameraData. Including the other header here would give this TU a
// DIFFERENT `rw::math::vpu::Matrix44` from its only caller's -- an ODR fork that links
// silently. (The version this file superseded dodged it only by never including either: it
// forward-declared `class Matrix44`, which is also a `class`-vs-`struct` mismatch against the
// real `struct` definition.)

#include <cstddef>   // offsetof -- the layout witness at the foot of this file

namespace renderengine
{
    class DepthStencilState;
    class MaterialState;            // a.k.a. BlendState (renderstates.h)
    class TextureState;
}

namespace BrnGraphics
{
    // BrnGraphics::BlendState alias used by the DWARF SetState overload -- the
    // render-engine material/blend state block.
    typedef renderengine::MaterialState BlendState;

    class LionBlendRenderer
    {
    public:
        // BrnLionBlendRenderer.h:41 -- build the two-program Lion-blend renderer and resolve
        // its eight named shader constants. FOLDED onto Im3dBlend::Construct @0x8229B260 in
        // exactly the way BeginRendering is folded onto Im3dBlend::BeginRendering: mRenderer is
        // the offset-0 member, so the console's ParticleModule::Prepare @0x8229BEA0 (asm word
        // 145, `bl BrnGraphics__Im3dBlend__Construct` with r3 = this+0x92E0 == the
        // LionBlendRenderer itself, r4 = off_82F2C814) reaches the base method through the
        // derived object's own address and the linker keeps one body. The forward IS the whole
        // function, so it is inline here.
        void Construct(rw::IResourceAllocator* lpAllocator)
        {
            mRenderer.Construct(lpAllocator);
        }

        // BrnLionBlendRenderer.h:53 -- begin an immediate-mode batch with the packed
        // view-projection matrix and the depth-fade / viewport parameters, sampling
        // apDepthTextureState. FOLDED onto Im3dBlend::BeginRendering @0x82282060 (see the
        // COMPOSITION note above): the forward IS the whole function, so it is inline here and
        // the real body lives in BrnLionBlendIm3d.cpp.
        //
        // ⚠ The parameter list is recovered from the CALL SITE, not from Hex-Rays (which types
        // the callee `int(int a1 .. int a31)`): LionParticleRender::BeginRendering @0x82289568
        // passes mViewProjection in r4, then f1..f6 for the six floats with r6 carrying the
        // bool8_t between them -- each f32 eats its GPR slot, so the ninth argument (the depth
        // texture state) lands in the stack parameter slot the callee reads at `arg_5C`.
        void BeginRendering(const rw::math::vpu::Matrix44& arViewProjection,
                            float32_t afColourScale, bool8_t abZFadeEnable,
                            float32_t afZFadeNear, float32_t afZFadeFar,
                            float32_t afDepthRange,
                            float32_t afDepthSamplerOffsetU, float32_t afDepthSamplerOffsetV,
                            renderengine::TextureState* apDepthTextureState)
        {
            mRenderer.BeginRendering(arViewProjection, afColourScale, abZFadeEnable,
                                     afZFadeNear, afZFadeFar, afDepthRange,
                                     afDepthSamplerOffsetU, afDepthSamplerOffsetV,
                                     apDepthTextureState);
        }

        // BrnLionBlendRenderer.h:55. X360 @0x8227E610 -- the one method with its own out-of-line
        // body (it is NOT a pure forward: it also clears the module's active-renderer static).
        void EndRendering();

        // BrnLionBlendRenderer.h:74. X360 @0x822824F8 -- load the back / view / view-projection
        // matrices, deriving mCameraTransform from the back matrix.
        //
        // The DWARF declares all three BY VALUE; the X360 ABI passes each 64-byte matrix as a
        // pointer to the caller's own copy (RenderGroupBeginLite @0x822894C8 passes
        // `&mBackMat`, `&mViewMat`, `&mViewProjection` straight out of its own object with no
        // copy made), and this reconstruction keeps that -- const& is the same one-pointer call
        // and does not add three 64-byte host copies the console never makes.
        void SetCameraData(const rw::math::vpu::Matrix44Affine& arBackMat,
                           const rw::math::vpu::Matrix44Affine& arViewMat,
                           const rw::math::vpu::Matrix44& arViewProjection);

        // BrnLionBlendRenderer.h:81 / :86 -- bind a depth-stencil or blend render state on
        // the immediate-mode renderer.
        void SetState(const renderengine::DepthStencilState* apState);
        void SetState(const BlendState* apState);

        // BrnLionBlendRenderer.h:99 / :109 / :119 -- emit the particle geometry for the
        // selected draw shape.
        //
        // ⚠⚠ THESE THREE PROTOTYPES ARE RECOVERED FROM THE CALL SITE, NOT FROM THE CALLEE.
        // Hex-Rays gives all three as `int RenderSprites()` -- ZERO parameters -- because the
        // bodies pull every argument out of saved registers inside an inline-asm region it
        // cannot type. LionParticleRender::Render @0x82289050 sets up the identical six-register
        // argument frame before all three (asm words 25-31 / 32-38 / 39-45):
        //     r3 = mpCurrentRenderer   (`lwz r3, 0x164(r26)`)   the implicit this
        //     r4 = its own r4          arIterator   EffectsVertexBufferIterator&
        //     r5 = its own r5          apParticle   RenderedParticle*
        //     r6 = its own r6          apMatrix     const cMatrix*
        //     r7 = its own r7          auCount      U32   (`cmplwi r30,0` -> early-out at 0)
        //     r8 = its own r9          apEmitter    const cParticleEmitter*
        //     r9 = [sp + arg_54]       arTime       const cTime&
        // which is exactly the DWARF's :99/:109/:119 signature, so the two agree independently.
        void RenderSprites(EffectsVertexBufferIterator& arIterator, RenderedParticle* apParticle,
                           const cMatrix* apMatrix, U32 auCount,
                           const cParticleEmitter* apEmitter, const cTime& arTime);
        void RenderQuads(EffectsVertexBufferIterator& arIterator, RenderedParticle* apParticle,
                         const cMatrix* apMatrix, U32 auCount,
                         const cParticleEmitter* apEmitter, const cTime& arTime);
        void RenderTilts(EffectsVertexBufferIterator& arIterator, RenderedParticle* apParticle,
                         const cMatrix* apMatrix, U32 auCount,
                         const cParticleEmitter* apEmitter, const cTime& arTime);

    private:
        // BrnLionBlendRenderer.h:138 (DWARF) -- build the camera-facing basis the three
        // Render* shapes billboard their geometry against. X360 @0x8227A478.
        // RECONSTRUCTED (BrnLionBlendRenderer.cpp).
        void BuildCameraOrientatedLocator(cMatrix& arOut, const cParticleEmitter* apEmitter,
                                          const cMatrix& arCameraTransform, const cTime& arTime);

        // ---- MEMBERS (DWARF order; see the console byte table at the head of this file) -----
        Im3dBlend                     mRenderer;          // console +0x000, sizeof 0xE0
        cMatrix                       mCameraTransform;   // console +0x0E0
        rw::math::vpu::Matrix44Affine mBackMat;           // console +0x120
        rw::math::vpu::Matrix44Affine mViewMat;           // console +0x160
        rw::math::vpu::Matrix44       mViewProjection;    // console +0x1A0

        friend struct LionBlendRendererLayoutCheck;
    };

    // ---- LAYOUT WITNESS ---------------------------------------------------------------------
    // Asserts only what survives x64 widening: the four matrices are 0x40-byte 16-aligned
    // blocks laid end to end immediately after mRenderer, and nothing follows them. Break any
    // one of these (drop a matrix, reorder two, insert a member) and the build stops here with
    // the asm word that says otherwise.
    struct LionBlendRendererLayoutCheck
    {
        static_assert(sizeof(rw::math::vpu::Matrix44) == 0x40,
                      "Matrix44 is the console's four 16-byte rows");
        static_assert(sizeof(rw::math::vpu::Matrix44Affine) == 0x40,
                      "Matrix44Affine is FOUR rows here, not three -- SetCameraData asm words "
                      "43-58 copy r4/r5 at +0x00/+0x10/+0x20/+0x30 (four lvx128/stvx128 pairs)");
        static_assert(sizeof(cMatrix) == 0x40, "cMatrix is the Lion 4x4 (Matrix.h)");

#define LBR_DELTA(a, b) \
    (offsetof(LionBlendRenderer, b) - offsetof(LionBlendRenderer, a))

        // mCameraTransform starts where mRenderer ends -- the console's 0xE0, with no gap. This
        // is the join the whole class hangs on: Im3dBlend's own witness proves its tail is the
        // eight handles, and this proves the matrix block starts immediately after it.
        static_assert(offsetof(LionBlendRenderer, mCameraTransform) == sizeof(Im3dBlend),
                      "SetCameraData asm word 8 (0x82282518 stfs f10, 0xE0(r3)) == sizeof(Im3dBlend)");

        static_assert(LBR_DELTA(mCameraTransform, mBackMat) == 0x120 - 0x0E0,
                      "SetCameraData asm word 15 (0x82282534 addi r11, r3, 0x120)");
        static_assert(LBR_DELTA(mBackMat, mViewMat) == 0x160 - 0x120,
                      "SetCameraData asm word 38 (0x82282590 addi r10, r3, 0x160)");
        static_assert(LBR_DELTA(mViewMat, mViewProjection) == 0x1A0 - 0x160,
                      "SetCameraData asm word 22 (0x8228254C addi r9, r3, 0x1A0)");

        // The four matrices are the whole tail: nothing follows mViewProjection. On the console
        // this is 0x1E0 - 0xE0 == 0x100, and 0x1E0 is the exact slot ParticleModule reserves
        // between mLionImmediateModeRenderer (+0x92E0) and mSparkRenderer (+0x94C0).
        static_assert(sizeof(LionBlendRenderer) - offsetof(LionBlendRenderer, mCameraTransform)
                        == 0x1E0 - 0x0E0,
                      "ParticleModule::Prepare bounds the object: 0x94C0 (asm word 79, "
                      "stwx r27,r31,r11) - 0x92E0 (asm word 55, addi r30,r30,-0x6D20) == 0x1E0");

        static_assert(alignof(LionBlendRenderer) == 16,
                      "the matrix rows are VMX lane registers (lvx128/stvx128 in SetCameraData)");

#undef LBR_DELTA
    };
}
