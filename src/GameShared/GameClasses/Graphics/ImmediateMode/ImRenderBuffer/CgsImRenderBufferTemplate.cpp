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

// The PC D3D9 device headers MUST be included before the template header: the template
// header transitively pulls rw/core/debug/DebugCriticalSection.h, which defines NOUSER/
// NOGDI/WIN32_LEAN_AND_MEAN ahead of its own <windows.h> (to keep USER/GDI macros from
// leaking). That NOUSER drop strips winuser (LPMSG), which <d3d9.h> needs - so the full
// <Windows.h> + <d3d9.h> must be brought in first, before that guarded include runs.
#include <Windows.h>                            // full Win32 (LPMSG / winuser) for <d3d9.h>
#include <d3d9.h>

#include "pc/gcm/renderengine/device.h"        // renderengine::gDevice, gDisplayWidth/Height (the existing PC D3D9 device)
#include "pc/gcm/renderengine/texture.h"        // renderengine::Texture::mpD3DTexture
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::TextureState::mpRaster

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
    // PushMaskGeometry - the Apt mask ADD op (AptCallbackRender::DrawRenderingUnit @0x5CBA30,
    // per-mesh). Reserve the 2-vertex screen-space corner run (AllocVertices(2)), append a
    // 16-byte {muType=18, muSize=16} record carrying the bound texture id @ +8 and the corner-run
    // pointer @ +12, then copy the two filled corners into the run. The PS3 stores the command
    // header FIRST (and pre-warms the line with dcbz, dropped here), then calls AllocVertices, then
    // -- only when BOTH the run and the command slot are valid -- writes the +8/+12 payload and the
    // two corner vertices. A command-buffer overflow (no room for the 16-byte record) rewinds.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::PushMaskGeometry(uintptr_t luTextureId, const V& lrCorner0,
                                             const V& lrCorner1)
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        ImCommandPushMaskTexture<V>* lpCommand = nullptr;
        if (muCommandBufferSize >= luPos + 16u)
        {
            lpCommand = reinterpret_cast<ImCommandPushMaskTexture<V>*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = 16u;
            lpCommand->muType = IM_CMD_PUSH_MASK_GEOMETRY;                    // 18
            mpWriteBuffer->muCommandBufferWritePos = luPos + 16u;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }

        // The corner run is bump-allocated from the vertex stream (the guest's sub_4F6EE4(this,2)).
        V* lpRun = AllocVertices(2u);

        // Only stamp the payload when both the command slot and the vertex run carved successfully
        // (the guest's `if (v28 && v29)` guard).
        if (lpCommand && lpRun)
        {
            // +8 = the bound texture id, +12 = the corner-run pointer (the console writes the run
            // pointer as a 32-bit word; here it is the typed run pointer).
            lpCommand->mpTexture  = reinterpret_cast<renderengine::Texture*>(luTextureId);
            lpCommand->mpVertices = lpRun;
            lpRun[0] = lrCorner0;   // transformed min corner {pos, colour, uv}
            lpRun[1] = lrCorner1;   // transformed max corner {pos, colour, uv}
        }
    }

    // -------------------------------------------------------------------------
    // EndMask - the Apt mask SUBTRACT op (DrawRenderingUnit @0x5CBA30 loc_5CBF24). A 16-byte
    // header-only {muType=19, muSize=16} record; overflow rewinds like the other writers.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::EndMask()
    {
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + 16u)
        {
            ImCommand* lpCommand = reinterpret_cast<ImCommand*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = 16u;
            lpCommand->muType = IM_CMD_END_MASK;                             // 19
            mpWriteBuffer->muCommandBufferWritePos = luPos + 16u;
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
    // GPU DISPATCH (the consumer side) - the PC/D3D9 realisation of
    // Im2dRenderBuffer::Dispatch @0x575BD4.
    //
    // The PS3 body (read in full from the External ELF dossier) walks the frozen
    // DISPATCH buffer with GetFirstCommand/GetNextCommand and switches on each
    // ImCommand::muType, re-issuing the command to the GPU. On the PS3 that re-issue
    // pushes the RSX ring through shadow::Device::*FastPS3 + cellGcm* + the DirectDraw
    // segment ring (the X360 pushes the D3D ring); that lowest device-internal push is
    // the only PLATFORM-DIVERGENT leaf. The portable shape is the command WALK and the
    // opcode->state/draw mapping, which the PS3 switch makes explicit:
    //   case 0  BEGIN_RENDERING      : open a render block (reset bound state, bind program)
    //   case 1  END_RENDERING        : close the block
    //   case 2  RENDER_PRIMITIVES    : *(cmd+8)=primType, *(cmd+12)=verts, *(cmd+16)=count -> Draw
    //   case 3  SET_STATE_BLEND      : *(cmd+8) -> SetStateFastPS3(BlendState*)
    //   case 4  SET_STATE_DEPTH      : *(cmd+8) -> SetStateFastPS3(DepthStencilState*)
    //   case 6  SET_STATE_RASTERIZER : *(cmd+8) -> SetStateFastPS3(RasterizerState*)
    //   case 8  SET_STATE_SAMPLER    : *(cmd+8) -> SetSamplerStateFastPS3(SamplerState*,0)
    //   case 9  SET_STATE_TEXTURE    : *(cmd+8) -> SetTextureStateFastPS3(TextureState*,0)
    //   (case 10 SET_TEXTURE / 15 SET_PROGRAM / 16 SET_TRANSFORM / 18 PUSH_MASK / 19 END_MASK
    //    are the Apt-rasteriser-emitted opcodes the same switch consumes.)
    //
    // The PC realisation below walks the buffer identically and translates each opcode
    // into the EXISTING src/pc/gcm/renderengine D3D9 device (renderengine::gDevice),
    // reusing the exact idioms already proven in CgsIm2d.cpp: state commands map to
    // SetRenderState / SetTexture / sampler stage state; RENDER_PRIMITIVES folds the
    // current SET_TRANSFORM (origin + right/up basis + colour shift/scale) into each
    // vertex on the CPU, scales the 1280x720 logical space to the back buffer, and
    // submits one DrawPrimitiveUP per draw (D3DFVF_XYZRHW screen-space verts).
    //
    // FLAG: the single device-internal leaf is the D3D ring write itself, which lives
    //       inside IDirect3DDevice9::DrawPrimitiveUP (the pc/gcm/renderengine device) -
    //       there is nothing lower for this TU to reconstruct; the renderengine device
    //       already exposes the draw call, so no extra FLAG'd shim is introduced here.
    // =========================================================================
    namespace
    {
        // The D3D9 pre-transformed screen-space vertex DrawPrimitiveUP consumes (same shape
        // CgsIm2d.cpp uses for the immediate path).
        struct DispatchScreenVertex { float x, y, z, rhw; u32 color; float u, v; };
        const unsigned long KU_DISPATCH_FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

        // The engine works in a fixed 1280x720 logical space; map it onto the actual back buffer.
        const f32 KF_DISPATCH_LOGICAL_W = 1280.0f;
        const f32 KF_DISPATCH_LOGICAL_H = 720.0f;

        // PrimitiveType -> D3DPRIMITIVETYPE + primitive count. The Apt path stores 4 (TRIANGLES),
        // 6 (TRIANGLESTRIPS) and 2 (LINES); map them to their D3D9 topologies (matching CgsIm2d.cpp,
        // which draws the loading-screen quads as strips).
        bool DispatchTopology(s32 liPrimitiveType, u32 luNumVertices,
                              D3DPRIMITIVETYPE* lpeTypeOut, u32* lpuPrimCountOut)
        {
            switch (liPrimitiveType)
            {
            case 2:  // PRIMITIVETYPE_LINES
                if (luNumVertices < 2u) return false;
                *lpeTypeOut = D3DPT_LINELIST;       *lpuPrimCountOut = luNumVertices / 2u;     return true;
            case 4:  // PRIMITIVETYPE_TRIANGLES
                if (luNumVertices < 3u) return false;
                *lpeTypeOut = D3DPT_TRIANGLELIST;   *lpuPrimCountOut = luNumVertices / 3u;     return true;
            case 6:  // PRIMITIVETYPE_TRIANGLESTRIPS
            default:
                if (luNumVertices < 3u) return false;
                *lpeTypeOut = D3DPT_TRIANGLESTRIP;  *lpuPrimCountOut = luNumVertices - 2u;     return true;
            }
        }

        // Fold one colour channel through the batch transform's colour shift/scale (floats in [0,1],
        // the X360 vertex-program colour transform) and re-pack to 8-bit. shift/scale default to
        // {0,1} (identity) for buffers that never emitted a SET_TRANSFORM.
        u8 DispatchColourChannel(u8 lu8In, f32 lfScale, f32 lfShift)
        {
            f32 lfOut = (static_cast<f32>(lu8In) / 255.0f) * lfScale + lfShift;
            if (lfOut < 0.0f) lfOut = 0.0f;
            if (lfOut > 1.0f) lfOut = 1.0f;
            return static_cast<u8>(lfOut * 255.0f + 0.5f);
        }
    }

    // -------------------------------------------------------------------------
    // Dispatch @0x575BD4 - the PC/D3D9 command-walk. Drains the frozen dispatch buffer
    // and submits each command through renderengine::gDevice.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::Dispatch()
    {
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice == nullptr)
        {
            return;
        }

        const f32 lfScaleX = static_cast<f32>(renderengine::gDisplayWidth)  / KF_DISPATCH_LOGICAL_W;
        const f32 lfScaleY = static_cast<f32>(renderengine::gDisplayHeight) / KF_DISPATCH_LOGICAL_H;

        // The per-batch transform the current SET_TRANSFORM command latched (identity until one is
        // seen): screen pos = origin + right*v.x + up*v.y, colour = colour*scale + shift. mRightUp
        // holds the apt 2x2 {a,b,c,d} exactly as AptCallbackRender::SetVertexMatrix stamps it
        // (mRightUp.x=a, .y=b, .z=c, .w=d). The screen fold is the standard SWF/apt affine
        // x'=a*localX + c*localY + tx, y'=b*localX + d*localY + ty -- i.e. the "right" basis (the
        // localX multipliers) is (a,b)=(.x,.y) and the "up" basis (the localY multipliers) is
        // (c,d)=(.z,.w). This is the SAME unpacking the committed AptCallbackRender::DrawRenderingUnit
        // mask-corner fold uses; the two render paths must agree.
        bool        lbHaveTransform = false;
        Im2dTransform lTransform;
        // Identity transform defaults (used until a SET_TRANSFORM arrives).
        f32 lfOriginX = 0.0f, lfOriginY = 0.0f;
        f32 lfRightX  = 1.0f, lfRightY  = 0.0f;
        f32 lfUpX     = 0.0f, lfUpY     = 1.0f;
        f32 lfColScaleR = 1.0f, lfColScaleG = 1.0f, lfColScaleB = 1.0f, lfColScaleA = 1.0f;
        f32 lfColShiftR = 0.0f, lfColShiftG = 0.0f, lfColShiftB = 0.0f, lfColShiftA = 0.0f;

        // BeginRendering's D3D state prologue (matches CgsIm2d.cpp's ImRenderer::BeginRendering):
        // no lighting, no depth, no cull, alpha-blend over the framebuffer, bilinear filtering, and
        // the colour stage modulating the bound texture by the vertex colour.
        lpDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
        lpDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        lpDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        lpDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        lpDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        lpDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        lpDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        lpDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        lpDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        lpDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        lpDevice->SetFVF(KU_DISPATCH_FVF);

        // A scratch CPU batch for the per-draw vertex fold. Sized like CgsIm2d.cpp's reserve scratch
        // so a whole line of glyph quads / a full Apt mesh never overflows in one DrawPrimitiveUP.
        enum { KI_DISPATCH_MAX = 2048 };
        static DispatchScreenVertex saBatch[KI_DISPATCH_MAX];

        // Walk the frozen dispatch buffer command-by-command (the PS3 while(GetNextCommand) loop).
        for (const ImCommand* lpCommand = GetFirstCommand();
             lpCommand != nullptr;
             lpCommand = GetNextCommand(lpCommand))
        {
            switch (lpCommand->muType)
            {
            case IM_CMD_BEGIN_RENDERING:   // case 0
            case IM_CMD_END_RENDERING:     // case 1
                // Block open/close: the PS3 resets/clears the active-renderer shadow; on PC the
                // device render state is already installed above and persists across the block.
                break;

            case IM_CMD_SET_TRANSFORM:     // case 16 - latch the per-batch transform (80-byte record)
            {
                const ImCommandSetTransform* lpSet =
                    static_cast<const ImCommandSetTransform*>(lpCommand);
                lTransform      = lpSet->mTransform;
                lbHaveTransform = true;
                lfOriginX = lTransform.mOriginXYZ.x; lfOriginY = lTransform.mOriginXYZ.y;
                // mRightUp = the apt 2x2 {a,b,c,d}: right basis (localX multipliers) = (a,b) = (.x,.y),
                // up basis (localY multipliers) = (c,d) = (.z,.w). Folds to x'=a*x+c*y, y'=b*x+d*y --
                // matching AptCallbackRender::SetVertexMatrix's packing + DrawRenderingUnit's fold.
                lfRightX  = lTransform.mRightUp.x;   lfRightY  = lTransform.mRightUp.y;
                lfUpX     = lTransform.mRightUp.z;   lfUpY     = lTransform.mRightUp.w;
                lfColScaleR = lTransform.mColourScale.x; lfColScaleG = lTransform.mColourScale.y;
                lfColScaleB = lTransform.mColourScale.z; lfColScaleA = lTransform.mColourScale.w;
                lfColShiftR = lTransform.mColourShift.x; lfColShiftG = lTransform.mColourShift.y;
                lfColShiftB = lTransform.mColourShift.z; lfColShiftA = lTransform.mColourShift.w;
                break;
            }

            case IM_CMD_SET_STATE_TEXTURE: // case 9 - bind a resolved renderengine::TextureState
            {
                const ImCommandSetStateTexture* lpSet =
                    static_cast<const ImCommandSetStateTexture*>(lpCommand);
                const renderengine::TextureState* lpState =
                    reinterpret_cast<const renderengine::TextureState*>(lpSet->mpTextureState);
                renderengine::Texture* lpTexture = (lpState != nullptr) ? lpState->mpRaster : nullptr;
                lpDevice->SetTexture(0, lpTexture != nullptr ? lpTexture->mpD3DTexture : nullptr);
                // Texture present: modulate by vertex colour. (CgsIm2d.cpp's SetState(TextureState*).)
                lpDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                lpDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                break;
            }

            case IM_CMD_SET_TEXTURE:       // case 10 - bind a raw renderengine::Texture (white fallback)
            {
                const ImCommandSetTexture* lpSet =
                    static_cast<const ImCommandSetTexture*>(lpCommand);
                renderengine::Texture* lpTexture = lpSet->mpTexture;
                lpDevice->SetTexture(0, lpTexture != nullptr ? lpTexture->mpD3DTexture : nullptr);
                // No texture -> drive the stage from the vertex colour alone (SELECTARG2 = DIFFUSE),
                // exactly as CgsIm2d.cpp's SetTexture(nullptr) path does for the untextured solids.
                const unsigned long luOp = (lpTexture != nullptr) ? D3DTOP_MODULATE : D3DTOP_SELECTARG2;
                lpDevice->SetTextureStageState(0, D3DTSS_COLOROP, luOp);
                lpDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, luOp);
                break;
            }

            case IM_CMD_SET_STATE_BLEND:   // case 3 - install the standard alpha-blend-over state
                lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                lpDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                lpDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
                break;

            case IM_CMD_RENDER_PRIMITIVES: // case 2 - the actual draw
            {
                const ImCommandRenderPrimitives<V>* lpDraw =
                    static_cast<const ImCommandRenderPrimitives<V>*>(lpCommand);
                const V* lpVertices = lpDraw->mpVertices;
                u32      luCount    = lpDraw->muNumVertices;
                if (lpVertices == nullptr || luCount < 2u)
                {
                    break;
                }

                D3DPRIMITIVETYPE leTopology;
                u32              luPrimCount;
                if (!DispatchTopology(static_cast<s32>(lpDraw->mePrimitiveType), luCount,
                                      &leTopology, &luPrimCount))
                {
                    break;
                }

                if (luCount > static_cast<u32>(KI_DISPATCH_MAX))
                {
                    luCount = static_cast<u32>(KI_DISPATCH_MAX);
                }

                // Fold the batch transform into each vertex, scale logical->back-buffer, pack colour.
                for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
                {
                    const Basic2dColouredTexturedVertex& lrVertex =
                        reinterpret_cast<const Basic2dColouredTexturedVertex*>(lpVertices)[luIndex];

                    f32 lfLocalX = lrVertex.mv2Pos.x;
                    f32 lfLocalY = lrVertex.mv2Pos.y;
                    f32 lfScreenX, lfScreenY;
                    if (lbHaveTransform)
                    {
                        lfScreenX = lfOriginX + lfRightX * lfLocalX + lfUpX * lfLocalY;
                        lfScreenY = lfOriginY + lfRightY * lfLocalX + lfUpY * lfLocalY;
                    }
                    else
                    {
                        lfScreenX = lfLocalX;
                        lfScreenY = lfLocalY;
                    }

                    saBatch[luIndex].x   = lfScreenX * lfScaleX;
                    saBatch[luIndex].y   = lfScreenY * lfScaleY;
                    saBatch[luIndex].z   = 0.0f;
                    saBatch[luIndex].rhw = 1.0f;

                    u8 lu8R = lrVertex.mv4Colour.r;
                    u8 lu8G = lrVertex.mv4Colour.g;
                    u8 lu8B = lrVertex.mv4Colour.b;
                    u8 lu8A = lrVertex.mv4Colour.a;
                    if (lbHaveTransform)
                    {
                        lu8R = DispatchColourChannel(lu8R, lfColScaleR, lfColShiftR);
                        lu8G = DispatchColourChannel(lu8G, lfColScaleG, lfColShiftG);
                        lu8B = DispatchColourChannel(lu8B, lfColScaleB, lfColShiftB);
                        lu8A = DispatchColourChannel(lu8A, lfColScaleA, lfColShiftA);
                    }
                    saBatch[luIndex].color =
                        static_cast<u32>(D3DCOLOR_ARGB(lu8A, lu8R, lu8G, lu8B));

                    saBatch[luIndex].u = lrVertex.mv2Tex0UV.x;
                    saBatch[luIndex].v = lrVertex.mv2Tex0UV.y;
                }

                // Recompute the primitive count if the count was clamped.
                if (!DispatchTopology(static_cast<s32>(lpDraw->mePrimitiveType), luCount,
                                      &leTopology, &luPrimCount) || luPrimCount == 0u)
                {
                    break;
                }
                lpDevice->DrawPrimitiveUP(leTopology, luPrimCount, saBatch, sizeof(DispatchScreenVertex));
                break;
            }

            // The remaining state opcodes (SET_STATE_DEPTH/RASTERIZER/SAMPLER/RT, SET_VIEWPORT/
            // SCISSOR/CLEAR, SET_SHADER_PROGRAM) and the mask ops (PUSH_MASK_GEOMETRY/END_MASK)
            // configure GPU descriptor state the fixed-function PC 2D path already covers via the
            // BeginRendering prologue above (or, for the stencil mask, is a follow-on). They are
            // walked-over here (the stride is correct - GetNextCommand uses muSize) so the draw
            // stream stays in sync; the visible Apt/GUI/text output is produced by the texture/
            // transform/draw opcodes handled above.
            default:
                break;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Explicit instantiation: the screen-space 2D buffer the Apt/Flapt/GUI/text
    // rasteriser fills and submits (the family this TU targets). This is the
    // <Basic2dColouredTexturedVertex> instance whose bodies the dossiers above
    // were read from.
    // -------------------------------------------------------------------------
    template struct ImRenderBuffer<Basic2dColouredTexturedVertex>;
}
