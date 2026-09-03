#pragma once

// ============================================================================================
// GameSource/Effects/Particles/Native/BrnLionBlendIm3d.h
//
// BrnGraphics::Im3dBlend -- the Lion-blend immediate-mode 3D renderer.
// DWARF (DecFIGS BrnLionBlendIm3d.h:52):
//     struct BrnGraphics::Im3dBlend : public CgsGraphics::Im3dBase<BrnGraphics::LionBlendVertex>
// carrying EIGHT ProgramVariableHandle members and two methods (Construct :58,
// BeginRendering :71). It is the FIRST (and offset-0) member of BrnGraphics::LionBlendRenderer,
// so its size is what places every matrix in that class -- see BrnLionBlendRenderer.h.
//
// --------------------------------------------------------------------------------------------
// THE EIGHT HANDLES ARE PINNED BY THREE INDEPENDENT INSTRUCTION SETS. Every offset below is a
// literal immediate in the X360 image; none is inferred.
//
//   (1) Im3dBlend::Construct @0x8229B260 -- the resolve site. Each handle is the `a3` out-param
//       of renderengine::ProgramBuffer::GetVariableHandleByName, and the NAME is the adjacent
//       string literal, so the offset and the shader variable are witnessed together:
//         +0xC0  asm word 39 (0x8229B358 addi r5,r31,0xC0)  "worldViewProj"       vtx program 0
//         +0xC4  asm word 45 (0x8229B394 addi r5,r31,0xC4)  "colourScale"         vtx program 0
//         +0xC8  asm word 58 (0x8229B3C8 addi r5,r31,0xC8)  "worldViewProj"       vtx program 1
//         +0xCC  asm word 67 (0x8229B3FC addi r5,r31,0xCC)  "colourScale"         vtx program 1
//         +0xD0  asm word 77 (0x8229B438 addi r5,r31,0xD0)  "gOffset"             vtx program 1
//         +0xD4  asm word 86 (0x8229B470 addi r5,r31,0xD4)  "gScale"              vtx program 1
//         +0xD8  asm word 98 (0x8229B4B0 addi r5,r31,0xD8)  "gDepthConversion"    pix program 1
//         +0xDC  asm word 107(0x8229B4E8 addi r5,r31,0xDC)  "gDepthFadeConstants" pix program 1
//       (program 0 == the plain LionBlended pair, program 1 == the LionBlendedZFade pair: the
//        vertex-program table is read at +0x14 / +0x18 and the pixel table at +0x38 == [1].)
//
//   (2) Im3dBlend::BeginRendering @0x82282060 -- the USE site, in the same order:
//       the fog path binds +0xC8/+0xCC/+0xD0/+0xD4/+0xD8/+0xDC, the no-fog path +0xC0/+0xC4.
//
//   (3) LionBlendRenderer::SetCameraData @0x822824F8 -- writes mCameraTransform at +0xE0,
//       i.e. the byte immediately after the last handle. sizeof(Im3dBlend) == 0xE0 EXACTLY,
//       with no slack for an unmodelled member.
//
// CONSOLE BYTE TABLE for Im3dBlend (32-bit pointers). Every row is attested:
//     +0x00  vptr                              ImRenderer<V> (DWARF `_vptr.ImRenderer`)
//     +0x04  ImRendererBase (empty base)        LionBlendRenderer::EndRendering @0x8227E610
//                                               `addi r30, r31, 4` -- the base-cast idiom,
//                                               with the null guard MSVC emits for a
//                                               NON-primary base
//     +0x04..+0x10  mDirectDrawParameters + mDirectDrawBatchIterator (DWARF :302/:303)
//     +0x10  mpVertexDescriptor                 ImRenderer<V>::BeginRendering `lwz r10,0x10`
//     +0x14  mapVertexProgramBuffer[8]          Construct `lwz r11,0x14` / `0x18`
//     +0x34  mapPixelProgramBuffer[8]           Construct `lwz r11,0x38` == [1]
//     +0x54  mi8CurrentProgram                  ImRenderer<V>::BeginRendering `stb ->0x54`
//     +0x58  maWorldViewProjStateHandle[8]      Im3dBase<V> (DWARF CgsIm3d.h:84)
//     +0x80  mCurrentTransform (Matrix44)       Im3dBase<V> (DWARF CgsIm3d.h:85);
//                                               Construct stores four rows at +0x80/90/A0/B0
//     +0xC0..+0xE0  the eight handles below
//     ============ sizeof == 0xE0, 16-aligned, ZERO slack ============
//
// FLAG -- ONE CONSOLE WORD, TWO HOST HOMES (pre-existing, NOT introduced here; recorded so the
// next wave fixes it once instead of adding a third). The committed CgsGraphics::ImRenderer<V>
// (CgsImRenderer.h) carries `u32 maShaderStateBlocks[8]` and `u8 mauTransform[64]`. The DWARF
// says ImRenderer<V> has NEITHER: they are Im3dBase<V>::maWorldViewProjStateHandle[8] (+0x58)
// and Im3dBase<V>::mCurrentTransform (+0x80) -- and for the 2D family they are
// Im2dBase<V>::maHandleOffsetXYZ[8] (+0x58) and Im2dBase<V>::mCurrentTransform (+0xE0, which is
// exactly the "+0xE0" CgsIm2dColTex.cpp records for its transform store, four handle arrays
// further down). Im3dSkidsRenderer names the SAME +0x58 word mWorldViewProjStateHandle. So the
// two ImRenderer<V> members are a per-derived-class array under a generic alias. Three TUs
// (CgsIm2dUntex.cpp, CgsIm2dColTex.cpp, CgsIm3d.cpp) write them by those names, which is why
// this wave did not rename them: that is a four-file change in a shared template header and is
// outside this slice. Nothing here reads them -- Im3dBlend reaches its own handles by name.
// ============================================================================================

#include "types.hpp"
// float32_t / bool8_t -- the DWARF spells BeginRendering's parameters with those names and their
// one home in this tree is the Lion ParticleRender header (the same one BrnLionBlendRenderer.h
// takes them from). Declared there, not re-typedef'd here: a second `typedef float float32_t;`
// is exactly the kind of quiet fork this subsystem has paid for before.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/ParticleRender.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"       // CgsGraphics::Im3dBase<V>
#include "GameSource/Effects/Particles/Native/BrnLionBlendVertex.h"      // BrnGraphics::LionBlendVertex
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // renderengine::ProgramVariableHandle

namespace rw { struct IResourceAllocator; }
namespace renderengine { class TextureState; }

namespace BrnGraphics
{
    struct Im3dBlend : public CgsGraphics::Im3dBase<LionBlendVertex>
    {
        // DWARF BrnLionBlendIm3d.h:58. X360 @0x8229B260. Build the two-program Lion-blend
        // renderer (LionBlended + LionBlendedZFade) and resolve the eight named constants.
        void Construct(rw::IResourceAllocator* lpAllocator);

        // DWARF BrnLionBlendIm3d.h:71. X360 @0x82282060. Start a Lion particle batch: reset the
        // device shadow, bind program 0 (no fog) or program 1 (ZFade), and push the pass
        // constants. See the body for the per-argument derivation.
        void BeginRendering(const Matrix44& arViewProjection,
                            float32_t afColourScale, bool8_t abZFadeEnable,
                            float32_t afZFadeNear, float32_t afZFadeFar,
                            float32_t afDepthRange,
                            float32_t afHalfViewportWidth, float32_t afHalfViewportHeight,
                            renderengine::TextureState* apDepthTextureState);

    protected:
        // DWARF BrnLionBlendIm3d.h:76..85, in declaration order == console offset order.
        renderengine::ProgramVariableHandle mViewProjectionMatrixStateHandle_Normal;  // +0xC0
        renderengine::ProgramVariableHandle mColourScaleStateHandle_Normal;           // +0xC4
        renderengine::ProgramVariableHandle mViewProjectionMatrixStateHandle_ZFade;   // +0xC8
        renderengine::ProgramVariableHandle mColourScaleStateHandle_ZFade;            // +0xCC
        renderengine::ProgramVariableHandle mOffsetStateHandle_ZFade;                 // +0xD0
        renderengine::ProgramVariableHandle mScaleStateHandle_ZFade;                  // +0xD4
        renderengine::ProgramVariableHandle mDepthConversionStateHandle_ZFade;        // +0xD8
        renderengine::ProgramVariableHandle mDepthFadeStateHandle_ZFade;              // +0xDC

        // The layout witness below names these protected members; it is not a second home for
        // any of them.
        friend struct Im3dBlendLayoutCheck;
    };

    // ---- LAYOUT WITNESS ---------------------------------------------------------------------
    // The handle block is LLP64-INVARIANT (eight 4-byte handles), so its internal geometry and
    // its position relative to the END of the class are host layout facts and are checked. The
    // ABSOLUTE console offsets are not host facts (the prefix carries pointers that widen), so
    // they are named in the message, not asserted.
    struct Im3dBlendLayoutCheck
    {
        static_assert(sizeof(renderengine::ProgramVariableHandle) == 4,
                      "ProgramVariableHandle is the 4-byte handle (programbuffer.h)");

#define IM3DBLEND_HANDLE_DELTA(a, b) \
    (offsetof(Im3dBlend, b) - offsetof(Im3dBlend, a))

        static_assert(IM3DBLEND_HANDLE_DELTA(mViewProjectionMatrixStateHandle_Normal,
                                             mColourScaleStateHandle_Normal) == 0xC4 - 0xC0,
                      "Construct asm word 45 (0x8229B394 addi r5,r31,0xC4) -- \"colourScale\" program 0");
        static_assert(IM3DBLEND_HANDLE_DELTA(mColourScaleStateHandle_Normal,
                                             mViewProjectionMatrixStateHandle_ZFade) == 0xC8 - 0xC4,
                      "Construct asm word 58 (0x8229B3C8 addi r5,r31,0xC8) -- \"worldViewProj\" program 1");
        static_assert(IM3DBLEND_HANDLE_DELTA(mViewProjectionMatrixStateHandle_ZFade,
                                             mColourScaleStateHandle_ZFade) == 0xCC - 0xC8,
                      "Construct asm word 67 (0x8229B3FC addi r5,r31,0xCC) -- \"colourScale\" program 1");
        static_assert(IM3DBLEND_HANDLE_DELTA(mColourScaleStateHandle_ZFade,
                                             mOffsetStateHandle_ZFade) == 0xD0 - 0xCC,
                      "Construct asm word 77 (0x8229B438 addi r5,r31,0xD0) -- \"gOffset\"");
        static_assert(IM3DBLEND_HANDLE_DELTA(mOffsetStateHandle_ZFade,
                                             mScaleStateHandle_ZFade) == 0xD4 - 0xD0,
                      "Construct asm word 86 (0x8229B470 addi r5,r31,0xD4) -- \"gScale\"");
        static_assert(IM3DBLEND_HANDLE_DELTA(mScaleStateHandle_ZFade,
                                             mDepthConversionStateHandle_ZFade) == 0xD8 - 0xD4,
                      "Construct asm word 98 (0x8229B4B0 addi r5,r31,0xD8) -- \"gDepthConversion\"");
        static_assert(IM3DBLEND_HANDLE_DELTA(mDepthConversionStateHandle_ZFade,
                                             mDepthFadeStateHandle_ZFade) == 0xDC - 0xD8,
                      "Construct asm word 107 (0x8229B4E8 addi r5,r31,0xDC) -- \"gDepthFadeConstants\"");

        // THE KEYSTONE: the handle block is the LAST thing in Im3dBlend. On the console that is
        // 0xE0 - 0xC0 == 0x20 bytes, and 0xE0 is where SetCameraData @0x822824F8 (asm word 8,
        // `stfs f10, 0xE0(r3)`) starts writing LionBlendRenderer::mCameraTransform. If this ever
        // fails, a member was added to Im3dBlend that the console does not have.
        static_assert(sizeof(Im3dBlend)
                        - offsetof(Im3dBlend, mViewProjectionMatrixStateHandle_Normal) == 0xE0 - 0xC0,
                      "Im3dBlend ends at the console's 0xE0 -- SetCameraData asm word 8 "
                      "(0x82282518 stfs f10, 0xE0(r3)) writes mCameraTransform there");

#undef IM3DBLEND_HANDLE_DELTA
    };
}
