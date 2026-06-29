// =============================================================================
// CgsGraphics::ImRenderBuffer<V> - method bodies, faithfully decompiled from the
// PS3 External ELF dossiers for the <Basic2dColouredTexturedVertex> instantiation
// (addresses noted per-method). See CgsImRenderBufferTemplate.h for the layout /
// source map. The buffer is reconstructed x64-native; the only platform leaf is
// the GPU dispatch (a separate Dispatch path), which is // FLAG'd below, NOT here.
//
// The PS3 originals interleave a `dcbz` (data-cache block zero) before each command
// append to pre-warm the cache line being written. That is a pure micro-opt with no
// observable effect on the produced command stream, so it is dropped on x64 (there
// is no portable equivalent and it changes nothing the dispatcher reads).
// =============================================================================

#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h"

namespace CgsGraphics
{
    // The vertex stride the PS3 bodies hard-code (20 * luNumVertices): a
    // Basic2dColouredTexturedVertex is exactly 20 bytes (Vector2 + RGBA8 + Vector2).
    // Using sizeof(V) keeps the template honest for the other instantiations.

    // -------------------------------------------------------------------------
    // Construct @0x1EA6F0 - zero the whole instance. The PS3 body clears every
    // word/byte of the fixed layout; on x64 the named zero-init is equivalent.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::Construct()
    {
        maBuffers[0].mpu8VertexBuffer        = nullptr;
        maBuffers[0].mpu8CommandBuffer       = nullptr;
        maBuffers[0].muCommandBufferWritePos = 0;
        maBuffers[0].muVertexBufferWritePos  = 0;
        maBuffers[1].mpu8VertexBuffer        = nullptr;
        maBuffers[1].mpu8CommandBuffer       = nullptr;
        maBuffers[1].muCommandBufferWritePos = 0;
        maBuffers[1].muVertexBufferWritePos  = 0;
        mpWriteBuffer       = nullptr;
        mpDispatchBuffer    = nullptr;
        miWriteBufferIndex  = 0;
        muVertexBufferSize  = 0;
        muCommandBufferSize = 0;
        mbInRenderBlock     = false;
        miNumRendersStarted = 0;
        muLastEndRenderPos  = 0;
        mbBufferIsFull      = false;
        mbFailGracefully    = false;
    }

    // -------------------------------------------------------------------------
    // Prepare @0x1EC844 - allocate the two command + two vertex streams from the
    // RenderWare resource allocator, then prime the double buffer (Clear/Swap x2
    // so both halves start empty and mpWriteBuffer/mpDispatchBuffer are valid).
    //
    // The PS3 body builds a 2-entry rw::ResourceDescriptor ({size, 128-byte align})
    // and calls the allocator vtable slot (here the faithful rw::IResourceAllocator
    // ::DoAllocate(const ResourceDescriptor&, const char*) -> rw::Resource whose
    // m_baseResources[0] is the allocated block). It over-allocates the command
    // buffers by 128 bytes (the PS3 dcbz guard band); we keep that faithfully.
    // -------------------------------------------------------------------------
    template <typename V>
    bool ImRenderBuffer<V>::Prepare(u32 luCommandBufferSizeBytes,
                                    u32 luVertexBufferSizeBytes,
                                    rw::IResourceAllocator* lpAllocator,
                                    bool lbFailGracefully)
    {
        mbFailGracefully    = lbFailGracefully;
        muCommandBufferSize = luCommandBufferSizeBytes;
        muVertexBufferSize  = luVertexBufferSizeBytes;

        // 128-byte aligned over-allocation (the PS3 cache-line guard band of +128).
        const u32 luCommandAllocBytes = luCommandBufferSizeBytes + 128u;
        const u32 luVertexAllocBytes  = luVertexBufferSizeBytes + 128u;

        // Helper mirroring the PS3 descriptor build + allocator call. The descriptor
        // is {size, alignment=128} in entry 0 (the PS3 body fills entry 1 = 128 too).
        auto lAllocate = [lpAllocator](u32 luBytes) -> u8*
        {
            rw::ResourceDescriptor lDescriptor;
            lDescriptor.m_baseResourceDescriptors[0].m_size      = luBytes;
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = 128u;
            lDescriptor.m_baseResourceDescriptors[1].m_size      = luBytes;
            lDescriptor.m_baseResourceDescriptors[1].m_alignment = 128u;
            lDescriptor.m_baseResourceDescriptors[2].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[2].m_alignment = 1u;
            lDescriptor.m_baseResourceDescriptors[3].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[3].m_alignment = 1u;
            rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, nullptr);
            return static_cast<u8*>(lResource.m_baseResources[0]);
        };

        maBuffers[0].mpu8CommandBuffer = lAllocate(luCommandAllocBytes);
        maBuffers[0].mpu8VertexBuffer  = lAllocate(luVertexAllocBytes);
        maBuffers[1].mpu8CommandBuffer = lAllocate(luCommandAllocBytes);
        maBuffers[1].mpu8VertexBuffer  = lAllocate(luVertexAllocBytes);

        // The PS3 body writes a 0 type into the head of every freshly-allocated
        // command/vertex buffer (an empty terminator the first GetFirstCommand sees).
        if (maBuffers[0].mpu8CommandBuffer) maBuffers[0].mpu8CommandBuffer[0] = 0;
        if (maBuffers[0].mpu8VertexBuffer)  maBuffers[0].mpu8VertexBuffer[0]  = 0;
        if (maBuffers[1].mpu8CommandBuffer) maBuffers[1].mpu8CommandBuffer[0] = 0;
        if (maBuffers[1].mpu8VertexBuffer)  maBuffers[1].mpu8VertexBuffer[0]  = 0;

        miWriteBufferIndex = 0;
        mpDispatchBuffer   = &maBuffers[1];   // PS3: this + 16  == &maBuffers[1]
        mpWriteBuffer      = &maBuffers[0];   // PS3: this       == &maBuffers[0]

        Clear();
        Swap();
        Clear();
        Swap();
        return true;
    }

    // -------------------------------------------------------------------------
    // Clear @0x1EA7D4 - reset the WRITE buffer's stream positions and the per-frame
    // block/full/last-end-render flags. (mpWriteBuffer is this+32 on PS3.)
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::Clear()
    {
        mbInRenderBlock = false;                            // this+52
        mpWriteBuffer->muVertexBufferWritePos  = 0;         // (*this+32)+12
        mpWriteBuffer->muCommandBufferWritePos = 0;         // (*this+32)+8
        mbBufferIsFull     = false;                         // this+64
        muLastEndRenderPos = 0;                             // this+60
    }

    // -------------------------------------------------------------------------
    // Swap @0x1EA834 - rotate write<->dispatch buffers (the producer just finished
    // a frame; the consumer will dispatch what was written). Resets the new write
    // buffer's per-frame flags. (PS3: miWriteBufferIndex = 1 - miWriteBufferIndex;
    // mpWriteBuffer = this + 16*index; mpDispatchBuffer = this + 16*(1-index).)
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::Swap()
    {
        const s32 liNewIndex = 1 - miWriteBufferIndex;
        muLastEndRenderPos  = 0;       // this+60
        mbBufferIsFull      = false;   // this+64
        mbInRenderBlock     = false;   // this+52
        miWriteBufferIndex  = liNewIndex;
        mpWriteBuffer       = &maBuffers[liNewIndex];
        mpDispatchBuffer    = &maBuffers[1 - liNewIndex];
    }

    // -------------------------------------------------------------------------
    // IsInARenderingBlock @0x5CEEA0 - one byte read.
    // -------------------------------------------------------------------------
    template <typename V>
    bool ImRenderBuffer<V>::IsInARenderingBlock()
    {
        return mbInRenderBlock;
    }

    // -------------------------------------------------------------------------
    // AllocVertices @0x24DAE8 - bump-allocate luNumVertices from the write buffer's
    // vertex stream; returns the run pointer, or nullptr if it would overflow.
    // -------------------------------------------------------------------------
    template <typename V>
    V* ImRenderBuffer<V>::AllocVertices(u32 luNumVertices)
    {
        const u32 luOldPos  = mpWriteBuffer->muVertexBufferWritePos;          // (*this+32)+0xC
        const u32 luNewPos  = static_cast<u32>(sizeof(V)) * luNumVertices + luOldPos;
        if (muVertexBufferSize >= luNewPos)                                   // this+44
        {
            u8* lpBase = mpWriteBuffer->mpu8VertexBuffer;                     // *(*this+32)
            mpWriteBuffer->muVertexBufferWritePos = luNewPos;
            return reinterpret_cast<V*>(lpBase + luOldPos);
        }
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // BeginRendering @0x24DD6C - latch "in block" and append a 16-byte
    // {type:IM_CMD_BEGIN_RENDERING} command (or rewind if the command buffer is
    // full). The PS3 store writes a QWORD 16 at the command head (==> muType=0,
    // muSize=16 on big-endian); reconstructed here as the named fields.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::BeginRendering()
    {
        mbInRenderBlock = true;                                               // this+52
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;            // (*this+32)+8
        if (muCommandBufferSize < luPos + 16u)                               // this+48
        {
            SetBufferFullRewindToLastEndRender();
        }
        else
        {
            ImCommand* lpCommand = reinterpret_cast<ImCommand*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muType = IM_CMD_BEGIN_RENDERING;
            lpCommand->muSize = 16u;
            mpWriteBuffer->muCommandBufferWritePos = luPos + 16u;
        }
    }

    // -------------------------------------------------------------------------
    // EndRendering @0x24EF0C - clear "in block", append a 16-byte
    // {type:IM_CMD_END_RENDERING} command, and remember this command position as
    // the rewind target (muLastEndRenderPos).
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::EndRendering()
    {
        mbInRenderBlock = false;                                              // this+52
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize < luPos + 16u)
        {
            SetBufferFullRewindToLastEndRender();
        }
        else
        {
            ImCommand* lpCommand = reinterpret_cast<ImCommand*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muType = IM_CMD_END_RENDERING;
            lpCommand->muSize = 16u;
            mpWriteBuffer->muCommandBufferWritePos = luPos + 16u;
            muLastEndRenderPos = luPos;                                       // this+60
        }
    }

    // -------------------------------------------------------------------------
    // Render @0x24EDE0 - copy a vertex run into the write buffer and append a
    // 32-byte {type:IM_CMD_RENDER_PRIMITIVES} command pointing at the copy. A no-op
    // once the buffer is full (mbBufferIsFull latched). AllocVertices failure or a
    // command-buffer overflow both rewind.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::Render(renderengine::PrimitiveType lePrimitiveType,
                                   const V* lpVertices, u32 luNumVertices)
    {
        if (mbBufferIsFull)                                                   // this+64
            return;

        V* lpDest = AllocVertices(luNumVertices);
        if (!lpDest)
        {
            SetBufferFullRewindToLastEndRender();
            return;
        }

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 32u)
        {
            ImCommandRenderPrimitives<V>* lpCommand =
                reinterpret_cast<ImCommandRenderPrimitives<V>*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muType         = IM_CMD_RENDER_PRIMITIVES;
            lpCommand->muSize         = 32u;
            mpWriteBuffer->muCommandBufferWritePos = luPos + 32u;
            lpCommand->mePrimitiveType = lePrimitiveType;
            lpCommand->mpVertices      = lpDest;
            lpCommand->muNumVertices   = luNumVertices;

            // PS3 sub_132B0(dest, src, 20 * num): the inline vertex copy.
            for (u32 luIndex = 0; luIndex < luNumVertices; ++luIndex)
                lpDest[luIndex] = lpVertices[luIndex];
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // RenderFromStaticVertexBuffer @0x5CF45C - append a 32-byte render command that
    // points straight at a CALLER-owned static vertex buffer (no copy into the
    // write buffer). Used when the vertices already live in a persistent buffer.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::RenderFromStaticVertexBuffer(renderengine::PrimitiveType lePrimitiveType,
                                                         const V* lpVertices, u32 luNumVertices)
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 32u)
        {
            ImCommandRenderPrimitives<V>* lpCommand =
                reinterpret_cast<ImCommandRenderPrimitives<V>*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize          = 32u;
            lpCommand->muType          = IM_CMD_RENDER_PRIMITIVES;
            mpWriteBuffer->muCommandBufferWritePos = luPos + 32u;
            lpCommand->mpVertices      = lpVertices;
            lpCommand->mePrimitiveType = lePrimitiveType;
            lpCommand->muNumVertices   = luNumVertices;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // RenderStart @0x57E0A0 - reserve luNumVertices in the vertex stream and return
    // the write pointer for the caller to fill (paired with RenderEnd). The PS3
    // forwards through a thunk to AllocVertices.
    // -------------------------------------------------------------------------
    template <typename V>
    V* ImRenderBuffer<V>::RenderStart(u32 luNumVertices)
    {
        return AllocVertices(luNumVertices);
    }

    // -------------------------------------------------------------------------
    // RenderEnd @0x57E5DC - close a RenderStart run: append a 32-byte render command
    // that points at the run the caller just filled in place (the run is already in
    // the vertex stream, so no copy here - unlike Render).
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::RenderEnd(renderengine::PrimitiveType lePrimitiveType,
                                      const V* lpVerticesFromRenderStart, u32 luNumVertices)
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 32u)
        {
            ImCommandRenderPrimitives<V>* lpCommand =
                reinterpret_cast<ImCommandRenderPrimitives<V>*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize          = 32u;
            lpCommand->muType          = IM_CMD_RENDER_PRIMITIVES;
            mpWriteBuffer->muCommandBufferWritePos = luPos + 32u;
            lpCommand->mpVertices      = lpVerticesFromRenderStart;
            lpCommand->mePrimitiveType = lePrimitiveType;
            lpCommand->muNumVertices   = luNumVertices;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // SetTexture @0x24ED54 - append a 16-byte {type:IM_CMD_SET_TEXTURE} command
    // carrying the bound texture pointer.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::SetTexture(renderengine::Texture* lpTexture)
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 16u)
        {
            ImCommandSetTexture* lpCommand = reinterpret_cast<ImCommandSetTexture*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize    = 16u;
            lpCommand->muType    = IM_CMD_SET_TEXTURE;
            mpWriteBuffer->muCommandBufferWritePos = luPos + 16u;
            lpCommand->mpTexture = lpTexture;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // SetProgram @0x24ECC8 - append a 16-byte {type:IM_CMD_SET_SHADER_PROGRAM}
    // command carrying the byte program id.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::SetProgram(s8 li8Program)
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 16u)
        {
            ImCommandSetShaderProgram* lpCommand =
                reinterpret_cast<ImCommandSetShaderProgram*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize          = 16u;
            lpCommand->muType          = IM_CMD_SET_SHADER_PROGRAM;
            mpWriteBuffer->muCommandBufferWritePos = luPos + 16u;
            lpCommand->mi8ShaderProgram = li8Program;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // SetTextureState - append a 16-byte {type:IM_CMD_SET_STATE_TEXTURE} command
    // binding a resolved renderengine::TextureState. Decompiled from the inline
    // writer in AptRenderHandler::Render @0x5CB230 (LABEL_19): it stores muType=9,
    // muSize=16, advances the write position, then writes the state pointer at +8
    // (the ImCommandSetStateTexture::mpTextureState payload slot). Same overflow
    // arm as the other 16-byte command writers.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::SetTextureState(const renderengine::TextureState* lpTextureState)
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 16u)
        {
            ImCommandSetStateTexture* lpCommand = reinterpret_cast<ImCommandSetStateTexture*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muType         = IM_CMD_SET_STATE_TEXTURE;            // 9
            lpCommand->muSize         = 16u;
            mpWriteBuffer->muCommandBufferWritePos = luPos + 16u;
            lpCommand->mpTextureState = reinterpret_cast<const TextureState*>(lpTextureState);
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // SetTransform - append an 80-byte {type:IM_CMD_SET_TRANSFORM} command carrying
    // the batch transform. Decompiled from the inline writer at the head of
    // AptRenderHandler::Render @0x5CB230: it stores muType=16, muSize=80, advances
    // the write position, then copies the 64-byte CgsGraphics::Im2dTransform into the
    // record 16 bytes past its head (the four lvx/stvx). The PS3 `dcbz` cache-line
    // pre-warm before the copy is dropped (no observable effect; see file header).
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::SetTransform(const Im2dTransform& lrTransform)
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 80u)
        {
            ImCommandSetTransform* lpCommand = reinterpret_cast<ImCommandSetTransform*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = 80u;
            lpCommand->muType = IM_CMD_SET_TRANSFORM;                        // 16
            mpWriteBuffer->muCommandBufferWritePos = luPos + 80u;
            lpCommand->mTransform = lrTransform;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // GetFirstCommand @0x57E024 - the dispatcher's iterator start: the first
    // command in the DISPATCH buffer, or nullptr if it is empty. (PS3 reads
    // mpDispatchBuffer == this+36; returns its command base iff write-pos != 0.)
    // -------------------------------------------------------------------------
    template <typename V>
    const ImCommand* ImRenderBuffer<V>::GetFirstCommand() const
    {
        const SingleBuffer* lpDispatch = mpDispatchBuffer;                    // this+36
        if (lpDispatch->muCommandBufferWritePos)
            return reinterpret_cast<const ImCommand*>(lpDispatch->mpu8CommandBuffer);
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // GetNextCommand @0x57E040 - stride past lpCurrentCommand by its muSize; returns
    // the next command, or nullptr once the dispatch buffer's written range is
    // exhausted. (PS3: offset = (cur + cur->muSize) - cmdBase; valid iff < writePos.)
    // -------------------------------------------------------------------------
    template <typename V>
    const ImCommand* ImRenderBuffer<V>::GetNextCommand(const ImCommand* lpCurrentCommand) const
    {
        const SingleBuffer* lpDispatch = mpDispatchBuffer;                    // this+36
        const u8* lpCmdBase = lpDispatch->mpu8CommandBuffer;                  // +4
        const u8* lpNext = reinterpret_cast<const u8*>(lpCurrentCommand) + lpCurrentCommand->muSize;
        const u32 luNextOffset = static_cast<u32>(lpNext - lpCmdBase);
        if (lpDispatch->muCommandBufferWritePos > luNextOffset)              // +8
            return reinterpret_cast<const ImCommand*>(lpNext);
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // SetBufferFullRewindToLastEndRender @0x24DD54 - latch "full" and rewind the
    // write buffer's command position back to the last EndRendering so the
    // already-committed batch stays intact and the overflowing tail is discarded.
    // (The PS3 original also emits a CgsDev::Message diagnostic on overflow; that
    // logging path is omitted here - it has no effect on the produced stream.)
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::SetBufferFullRewindToLastEndRender()
    {
        const u32 luRewindTo = muLastEndRenderPos;                            // this+60
        mbBufferIsFull = true;                                                // this+64
        mpWriteBuffer->muCommandBufferWritePos = luRewindTo;                  // (*this+32)+8
    }

    // -------------------------------------------------------------------------
    // Release / Destruct - the resource hand-back. The matching allocator-free path
    // is the inverse of Prepare's DoAllocate; the concrete free is owned by the
    // RenderWare allocator that handed out the blocks (rw::IResourceAllocator's
    // DoFree* slots), reached through the same interface. Modelled as a no-op shell
    // here so the lifecycle compiles; the bring-up never tears these down.
    // FLAG: allocator-free leaf deferred (the rw IResourceAllocator free slot is the
    //       owner's; not part of the portable buffer logic this TU reconstructs).
    // -------------------------------------------------------------------------
    template <typename V>
    bool ImRenderBuffer<V>::Release()
    {
        maBuffers[0].mpu8CommandBuffer = nullptr;
        maBuffers[0].mpu8VertexBuffer  = nullptr;
        maBuffers[1].mpu8CommandBuffer = nullptr;
        maBuffers[1].mpu8VertexBuffer  = nullptr;
        mpWriteBuffer    = nullptr;
        mpDispatchBuffer = nullptr;
        return true;
    }

    template <typename V>
    void ImRenderBuffer<V>::Destruct()
    {
        Release();
    }

    // =========================================================================
    // GPU DISPATCH (the platform leaf).
    //
    // The consumer side - Im2dRenderBuffer::Dispatch @0x575BD4 - walks the dispatch
    // buffer with GetFirstCommand/GetNextCommand and re-issues each ImCommand to the
    // graphics device. That re-issue is PURELY platform GPU-submission: the PS3 body
    // writes the RSX push-buffer through gCellGcmCurrentContext + shadow::Device
    // (cellGcmSetVertexProgramParameterBlockInline, the DirectDraw segment ring, the
    // dcbz/stvx vertex packing, etc.); the X360 equivalent writes the D3D ring.
    //
    // FLAG: ImRenderBuffer<V>::Dispatch is a DECLARED-BUT-DEFERRED platform leaf.
    //       It is NOT reconstructed here - the device draw-call / push-buffer write
    //       has no PC-portable shape and must be supplied by the PC render backend
    //       (the same place CgsIm2d.cpp's DrawPrimitiveUP lives). The portable
    //       command-stream PRODUCER above (append/encode/rewind/swap/walk) is the
    //       half this TU faithfully decompiles; the command opcode table
    //       (EImCommandType) is the contract the PC dispatcher consumes.
    // =========================================================================

    // -------------------------------------------------------------------------
    // Explicit instantiation: the screen-space 2D buffer the Apt/Flapt/GUI/text
    // rasteriser fills and submits (the family this TU targets). This is the
    // <Basic2dColouredTexturedVertex> instance whose bodies the dossiers above
    // were read from.
    // -------------------------------------------------------------------------
    template struct ImRenderBuffer<Basic2dColouredTexturedVertex>;
}
