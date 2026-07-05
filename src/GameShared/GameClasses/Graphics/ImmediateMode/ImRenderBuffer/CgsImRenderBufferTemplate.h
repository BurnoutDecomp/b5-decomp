#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"                                       // rw::IResourceAllocator / ResourceDescriptor / Resource
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"  // renderengine::PrimitiveType / Texture / TextureState fwd-decls
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"  // CgsGraphics::Basic2dColouredTexturedVertex
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredVertex.h"  // CgsGraphics::BasicColouredVertex
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"  // CgsGraphics::Im2dTransform (the SetTransform command payload)

// =============================================================================
// CgsGraphics::ImRenderBuffer<V> - the immediate-mode command/vertex render buffer.
//
// This is the FAITHFUL templated buffer the X360/PS3 immediate renderers (Im2d /
// Im3d / Im3dUntex) fill and submit. It is a double-buffered (write / dispatch)
// command stream paired with a parallel vertex stream: a producer thread opens a
// BeginRendering block, appends typed ImCommands (set-state / set-texture / draw-
// primitives / set-program / ...) interleaved with vertex runs, closes the block
// with EndRendering, then Swap()s so the consumer ("Dispatch") walks the now-frozen
// dispatch buffer command-by-command and re-issues each command to the GPU device.
//
// LAYOUT + METHOD SET: DecFIGS PS3 DWARF (CgsImRenderBuffer.h) - the authoritative
// member names/order. Console (PS3, 4-byte ptr) byte-offsets are documented as
// [c:0xNN]; reconstructed here x64-native (8-byte ptrs).
// BODIES: PS3 External ELF dossiers for the <Basic2dColouredTexturedVertex>
// instantiation (Prepare @0x1EC844, Clear @0x1EA7D4, Swap @0x1EA834, Construct
// @0x1EA6F0, BeginRendering @0x24DD6C, EndRendering @0x24EF0C, AllocVertices
// @0x24DAE8, Render @0x24EDE0, RenderFromStaticVertexBuffer @0x5CF45C, SetTexture
// @0x24ED54, SetProgram @0x24ECC8, RenderStart @0x57E0A0, RenderEnd @0x57E5DC,
// GetFirstCommand @0x57E024, GetNextCommand @0x57E040, IsInARenderingBlock @0x5CEEA0,
// SetBufferFullRewindToLastEndRender @0x24DD54). Cross-checked vs the X360 ARTIST
// addresses noted in the task.
//
// A SECOND instantiation, <BasicColouredVertex> (the untextured 3D buffer behind
// CgsGraphics::Im3dUntex / Im3dRenderBufferUntex), is X360-ARTIST-attested for a
// subset of members (ledger key `class:CgsGraphics::BasicColouredVertex>`):
// Prepare @0x824045B0, BeginRendering @0x82459650, EndRendering @0x82459738,
// Render @0x82459828, SetState(RasterizerState*) @0x824599C0, SetTransform
// @0x82459CA8, Swap @0x823F9558, SetBufferFullRewindToLastEndRender @0x82449310.
// Every one of these X360 bodies byte-matches this template's PS3-sourced layout
// (mpWriteBuffer @32, mpDispatchBuffer @36, muVertexBufferSize @44,
// muCommandBufferSize @48, mbInRenderBlock @52, muLastEndRenderPos @60,
// mbBufferIsFull @64, mbFailGracefully @65 - console byte offsets), confirming this
// is the SAME ImRenderBuffer<V> template, not a fork. Only the X360-attested members
// are explicitly instantiated for <BasicColouredVertex> (see the .cpp) - the wave-30
// lesson: per-member instantiation, never a blind whole-struct `template struct`.
// (AddProgram / Construct / SetProgram / EndRendering / SetTransform(const void*) for
// `CgsGraphics::BasicColouredVertex>` belong to a DIFFERENT template, ImRenderer<V> -
// see CgsIm3dUntex.cpp - NOT this one; the ledger's flat function-name grouping
// conflated the two instantiations under one TU id.)
//
// The actual GPU submission (ImRenderBuffer<V>::Dispatch / Im2dRenderBuffer::Dispatch
// @0x575BD4) is the PLATFORM LEAF: on the PS3 it writes the RSX push-buffer through
// gCellGcmCurrentContext and shadow::Device, on the X360 the D3D ring. That device
// flush is NOT reconstructed here (see // FLAG in the .cpp); the portable shape -
// the command/vertex storage, the block bookkeeping, the command opcode encoding,
// the rewind-on-full logic, the double-buffer swap, and the command-walk
// (GetFirstCommand/GetNextCommand) the dispatcher drives - IS faithfully decompiled.
// =============================================================================

namespace renderengine
{
    class BlendState;
    class DepthStencilState;
    class RasterizerState;
    class RenderTargetState;
    class SamplerState;
    struct ClearColorParameters;
}

namespace CgsGraphics
{
    // -------------------------------------------------------------------------
    // ImCommand - the base header every command in the stream begins with.
    // DWARF CgsImRenderBuffer.h:67. muType is the opcode (see EImCommandType);
    // muSize is the byte length of THIS command record (header + payload, rounded
    // up to the command alignment) so GetNextCommand can stride to the next record.
    // [c:0x00 muType] [c:0x04 muSize] -- 8 bytes, layout identical on x64.
    // -------------------------------------------------------------------------
    struct ImCommand
    {
        u32 muType;   // [c:0x00] EImCommandType opcode
        u32 muSize;   // [c:0x04] size in bytes of this command record (stride to next)
    };

    // The command opcodes the dispatcher switches on (Im2dRenderBuffer::Dispatch
    // @0x575BD4). Values are the literals the PS3 command writers store into
    // ImCommand::muType (BeginRendering writes 0, EndRendering 1, Render 2, ...).
    enum EImCommandType : u32
    {
        IM_CMD_BEGIN_RENDERING       = 0,   // open a render block (BeginRendering)
        IM_CMD_END_RENDERING         = 1,   // close a render block (EndRendering)
        IM_CMD_RENDER_PRIMITIVES     = 2,   // draw a vertex run (Render / RenderEnd / RenderFromStaticVertexBuffer)
        IM_CMD_SET_STATE_BLEND       = 3,   // SetState(const BlendState*)
        IM_CMD_SET_STATE_DEPTH       = 4,   // SetState(const DepthStencilState*)
        IM_CMD_SET_STATE_DEPTH_REF   = 5,   // SetState(const DepthStencilState*, stencilRef)
        IM_CMD_SET_STATE_RASTERIZER  = 6,   // SetState(const RasterizerState*)
        IM_CMD_SET_STATE_RT          = 7,   // SetState(const RenderTargetState*)
        IM_CMD_SET_STATE_SAMPLER     = 8,   // SetState(const SamplerState*)
        IM_CMD_SET_STATE_TEXTURE     = 9,   // SetState(const TextureState*)
        IM_CMD_SET_TEXTURE           = 10,  // SetTexture(renderengine::Texture*)
        IM_CMD_SET_VIEWPORT          = 11,  // SetViewport(...)
        IM_CMD_SET_SCISSOR           = 12,  // SetScissor(...)
        IM_CMD_SET_CLEAR             = 13,  // SetClear(ClearColorParameters)
        IM_CMD_SET_SHADER_PROGRAM    = 15,  // SetProgram(s8)
        // The Apt rasteriser (AptRenderHandler::Render @0x5CB230 / @0x827D... X360) inlines a
        // command writer that stamps muType 16 with an 80-byte record carrying the 64-byte
        // CgsGraphics::Im2dTransform (the per-batch screen-space + colour transform). The
        // command's payload begins 16 bytes in (header @ +0/+4, an 8-byte pad, the transform
        // @ +16..+79). Emitted by SetTransform() below.
        IM_CMD_SET_TRANSFORM         = 16,  // SetTransform(const Im2dTransform&) -- 80-byte record
        // The Apt mask path (AptCallbackRender::DrawRenderingUnit @0x5CBA30) inlines two more
        // command writers: muType 18 begins a stencil mask from a rendering unit's shape AABB
        // (a 16-byte record carrying the bound texture id @ +8 and a 2-vertex screen-space corner
        // run @ +12), and muType 19 ends/clears the current mask (a 16-byte header-only record).
        // (The Subtract op writes 19; the Add op writes a 18 per mesh.)
        IM_CMD_PUSH_MASK_GEOMETRY    = 18,  // DrawRenderingUnit Add op -- begin a mask from the shape AABB
        IM_CMD_END_MASK              = 19,  // DrawRenderingUnit Subtract op -- end/clear the current mask
    };

    // -------------------------------------------------------------------------
    // The concrete command records, each prefixed by ImCommand. DWARF
    // CgsImRenderBuffer.h:86..216. Payload layout matches the offsets the PS3
    // command writers and the dispatcher use (payload begins at +8, after the
    // 8-byte ImCommand header). Reconstructed x64-native (8-byte payload ptrs;
    // the records are still bump-allocated into the byte command buffer).
    // -------------------------------------------------------------------------

    // RenderPrimitives: a draw of muNumVertices vertices of topology mePrimitiveType
    // from mpVertices. The vertices either live inline in the parallel vertex buffer
    // (Render path) or in a caller-owned static buffer (RenderFromStaticVertexBuffer
    // / RenderEnd). DWARF :86 (templated on V).
    template <typename V>
    struct ImCommandRenderPrimitives : public ImCommand
    {
        renderengine::PrimitiveType mePrimitiveType;  // [c:0x08]
        const V*                    mpVertices;       // [c:0x0C console / +0x10 here]
        u32                         muNumVertices;    // [c:0x10 console / +0x18 here]
    };

    struct ImCommandSetStateBlend : public ImCommand                 // DWARF :95
    {
        const renderengine::BlendState* mpBlendState;                // [c:0x08]
    };

    struct ImCommandSetStateDepthStencil : public ImCommand          // DWARF :102
    {
        const renderengine::DepthStencilState* mpDepthStencilState;  // [c:0x08]
    };

    struct ImCommandSetStateDepthStencilStencilRef : public ImCommand // DWARF :109
    {
        const renderengine::DepthStencilState* mpDepthStencilState;  // [c:0x08]
        const u32                              muStencilRef;         // [c:0x0C console]
    };

    struct ImCommandSetStateRasterizer : public ImCommand            // DWARF :117
    {
        const renderengine::RasterizerState* mpRasterizerState;      // [c:0x08]
    };

    struct ImCommandSetStateRenderTarget : public ImCommand          // DWARF :124
    {
        const renderengine::RenderTargetState* mpRenderTargetState;  // [c:0x08]
    };

    struct ImCommandSetStateTexture : public ImCommand               // DWARF :131
    {
        const TextureState* mpTextureState;                          // [c:0x08]
    };

    struct ImCommandSetStateSampler : public ImCommand               // DWARF :138
    {
        const renderengine::SamplerState* mpSamplerState;            // [c:0x08]
    };

    struct ImCommandSetViewport : public ImCommand                   // DWARF :145
    {
        u32 mu32RenderTarget;  // [c:0x08]
        u32 mu32StartPosX;     // [c:0x0C]
        u32 mu32StartPosY;     // [c:0x10]
        u32 mu32Width;         // [c:0x14]
        u32 mu32Height;        // [c:0x18]
    };

    struct ImCommandSetScissor : public ImCommand                    // DWARF :156
    {
        u32 mu32RenderTarget;  // [c:0x08]
        u32 mu32StartPosX;     // [c:0x0C]
        u32 mu32StartPosY;     // [c:0x10]
        u32 mu32Width;         // [c:0x14]
        u32 mu32Height;        // [c:0x18]
    };

    struct ImCommandSetClear : public ImCommand                      // DWARF :167
    {
        renderengine::ClearColorParameters* mClearParams;            // [c:0x08] (by-value on console; kept a ptr to avoid pulling the heavy param type)
    };

    struct ImCommandSetTexture : public ImCommand                    // DWARF :183
    {
        renderengine::Texture* mpTexture;                            // [c:0x08]
    };

    // PushMask records (DWARF :191/:200): a stencil-mask push carrying the two
    // screen-space corner vertices. Templated on V (the GUI/Flapt mask path).
    template <typename V>
    struct ImCommandPushMaskTextureState : public ImCommand          // DWARF :191
    {
        TextureState* mpTextureState;  // [c:0x08]
        V*            mpVertices;      // [c:0x0C console]
    };

    template <typename V>
    struct ImCommandPushMaskTexture : public ImCommand               // DWARF :200
    {
        renderengine::Texture* mpTexture;   // [c:0x08]
        V*                     mpVertices;  // [c:0x0C console]
    };

    struct ImCommandPopMask : public ImCommand {};                   // DWARF :208 (header only)

    struct ImCommandSetShaderProgram : public ImCommand              // DWARF :214
    {
        s8 mi8ShaderProgram;                                         // [c:0x08]
    };

    // SetTransform: the per-batch screen-space + colour transform the Apt rasteriser stamps
    // ahead of its shape draws (AptRenderHandler::Render @0x5CB230). The PS3 inline writer
    // stores {muType=16, muSize=80} then copies the 64-byte Im2dTransform with four lvx/stvx
    // into the record 16 bytes past its head -- so the header occupies +0/+4, +8..+15 is an
    // unconsumed gap (the 16-byte command alignment the writer rounds to), and the transform
    // lands at +16. Reconstructed with the named transform member at that offset.
    struct ImCommandSetTransform : public ImCommand                  // muType IM_CMD_SET_TRANSFORM
    {
        u32                       mau32Pad[2];   // [c:0x08..0x0F] alignment gap (untouched by the writer)
        CgsGraphics::Im2dTransform mTransform;   // [c:0x10..0x4F] the 64-byte batch transform
    };

    // -------------------------------------------------------------------------
    // ImRenderBuffer<V> - the double-buffered command+vertex stream itself.
    // DWARF CgsImRenderBuffer.h:229. Member order/names authoritative from the
    // DWARF; reconstructed x64-native.
    // -------------------------------------------------------------------------
    template <typename V>
    struct ImRenderBuffer
    {
        // A single (vertex stream + command stream) pair. Two of these are owned;
        // one is the write buffer (producer fills it) and one is the dispatch
        // buffer (consumer drains it); Swap() rotates them. DWARF :232.
        // Console layout (16 bytes): [c:0x00 mpu8VertexBuffer] [c:0x04 mpu8CommandBuffer]
        // [c:0x08 muCommandBufferWritePos] [c:0x0C muVertexBufferWritePos].
        struct SingleBuffer
        {
            u8* mpu8VertexBuffer;          // base of this buffer's vertex stream
            u8* mpu8CommandBuffer;         // base of this buffer's command stream
            u32 muCommandBufferWritePos;   // bytes written into the command stream
            u32 muVertexBufferWritePos;    // bytes written into the vertex stream
        };

        // ---- typed accessors (DWARF declares these aliases inline) ----------
        typedef V VertexType;

        // ----- lifecycle -----
        void Construct();                                                                    // @0x1EA6F0
        bool Prepare(u32 luCommandBufferSizeBytes, u32 luVertexBufferSizeBytes,
                     rw::IResourceAllocator* lpAllocator, bool lbFailGracefully);            // @0x1EC844
        bool Release();
        void Destruct();

        // ----- double-buffer management -----
        void Swap();                                                                         // @0x1EA834
        void Clear();                                                                        // @0x1EA7D4

        // ----- render block bookkeeping -----
        bool IsInARenderingBlock();                                                          // @0x5CEEA0
        void BeginRendering();                                                               // @0x24DD6C
        void EndRendering();                                                                 // @0x24EF0C

        // ----- drawing -----
        void Render(renderengine::PrimitiveType lePrimitiveType,
                    const V* lpVertices, u32 luNumVertices);                                 // @0x24EDE0
        void RenderFromStaticVertexBuffer(renderengine::PrimitiveType lePrimitiveType,
                                          const V* lpVertices, u32 luNumVertices);           // @0x5CF45C
        V*   RenderStart(u32 luNumVertices);                                                 // @0x57E0A0
        void RenderEnd(renderengine::PrimitiveType lePrimitiveType,
                       const V* lpVerticesFromRenderStart, u32 luNumVertices);              // @0x57E5DC

        // ----- state setters (append a command) -----
        void SetTexture(renderengine::Texture* lpTexture);                                   // @0x24ED54
        void SetProgram(s8 li8Program);                                                      // @0x24ECC8
        // Append a 16-byte {type:IM_CMD_SET_STATE_RASTERIZER} command binding a rasterizer
        // state. Decompiled from the <BasicColouredVertex> (Im3dUntex) instantiation
        // (X360 @0x824599C0): asserts lpRasterizerState is non-null and we are inside a
        // BeginRendering/EndRendering block, then the same 16-byte overflow-checked append
        // as SetTexture/SetProgram (payload @ +8 is the state pointer).
        void SetState(const renderengine::RasterizerState* lpRasterizerState);               // @0x824599C0
        // Append a 16-byte {type:IM_CMD_SET_STATE_TEXTURE} command binding a resolved
        // renderengine::TextureState. Decompiled from the inline writer in
        // AptRenderHandler::Render @0x5CB230 (stores muType=9, muSize=16, the state ptr @ +8).
        void SetTextureState(const renderengine::TextureState* lpTextureState);
        // Append an 80-byte {type:IM_CMD_SET_TRANSFORM} command carrying the batch transform.
        // Decompiled from the inline writer in AptRenderHandler::Render @0x5CB230 (stores
        // muType=16, muSize=80, then copies the 64-byte transform 16 bytes into the record).
        void SetTransform(const Im2dTransform& lrTransform);

        // Begin a stencil mask from a shape's two screen-space corner vertices (the Apt mask
        // ADD op). Decompiled from the inline writer in AptCallbackRender::DrawRenderingUnit
        // @0x5CBA30: pre-warm the command line (dcbz, dropped), AllocVertices(2) for the corner
        // run (mask buffer-full rewinds), then a 16-byte {muType=18, muSize=16} record with the
        // bound texture id @ +8 and the corner-run pointer @ +12. Copies the two filled corner
        // vertices into the allocated run. luTextureId is the GuiTexture id (or the white-texture
        // pointer) the console stores at the record's +8 slot.
        void PushMaskGeometry(uintptr_t luTextureId, const V& lrCorner0, const V& lrCorner1);

        // End/clear the current stencil mask (the Apt mask SUBTRACT op). Decompiled from the
        // terminal branch of DrawRenderingUnit @0x5CBA30 (loc_5CBF24): a 16-byte header-only
        // {muType=19, muSize=16} record (overflow rewinds, matching the other writers).
        void EndMask();

        // ----- command-stream walk (the dispatcher drives these) -----
        const ImCommand* GetFirstCommand() const;                                            // @0x57E024
        const ImCommand* GetNextCommand(const ImCommand* lpCurrentCommand) const;            // @0x57E040

        // ----- GPU dispatch (the consumer side) ------------------------------
        // Im2dRenderBuffer::Dispatch @0x575BD4 - walk the frozen DISPATCH buffer command-by-
        // command (GetFirstCommand/GetNextCommand) and re-issue each ImCommand to the graphics
        // device. The PS3/X360 bodies push the RSX/D3D ring through shadow::Device + cellGcm*;
        // the PC realisation (in CgsImRenderBufferTemplate.cpp) translates each EImCommandType
        // opcode into the existing src/pc/gcm/renderengine D3D9 device (renderengine::gDevice)
        // exactly as CgsIm2d.cpp's immediate path does (state -> SetRenderState/SetTexture,
        // RENDER_PRIMITIVES -> DrawPrimitiveUP of the per-batch SET_TRANSFORM-folded vertices).
        void Dispatch();

        // ----- vertex sub-allocation -----
        V* AllocVertices(u32 luNumVertices);                                                 // @0x24DAE8

    private:
        // Buffer-full handler: discard everything written since the last EndRender
        // and latch the "full" flag so the producer stops appending. @0x24DD54.
        void SetBufferFullRewindToLastEndRender();

    public:
        // ---- members (DWARF :424..436) -------------------------------------
        SingleBuffer  maBuffers[2];        // [c:0x00 .. 0x20] the two stream pairs
        SingleBuffer* mpWriteBuffer;       // [c:0x20] producer-side (current write)
        SingleBuffer* mpDispatchBuffer;    // [c:0x24] consumer-side (frozen for dispatch)
        s32           miWriteBufferIndex;  // [c:0x28] which of maBuffers[] is the write buffer
        u32           muVertexBufferSize;  // [c:0x2C] capacity (bytes) of each vertex stream
        u32           muCommandBufferSize; // [c:0x30] capacity (bytes) of each command stream
        bool          mbInRenderBlock;     // [c:0x34] inside a BeginRendering/EndRendering pair
        s32           miNumRendersStarted; // [c:0x38] RenderStart depth counter
        u32           muLastEndRenderPos;  // [c:0x3C] command write-pos at last EndRendering (rewind target)
        bool          mbBufferIsFull;      // [c:0x40] latched once a write overflowed
        bool          mbFailGracefully;    // [c:0x41] true => overflow rewinds instead of asserting
    };

    // The screen-space 2D immediate buffer the Apt/Flapt/GUI rasteriser fills and
    // submits (the <Basic2dColouredTexturedVertex> explicit instantiation - 20-byte
    // pos+RGBA8+UV vertex). This is the family this TU reconstructs.
    typedef ImRenderBuffer<Basic2dColouredTexturedVertex> Im2dColouredTexturedRenderBuffer;

    // The untextured 3D immediate buffer (the <BasicColouredVertex> explicit
    // instantiation - 16-byte pos+RGBA8 vertex) X360-attested for
    // `class:CgsGraphics::BasicColouredVertex>` (see the top-of-file note). NOT aliased
    // to the committed `CgsGraphics::Im3dRenderBufferUntex` name (CgsIm3d.h) - that name
    // is already wired to `Im3dUntex` (the ImRenderer<V>-derived renderer) by
    // BrnRendererModuleIO.h / BrnProgressBarRenderer.cpp's real, already-compiling call
    // sites (`->SetTransform(Matrix44)`, a DIFFERENT overload than this type's
    // SetTransform(const Im2dTransform&)); re-pointing that name would break those
    // callers. This is the buffer's own real type, reachable by writing out the
    // template explicitly: ImRenderBuffer<BasicColouredVertex>.
    typedef ImRenderBuffer<BasicColouredVertex> BasicColouredVertexRenderBuffer;

    // The 3D immediate render-buffer vertex (DWARF Im3dVertex; the SIMD-aligned spelling of
    // BasicColouredTexturedVertex): 16B pos + 4B RGBA8 + 8B UV + pad = 32-byte stride.
    // X360-attested stride from ImRenderBuffer<Im3dVertex>::RenderStart @0x827EF548 (slwi r28,5
    // == 32*luNumVertices). The name is DWARF-attested (CgsIm3d.h / CgsIm3dRenderBuffer.h
    // PushMask signatures). It is DELIBERATELY NOT typedef'd to the committed 24-byte packed
    // CgsGraphics::BasicColouredTexturedVertex -- that packed type is what the ImRenderer<V>
    // path (CgsIm3d.cpp) copies at 24*count, and re-pointing it would break that attested
    // stride. Only the STRIDE (x32) is asm-attested for THIS RenderStart instantiation; the
    // DWARF field split (mv3Pos/mv4Colour/mv2Tex0UV) is not load-bearing for RenderStart (which
    // uses only sizeof(V)), so a correctly-sized opaque 32-byte span is used to avoid pulling
    // the vpu SIMD Vector3 dependency into this header (per the opaque-payload rule).
    struct alignas(16) Im3dVertex
    {
        u8 mau8Opaque[32];   // 32-byte X360-attested vertex stride (DWARF field split not load-bearing here)
    };
    static_assert(sizeof(Im3dVertex) == 32, "Im3dVertex stride is X360-attested at 32 bytes");

    // The 3D-text immediate buffer (Im3dRenderBuffer) the TextRenderer 3D path reserves runs
    // in via RenderStart @0x827EF548 / RenderEnd. Only RenderStart is X360-attested for this
    // instantiation in this wave.
    typedef ImRenderBuffer<Im3dVertex> Im3dTextRenderBuffer;
}
