// =============================================================================
// CgsGraphics::Im2dRenderBuffer command writers (ledger key
// class:CgsGraphics::Im2dRenderBuffer) -- the four clip-mask / boost-bar /
// static-batch command writers the X360 Im2dRenderBuffer (the
// <Basic2dColouredTexturedVertex> instantiation of ImRenderBuffer<V>) appends to
// its open command stream. Faithfully decompiled from BURNOUT_X360_ARTIST.XEX:
//
//   Im2dRenderBuffer::PushMask                             @0x82450030
//   Im2dRenderBuffer::PopMask                              @0x824501C8
//   Im2dRenderBuffer::PushBoostBarColours                  @0x824502A8
//   Im2dRenderBuffer::BatchTransformTextureBlendRenderStatic @0x8246F7D0
//
// (The fifth X360 Im2dRenderBuffer command writer, SetTransform @0x8244FF30, is
// byte-identical to ImRenderBuffer<V>::SetTransform already defined in
// CgsImRenderBufferTemplate.cpp -- the opcode-16 / 80-byte transform append -- so
// it is NOT re-homed here; it is the same symbol on the folded PC target.)
//
// The X360 asm proves these operate on THIS buffer's layout: every one derives the
// command-line base as (r3 + 4) + 0x20 == mpWriteBuffer, reads muCommandBufferSize
// @+0x30, mbInRenderBlock @+0x34, mbFailGracefully @+0x41, muVertexBufferSize
// @+0x2C -- the exact ImRenderBuffer<V> member set. They are appended with the SAME
// overflow-checked bump-append the base writers use (SetTexture/SetProgram/
// SetTransform above): if the record fits, stamp {muType,muSize} + payload and
// advance muCommandBufferWritePos; otherwise SetBufferFullRewindToLastEndRender().
//
// Per this template's established convention (see CgsImRenderBufferTemplate.cpp
// header): the X360 `dcbz128` command-line cache pre-warm is dropped (a pure
// micro-opt with no effect on the produced stream), and the diagnostic-only
// "Command called outside of a BeginRendering/EndRendering block" /
// "ImRenderBuffer command buffer is full" / "run out of vertex buffer space"
// asserts are dropped (they hold whenever the caller is correct and have no effect
// on the produced command stream -- exactly as BeginRendering/EndRendering/SetState
// already drop them). Every constant (opcode, record size, vertex count, payload
// offset) is taken straight from the asm immediates; none is fabricated.
//
// No new instantiation of the whole template is made here (that lives in
// CgsImRenderBufferTemplate.cpp for <Basic2dColouredTexturedVertex>); only the four
// members THIS TU owns are explicitly instantiated, PER MEMBER (the wave-30 lesson).
// =============================================================================

#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h"

namespace CgsGraphics
{
    // -------------------------------------------------------------------------
    // PushMask @0x82450030 - push a clip mask built from a 2-vertex screen-space
    // corner run bound to lpTexture. The X360 body appends the 16-byte {muType=17}
    // command FIRST (or rewinds on command-buffer overflow, leaving the command slot
    // null), then carves the 2-vertex (40-byte) corner run from the vertex stream
    // (the inline equivalent of AllocVertices(2); a vertex overflow leaves the run
    // null), and only when BOTH the command slot and the run are valid stamps the
    // texture @ +8, the run pointer @ +12, and copies the two corners into the run.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::PushMask(renderengine::Texture* lpTexture, const V* lpaMaskVertices)
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        ImCommandPushMaskTexture<V>* lpCommand = nullptr;
        if (muCommandBufferSize >= luPos + 16u)
        {
            lpCommand = reinterpret_cast<ImCommandPushMaskTexture<V>*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = 16u;
            lpCommand->muType = IM_CMD_PUSH_MASK;                             // 17
            mpWriteBuffer->muCommandBufferWritePos = luPos + 16u;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }

        // The 2-vertex corner run (X360: an inline carve of 40 == 2*sizeof(V) bytes from the
        // vertex stream; AllocVertices(2) is the identical size-checked bump-allocate).
        V* lpRun = AllocVertices(2u);

        // Only stamp the payload + copy the corners when both the command slot and the run
        // carved successfully (the guest's `if (v9 && v10)` guard).
        if (lpCommand != nullptr && lpRun != nullptr)
        {
            lpCommand->mpTexture  = lpTexture;
            lpCommand->mpVertices = lpRun;
            lpRun[0] = lpaMaskVertices[0];   // min corner {pos, colour, uv}
            lpRun[1] = lpaMaskVertices[1];   // max corner {pos, colour, uv}
        }
    }

    // -------------------------------------------------------------------------
    // PopMask @0x824501C8 - pop/clear the current clip mask: a 16-byte header-only
    // {muType=19, muSize=16} record (overflow rewinds like the other writers). Same
    // opcode + record as EndMask (a different X360 call site emitting the same op).
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::PopMask()
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 16u)
        {
            ImCommand* lpCommand = reinterpret_cast<ImCommand*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = 16u;
            lpCommand->muType = IM_CMD_END_MASK;                             // 19 (== pop/clear mask)
            mpWriteBuffer->muCommandBufferWritePos = luPos + 16u;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // PushBoostBarColours @0x824502A8 - push the boost-bar colour pair: a 48-byte
    // {muType=21, muSize=48} record carrying the two 16-byte colour vectors (passed
    // in v1/v2) at +16 and +32 (the two stvx128). Overflow rewinds.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::PushBoostBarColours(const rw::math::vpu::Vector4& lrColour0,
                                                const rw::math::vpu::Vector4& lrColour1)
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 48u)
        {
            ImCommandPushBoostBarColours* lpCommand = reinterpret_cast<ImCommandPushBoostBarColours*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = 48u;
            lpCommand->muType = IM_CMD_PUSH_BOOST_BAR_COLOURS;               // 21
            mpWriteBuffer->muCommandBufferWritePos = luPos + 48u;
            lpCommand->maColours[0] = lrColour0;   // v1 -> +16
            lpCommand->maColours[1] = lrColour1;   // v2 -> +32
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // BatchTransformTextureBlendRenderStatic @0x8246F7D0 - append a "set-transform +
    // bound texture/blend + draw a static vertex run" batch: a 112-byte {muType=22,
    // muSize=112} record. The X360 body copies the 64-byte Im2dTransform into the
    // record at +16 (four lvx/stvx, same as SetTransform), then stores the six
    // trailing args -- texture (a3) @ +80, blend state (a4) @ +84, primitive type
    // (a5) @ +88, static vertices (a6) @ +92, vertex count (a7) @ +96, flag byte
    // (a8) @ +100. Reconstructed by name (the trailing pointers widen on x64).
    // Overflow rewinds. The lvx128/stvx128 are pure 16-byte payload copies (no VMX
    // math) -- fully paraphrased, nothing to FLAG.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::BatchTransformTextureBlendRenderStatic(
        const Im2dTransform& lrTransform,
        renderengine::Texture* lpTexture,
        const renderengine::BlendState* lpBlendState,
        renderengine::PrimitiveType lePrimitiveType,
        const V* lpVertices,
        u32 luNumVertices,
        u8 lu8Flags)
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 112u)
        {
            ImCommandBatchTransformTextureBlendRender<V>* lpCommand =
                reinterpret_cast<ImCommandBatchTransformTextureBlendRender<V>*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muType = IM_CMD_BATCH_TRANSFORM_TEXTURE_BLEND_RENDER;  // 22
            lpCommand->muSize = 112u;
            mpWriteBuffer->muCommandBufferWritePos = luPos + 112u;
            lpCommand->mTransform      = lrTransform;      // -> +16 (64-byte copy)
            lpCommand->mpTexture       = lpTexture;        // a3 -> +80
            lpCommand->mpBlendState    = lpBlendState;     // a4 -> +84
            lpCommand->mePrimitiveType = lePrimitiveType;  // a5 -> +88
            lpCommand->mpVertices      = lpVertices;       // a6 -> +92
            lpCommand->muNumVertices   = luNumVertices;    // a7 -> +96
            lpCommand->mu8Flags        = lu8Flags;         // a8 -> +100
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // Explicit instantiation (PER MEMBER) for the screen-space 2D buffer the Apt/
    // Flapt/GUI rasteriser fills -- the <Basic2dColouredTexturedVertex> instance the
    // X360 Im2dRenderBuffer bodies above were read from. Only the four members THIS
    // TU owns are instantiated (the whole-struct instantiation for this vertex type
    // lives in CgsImRenderBufferTemplate.cpp and does not cover these new members).
    // -------------------------------------------------------------------------
    template void ImRenderBuffer<Basic2dColouredTexturedVertex>::PushMask(
        renderengine::Texture*, const Basic2dColouredTexturedVertex*);
    template void ImRenderBuffer<Basic2dColouredTexturedVertex>::PopMask();
    template void ImRenderBuffer<Basic2dColouredTexturedVertex>::PushBoostBarColours(
        const rw::math::vpu::Vector4&, const rw::math::vpu::Vector4&);
    template void ImRenderBuffer<Basic2dColouredTexturedVertex>::BatchTransformTextureBlendRenderStatic(
        const Im2dTransform&, renderengine::Texture*, const renderengine::BlendState*,
        renderengine::PrimitiveType, const Basic2dColouredTexturedVertex*, u32, u8);
}
