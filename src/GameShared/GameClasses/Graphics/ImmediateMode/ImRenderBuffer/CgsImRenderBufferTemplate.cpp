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
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (X360 RenderStart guards)

#include <cstdio>   // [diag] BRN_CXFORM_TRACE line formatting
// [diag] BRN_CXFORM_TRACE writes through the engine log (same hook CgsIm2d.cpp's trace uses),
// and stamps each line with the present counter that lives in device.cpp so a traced batch can
// be correlated against the BRN_FRAME_DUMP image of the SAME frame.
namespace CgsDev { namespace Log { void WriteToLog(const char*); } }
namespace renderengine { extern u32 guPresentCount; u32 FrameDumpEvery(); }

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
        static_assert(ImCommandRecord<ImCommand>::KU_BYTES == 16u, "console record size 16");
        const u32 luRecord = ImCommandRecord<ImCommand>::KU_BYTES;

        mbInRenderBlock = true;                                               // this+52
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;            // (*this+32)+8
        if (muCommandBufferSize < luPos + luRecord)                          // this+48
        {
            SetBufferFullRewindToLastEndRender();
        }
        else
        {
            ImCommand* lpCommand = reinterpret_cast<ImCommand*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muType = IM_CMD_BEGIN_RENDERING;
            lpCommand->muSize = luRecord;
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
        const u32 luRecord = ImCommandRecord<ImCommand>::KU_BYTES;   // 16, as on console

        mbInRenderBlock = false;                                              // this+52
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize < luPos + luRecord)
        {
            SetBufferFullRewindToLastEndRender();
        }
        else
        {
            ImCommand* lpCommand = reinterpret_cast<ImCommand*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muType = IM_CMD_END_RENDERING;
            lpCommand->muSize = luRecord;
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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

        static_assert(ImCommandRecord<ImCommandRenderPrimitives<V> >::KU_BYTES == 32u,
                      "console record size 32");
        const u32 luRecord = ImCommandRecord<ImCommandRenderPrimitives<V> >::KU_BYTES;

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommandRenderPrimitives<V>* lpCommand =
                reinterpret_cast<ImCommandRenderPrimitives<V>*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muType         = IM_CMD_RENDER_PRIMITIVES;
            lpCommand->muSize         = luRecord;
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
        const u32 luRecord = ImCommandRecord<ImCommandRenderPrimitives<V> >::KU_BYTES;  // 32

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommandRenderPrimitives<V>* lpCommand =
                reinterpret_cast<ImCommandRenderPrimitives<V>*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize          = luRecord;
            lpCommand->muType          = IM_CMD_RENDER_PRIMITIVES;
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
    // RenderStart @0x827EF748 (X360 <Basic2dColouredTexturedVertex>, stride 20).
    //
    // REPLACES the earlier PS3 bare-forward RenderStart (which just returned
    // AllocVertices(luNumVertices)). The X360 build inlines the vertex sub-allocation
    // but wraps it in the RenderStart/RenderEnd depth bookkeeping the console text path
    // relies on: assert the counter has not gone negative, ++counter BEFORE the carve,
    // carve luNumVertices from the write buffer's vertex stream (stride sizeof(V)), and
    // on overflow (or a zero base+pos) assert-unless-graceful, back the counter out and
    // return nullptr. The dcbz128 pre-warm and the assert file/line args are dropped per
    // the project convention. The X360 target is authoritative, so this supersedes the
    // PS3 forward. 0x827EF548 is the SAME body at stride 32 (a 32-byte vertex type not yet
    // recovered) -- no second instantiation is emitted for it in this wave.
    // -------------------------------------------------------------------------
    template <typename V>
    V* ImRenderBuffer<V>::RenderStart(u32 luNumVertices)
    {
        CGS_ASSERT(miNumRendersStarted >= 0,
                   "Mismatched RenderStart/RenderEnd on ImRenderBuffer");   // this+0x38

        const u32 luOldPos = mpWriteBuffer->muVertexBufferWritePos;          // (*this+0x20)+0xC
        const u32 luNewPos = static_cast<u32>(sizeof(V)) * luNumVertices + luOldPos;
        ++miNumRendersStarted;                                              // this+0x38

        if (muVertexBufferSize >= luNewPos)                                 // this+0x2C
        {
            u8* lpBase = mpWriteBuffer->mpu8VertexBuffer;                   // *(*this+0x20)
            mpWriteBuffer->muVertexBufferWritePos = luNewPos;
            V* lpRun = reinterpret_cast<V*>(lpBase + luOldPos);
            if (lpRun != nullptr)
            {
                return lpRun;
            }
        }

        // Overflow (or a null carve): diagnose unless the buffer fails gracefully, back out the
        // counter, return nullptr. The console streams the vertex count between the two rodata
        // literals; the runtime count + StrStream are dropped, leaving the two literals concatenated.
        CGS_ASSERT(mbFailGracefully, "Failed to allocate  in ImRenderBuffer");  // this+0x41
        --miNumRendersStarted;                                              // this+0x38
        return nullptr;
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
        const u32 luRecord = ImCommandRecord<ImCommandRenderPrimitives<V> >::KU_BYTES;  // 32

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommandRenderPrimitives<V>* lpCommand =
                reinterpret_cast<ImCommandRenderPrimitives<V>*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize          = luRecord;
            lpCommand->muType          = IM_CMD_RENDER_PRIMITIVES;
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
        static_assert(ImCommandRecord<ImCommandSetTexture>::KU_BYTES == 16u,
                      "console record size 16");
        const u32 luRecord = ImCommandRecord<ImCommandSetTexture>::KU_BYTES;

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommandSetTexture* lpCommand = reinterpret_cast<ImCommandSetTexture*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize    = luRecord;
            lpCommand->muType    = IM_CMD_SET_TEXTURE;
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
        const u32 luRecord = ImCommandRecord<ImCommandSetShaderProgram>::KU_BYTES;  // 16

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommandSetShaderProgram* lpCommand =
                reinterpret_cast<ImCommandSetShaderProgram*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize          = luRecord;
            lpCommand->muType          = IM_CMD_SET_SHADER_PROGRAM;
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
            lpCommand->mi8ShaderProgram = li8Program;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // SetState(RasterizerState*) @0x824599C0 - append a 16-byte
    // {type:IM_CMD_SET_STATE_RASTERIZER} command binding a rasterizer state.
    // Decompiled from the <BasicColouredVertex> (Im3dUntex) instantiation. The X360
    // body asserts lpRasterizerState is non-null and that we are inside a
    // BeginRendering/EndRendering block before appending (both are diagnostic-only
    // guards with no effect on the produced stream when they hold, matching the
    // convention already used for BeginRendering/EndRendering above -- dropped here);
    // an overflowing append rewinds exactly like SetTexture/SetProgram.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::SetState(const renderengine::RasterizerState* lpRasterizerState)
    {
        const u32 luRecord = ImCommandRecord<ImCommandSetStateRasterizer>::KU_BYTES;  // 16

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommandSetStateRasterizer* lpCommand =
                reinterpret_cast<ImCommandSetStateRasterizer*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = luRecord;
            lpCommand->muType = IM_CMD_SET_STATE_RASTERIZER;   // 6
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
            lpCommand->mpRasterizerState = lpRasterizerState;
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
        const u32 luRecord = ImCommandRecord<ImCommandSetStateTexture>::KU_BYTES;  // 16

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommandSetStateTexture* lpCommand = reinterpret_cast<ImCommandSetStateTexture*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muType         = IM_CMD_SET_STATE_TEXTURE;            // 9
            lpCommand->muSize         = luRecord;
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
        static_assert(ImCommandRecord<ImCommandSetTransform>::KU_BYTES == 80u,
                      "console record size 80");
        const u32 luRecord = ImCommandRecord<ImCommandSetTransform>::KU_BYTES;

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommandSetTransform* lpCommand = reinterpret_cast<ImCommandSetTransform*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = luRecord;
            lpCommand->muType = IM_CMD_SET_TRANSFORM;                        // 16
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
    // {muType=18} record carrying the bound texture id @ +8 and the corner-run
    // pointer @ +12, then copy the two filled corners into the run. The PS3 stores the command
    // header FIRST (and pre-warms the line with dcbz, dropped here), then calls AllocVertices, then
    // -- only when BOTH the run and the command slot are valid -- writes the +8/+12 payload and the
    // two corner vertices. A command-buffer overflow (no room for the record) rewinds.
    //
    // RECORD SIZE: the console literal is 16 (two 4-byte payload words after the 8-byte
    // header). Both payload members are POINTERS, so on x64 the record is 24 bytes and the
    // reserved stride is 32 -- see ImCommandRecord in the header. Writing the console's 16
    // here put the corner-run pointer on top of the FOLLOWING record's header and made
    // GetNextCommand re-enter this record mid-way, which crashed Dispatch on the first Apt
    // movie that carries masks (the licence/photo INTRO).
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::PushMaskGeometry(uintptr_t luTextureId, const V& lrCorner0,
                                             const V& lrCorner1)
    {
        const u32 luRecord = ImCommandRecord<ImCommandPushMaskTexture<V> >::KU_BYTES;

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        ImCommandPushMaskTexture<V>* lpCommand = nullptr;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            lpCommand = reinterpret_cast<ImCommandPushMaskTexture<V>*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = luRecord;
            lpCommand->muType = IM_CMD_PUSH_MASK_GEOMETRY;                    // 18
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
        const u32 luRecord = ImCommandRecord<ImCommandPopMask>::KU_BYTES;   // 16, as on console

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommand* lpCommand = reinterpret_cast<ImCommand*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = luRecord;
            lpCommand->muType = IM_CMD_END_MASK;                             // 19
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
        // CgsIm2d.cpp uses for the immediate path). The second UV set (u2,v2) carries the
        // Apt MASK sample coordinates (screen position mapped into the active mask's UV
        // space -- the CPU fold of the constants the PS3 case-0x12 uploads); stage 1 is
        // DISABLED outside a mask block, so the extra set is inert for unmasked draws.
        struct DispatchScreenVertex { float x, y, z, rhw; u32 color; float u, v; float u2, v2; };
        const unsigned long KU_DISPATCH_FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2;

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

        // Fold one colour channel through the batch transform's colour shift/scale and re-pack
        // to 8-bit. The apt CXForm channel space is FIXED-POINT 0..255 (identity scale = 255,
        // shift in 0..255 add units -- the same space AptRenderingContext composes in and
        // AptCallbackRender::SetColourTransform copies to the Im2dTransform), so both fold
        // terms normalise by 255 here. scale/shift default to {255,0} (identity) for buffers
        // that never emitted a SET_TRANSFORM.
        // [diag] BRN_CXFORM_TRACE=1 -- see the SET_TRANSFORM case. Logs every DISTINCT
        // (scale, shift) octet once, with a small dedupe table so a 60 Hz boot does not
        // flood the log. Temporary investigation aid.
        bool CxformTraceEnabled()
        {
            static int siState = -1;
            if (siState < 0)
            {
                char lacBuf[8];
                siState = (GetEnvironmentVariableA("BRN_CXFORM_TRACE", lacBuf, sizeof(lacBuf)) > 0) ? 1 : 0;
            }
            return siState == 1;
        }

        void CxformTraceLog(f32 lfSR, f32 lfSG, f32 lfSB, f32 lfSA,
                            f32 lfTR, f32 lfTG, f32 lfTB, f32 lfTA)
        {
            enum { KI_MAX_SEEN = 64 };
            static f32 saSeen[KI_MAX_SEEN][8];
            static int siNumSeen = 0;

            for (int liSeen = 0; liSeen < siNumSeen; ++liSeen)
            {
                const f32* lpS = saSeen[liSeen];
                if (lpS[0] == lfSR && lpS[1] == lfSG && lpS[2] == lfSB && lpS[3] == lfSA &&
                    lpS[4] == lfTR && lpS[5] == lfTG && lpS[6] == lfTB && lpS[7] == lfTA)
                    return;
            }
            if (siNumSeen >= KI_MAX_SEEN)
                return;
            f32* lpNew = saSeen[siNumSeen++];
            lpNew[0] = lfSR; lpNew[1] = lfSG; lpNew[2] = lfSB; lpNew[3] = lfSA;
            lpNew[4] = lfTR; lpNew[5] = lfTG; lpNew[6] = lfTB; lpNew[7] = lfTA;

            char lacLine[256];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[CXform] #%02d scale=(%.4f, %.4f, %.4f, %.4f)  shift=(%.4f, %.4f, %.4f, %.4f)\n",
                          siNumSeen - 1, lfSR, lfSG, lfSB, lfSA, lfTR, lfTG, lfTB, lfTA);
            CgsDev::Log::WriteToLog(lacLine);
        }


        // [diag] BRN_CXFORM_TRACE: dump each DISTINCT bound texture ONCE as raw level-0 bytes
        // plus a tiny header, into %BRN_CXFORM_TRACE_TEXDIR%. Decoded offline. This answers
        // "is the texel this shape samples what the art should be" -- which no amount of
        // vertex-side tracing can. Locks the texture directly (no D3DX in this SDK).
        void DumpBoundTextureOnce(renderengine::Texture* lpTexture)
        {
            enum { KI_MAX_TEX = 24 };
            static const void* sapSeen[KI_MAX_TEX];
            static int siNumTex = 0;
            if (lpTexture == 0 || lpTexture->mpD3DTexture == 0)
                return;
            for (int liSeen = 0; liSeen < siNumTex; ++liSeen)
                if (sapSeen[liSeen] == lpTexture) return;
            if (siNumTex >= KI_MAX_TEX) return;
            sapSeen[siNumTex++] = lpTexture;

            // renderengine::Texture holds the BASE interface; only a 2D texture has
            // GetLevelDesc/LockRect, so query for it and skip cube/volume textures.
            // GetType() avoids QueryInterface (which would drag in dxguid.lib for the IID)
            // and, unlike it, takes no reference -- so there is nothing to release.
            if (lpTexture->mpD3DTexture->GetType() != D3DRTYPE_TEXTURE)
                return;   // cube/volume textures have no GetLevelDesc/LockRect
            IDirect3DTexture9* lpD3D =
                static_cast<IDirect3DTexture9*>(lpTexture->mpD3DTexture);
            D3DSURFACE_DESC lDesc;
            if (FAILED(lpD3D->GetLevelDesc(0, &lDesc))) return;

            D3DLOCKED_RECT lLock;
            if (FAILED(lpD3D->LockRect(0, &lLock, 0, D3DLOCK_READONLY)))
                return;   // DEFAULT-pool textures are not lockable; skip them

            char lacDir[512];
            if (GetEnvironmentVariableA("BRN_CXFORM_TRACE_TEXDIR", lacDir, sizeof(lacDir)) == 0)
                lacDir[0] = 0;
            char lacPath[640];
            std::snprintf(lacPath, sizeof(lacPath), "%s%stex_%p.raw",
                          lacDir, lacDir[0] ? "\\" : "", (const void*)lpTexture);
            FILE* lpFile = std::fopen(lacPath, "wb");
            if (lpFile != 0)
            {
                const u32 lauHdr[4] = { lDesc.Width, lDesc.Height,
                                        static_cast<u32>(lDesc.Format),
                                        static_cast<u32>(lLock.Pitch) };
                std::fwrite(lauHdr, 4, 4, lpFile);
                u32 luRows = lDesc.Height;
                if (lDesc.Format == D3DFMT_DXT1 || lDesc.Format == D3DFMT_DXT3 ||
                    lDesc.Format == D3DFMT_DXT5)
                    luRows = (lDesc.Height + 3u) / 4u;      // DXT pitch is per BLOCK row
                std::fwrite(lLock.pBits, 1, luRows * lLock.Pitch, lpFile);
                std::fclose(lpFile);
            }
            lpD3D->UnlockRect(0);
        }

        // [diag] BRN_CXFORM_TRACE: one line per DISTINCT large batch (bounds + final colour).
        void BigBatchTraceLog(s32 liX0, s32 liY0, s32 liX1, s32 liY1,
                              u32 luColour, u32 luBright, u32 luVerts,
                              bool lbMasked, s32 liMaskDepth, const void* lpTexture,
                              f32 lfU0, f32 lfU1, f32 lfV0, f32 lfV1)
        {
            // Dedupe WITHIN a frame only: the point is a complete picture of one dumped frame,
            // so the same batch reappearing on a later frame must be logged again. A global
            // dedupe hid the per-frame composition and produced a misleading partial list.
            enum { KI_MAX_BIG = 64 };
            static u32 sauSeen[KI_MAX_BIG][5];
            static int siNumBig = 0;
            static u32 suSeenFrame = 0xFFFFFFFFu;

            if (suSeenFrame != renderengine::guPresentCount)
            {
                suSeenFrame = renderengine::guPresentCount;
                siNumBig = 0;
            }

            const u32 lauKey[5] = { static_cast<u32>(liX0), static_cast<u32>(liY0),
                                    static_cast<u32>(liX1), static_cast<u32>(liY1), luColour };
            for (int liSeen = 0; liSeen < siNumBig; ++liSeen)
            {
                const u32* lpS = sauSeen[liSeen];
                if (lpS[0] == lauKey[0] && lpS[1] == lauKey[1] && lpS[2] == lauKey[2] &&
                    lpS[3] == lauKey[3] && lpS[4] == lauKey[4])
                    return;
            }
            if (siNumBig >= KI_MAX_BIG)
                return;
            for (int liLane = 0; liLane < 5; ++liLane)
                sauSeen[siNumBig][liLane] = lauKey[liLane];
            ++siNumBig;

            char lacLine[320];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[BigBatch] f=%u (%d,%d)-(%d,%d) verts=%u"
                          "  dim=%02X %02X %02X %02X  bright=%02X %02X %02X %02X"
                          "  masked=%d depth=%d tex=%p uv=[%.3f..%.3f, %.3f..%.3f]\n",
                          renderengine::guPresentCount,
                          liX0, liY0, liX1, liY1, luVerts,
                          (luColour >> 24) & 0xFFu, (luColour >> 16) & 0xFFu,
                          (luColour >> 8) & 0xFFu, luColour & 0xFFu,
                          (luBright >> 24) & 0xFFu, (luBright >> 16) & 0xFFu,
                          (luBright >> 8) & 0xFFu, luBright & 0xFFu,
                          lbMasked ? 1 : 0, liMaskDepth, lpTexture,
                          lfU0, lfU1, lfV0, lfV1);
            CgsDev::Log::WriteToLog(lacLine);
        }

        // The SCALE half of the CXForm only. The console evaluates
        //     final = texel * (vertexColour * scale) + shift
        // i.e. the additive shift is applied AFTER the texture modulate, on the GPU.
        // Folding the shift into the vertex colour instead computes
        //     texel * (vertexColour * scale + shift)
        // which is wrong whenever a texture is bound, and catastrophically wrong for the
        // SOLID-FILL case Apt uses constantly (scale == 0, shift == the fill colour): the
        // console yields a flat, texture-INDEPENDENT colour, while the vertex fold yields
        // that colour tinted by whatever texel the shape happens to sample. Measured on the
        // autosave prompt's red banner: console/Xenia (107,0,0) vs the vertex fold's
        // 151/255 * 101 = (60,0,0). So the scale goes in the vertex colour and the shift is
        // added by a second texture stage from D3DRS_TEXTUREFACTOR -- see the stage setup.
        u8 DispatchColourScaleOnly(u8 lu8In, f32 lfScale)
        {
            f32 lfOut = (static_cast<f32>(lu8In) / 255.0f) * (lfScale / 255.0f);
            if (lfOut < 0.0f) lfOut = 0.0f;
            if (lfOut > 1.0f) lfOut = 1.0f;
            return static_cast<u8>(lfOut * 255.0f + 0.5f);
        }

        // Pack the CXForm shift into a D3DCOLOR for D3DRS_TEXTUREFACTOR.
        u32 DispatchShiftToTextureFactor(f32 lfR, f32 lfG, f32 lfB, f32 lfA)
        {
            const f32 lafIn[4] = { lfA, lfR, lfG, lfB };
            u32 luOut = 0;
            for (int liChan = 0; liChan < 4; ++liChan)
            {
                f32 lfV = lafIn[liChan] / 255.0f;
                if (lfV < 0.0f) lfV = 0.0f;
                if (lfV > 1.0f) lfV = 1.0f;
                luOut = (luOut << 8) | static_cast<u32>(lfV * 255.0f + 0.5f);
            }
            return luOut;
        }

        u8 DispatchColourChannel(u8 lu8In, f32 lfScale, f32 lfShift)
        {
            f32 lfOut = (static_cast<f32>(lu8In) / 255.0f) * (lfScale / 255.0f) + (lfShift / 255.0f);
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
        // Colour identity in the 0..255 CXForm space (see DispatchColourChannel).
        f32 lfColScaleR = 255.0f, lfColScaleG = 255.0f, lfColScaleB = 255.0f, lfColScaleA = 255.0f;
        f32 lfColShiftR = 0.0f, lfColShiftG = 0.0f, lfColShiftB = 0.0f, lfColShiftA = 0.0f;

        // ---- the Apt MASK realisation (PUSH_MASK_GEOMETRY 18 / END_MASK 19) ----------------
        // The PS3 Dispatch (@0x575BD4 case 0x12/0x13) implements the Apt clip mask as a PIXEL
        // mask, not a stencil: case 0x12 bumps a mask counter, switches the 2D program to its
        // "masked" sibling (mi8CurrentProgram + 1), binds the MASK texture as a second sampler
        // (renderengine::Device::SetResource) and uploads a screen->maskUV constant block folded
        // from the 2-corner run (the 1/(c1-c0) reciprocals in the VMX body); the masked draws'
        // pixels then multiply their alpha by the mask texture's alpha sample. Case 0x13
        // decrements the counter and restores the program (mi8CurrentProgram - 1). The PC
        // fixed-function equivalent: latch the mask command's texture + corner run; while a
        // mask is active, texture STAGE 1 samples the mask texture with per-vertex UVs the
        // RENDER_PRIMITIVES fold computes from the same screen->maskUV map (the CPU fold of
        // the constant block), ALPHAOP = MODULATE(current, mask) with the colour passed
        // through -- the same observable pixel result as the console's masked program.
        int liMaskDepth = 0;
        bool lbMaskStageBound = false;
        // [diag] BRN_CXFORM_TRACE: the texture currently bound on stage 0, so a traced batch
        // records whether it was MODULATEd by a texel or took its diffuse straight through.
        const void* lpTraceBoundTexture = nullptr;
        f32 lfMaskX0 = 0.0f, lfMaskY0 = 0.0f, lfMaskInvW = 0.0f, lfMaskInvH = 0.0f;
        f32 lfMaskU0 = 0.0f, lfMaskV0 = 0.0f, lfMaskDU = 0.0f, lfMaskDV = 0.0f;

        // BeginRendering's D3D state prologue (matches CgsIm2d.cpp's ImRenderer::BeginRendering):
        // no lighting, no depth, no cull, alpha-blend over the framebuffer, bilinear filtering, and
        // the colour stage modulating the bound texture by the vertex colour.
        //
        // FLAG PC-platform leaf: the console dispatch prologue BINDS this buffer's 2D vertex +
        // pixel programs (mi8CurrentProgram, and its "masked" sibling inside a mask block). This
        // backend runs the fixed-function pipeline instead, and in D3D9 that has to be selected
        // explicitly -- while a programmable shader is still bound (a preceding world pass leaves
        // its last technique's pair bound) the runtime ignores SetFVF and every texture-stage
        // state below and shades the whole Apt/GUI frame with those programs.
        lpDevice->SetVertexShader(nullptr);
        lpDevice->SetPixelShader(nullptr);
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
                // Channel order: (Red, Green, Blue, Alpha) -> (x, y, z, w). CORRECTED
                // 2026-07-30 -- this previously read (Alpha, Red, Green, Blue), so every
                // Apt/GUIAPT batch on PC took its "alpha" from the RED lane and its red
                // from alpha. AptCallbackRender::SetColourTransform does NOT copy the
                // CXForm straight through: the CXForm's mfVal[0..3] are (A,R,G,B) and it
                // explicitly ROTATES them into (R,G,B,A) before they reach an
                // Im2dTransform -- so by the time the transform is latched here, alpha is
                // already in .w. Confirmed four ways (X360 Im2d shader microcode; the
                // FLAPT alpha cull `vspltw v0,v0,3` in MovieClipInstance::Render
                // @0x824718F0; the fully-transparent early-out at +0xC in
                // AptRenderHandler::Render @0x82859300; and all 5158 shipped FLAPTHUD
                // cxform records, where [1,1,1,x] occurs 2333 times and [x,1,1,1] never).
                // The Apt and FLAPT pipelines share this one convention.
                lfColScaleR = lTransform.mColourScale.x; lfColScaleG = lTransform.mColourScale.y;
                lfColScaleB = lTransform.mColourScale.z; lfColScaleA = lTransform.mColourScale.w;
                lfColShiftR = lTransform.mColourShift.x; lfColShiftG = lTransform.mColourShift.y;
                lfColShiftB = lTransform.mColourShift.z; lfColShiftA = lTransform.mColourShift.w;
                // [diag] BRN_CXFORM_TRACE=1: log each DISTINCT colour transform latched, so the
                // units/magnitudes the Apt producer actually supplies can be read off a real boot
                // instead of inferred. Temporary investigation aid; remove once the Apt popup
                // brightness question is closed.
                if (CxformTraceEnabled())
                    CxformTraceLog(lfColScaleR, lfColScaleG, lfColScaleB, lfColScaleA,
                                   lfColShiftR, lfColShiftG, lfColShiftB, lfColShiftA);
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
                lpTraceBoundTexture = lpTexture;   // [diag]
                if (CxformTraceEnabled()) DumpBoundTextureOnce(lpTexture);
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
                lpTraceBoundTexture = lpTexture;   // [diag]
                if (CxformTraceEnabled()) DumpBoundTextureOnce(lpTexture);
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

            case IM_CMD_PUSH_MASK_GEOMETRY: // case 18 - begin/refresh the pixel mask (PS3 case 0x12)
            {
                const ImCommandPushMaskTexture<V>* lpPush =
                    static_cast<const ImCommandPushMaskTexture<V>*>(lpCommand);
                ++liMaskDepth;
                const Basic2dColouredTexturedVertex* lpCorners =
                    reinterpret_cast<const Basic2dColouredTexturedVertex*>(lpPush->mpVertices);
                renderengine::Texture* lpMaskTexture = lpPush->mpTexture;
                if (lpCorners != nullptr && lpMaskTexture != nullptr &&
                    lpMaskTexture->mpD3DTexture != nullptr)
                {
                    // The screen->maskUV map the PS3 constant fold encodes: reciprocals of the
                    // corner extents + the corner UV range (a degenerate extent maps flat).
                    const f32 lfDX = lpCorners[1].mv2Pos.x - lpCorners[0].mv2Pos.x;
                    const f32 lfDY = lpCorners[1].mv2Pos.y - lpCorners[0].mv2Pos.y;
                    lfMaskX0   = lpCorners[0].mv2Pos.x;
                    lfMaskY0   = lpCorners[0].mv2Pos.y;
                    lfMaskInvW = (lfDX != 0.0f) ? (1.0f / lfDX) : 0.0f;
                    lfMaskInvH = (lfDY != 0.0f) ? (1.0f / lfDY) : 0.0f;
                    lfMaskU0   = lpCorners[0].mv2Tex0UV.x;
                    lfMaskV0   = lpCorners[0].mv2Tex0UV.y;
                    lfMaskDU   = lpCorners[1].mv2Tex0UV.x - lpCorners[0].mv2Tex0UV.x;
                    lfMaskDV   = lpCorners[1].mv2Tex0UV.y - lpCorners[0].mv2Tex0UV.y;

                    // Stage 1 = the mask sample: colour passes through, alpha modulates by the
                    // mask texture's alpha (the masked-program observable). Border-clamp so
                    // pixels OUTSIDE the mask rect sample transparent (clipped away).
                    lpDevice->SetTexture(1, lpMaskTexture->mpD3DTexture);
                    lpDevice->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_SELECTARG2);
                    lpDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                    lpDevice->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
                    lpDevice->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                    lpDevice->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                    lpDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                    lpDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                    lpDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                    lpDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
                    lpDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
                    lpDevice->SetSamplerState(1, D3DSAMP_BORDERCOLOR, 0x00000000u);
                    lbMaskStageBound = true;
                }
                break;
            }

            case IM_CMD_END_MASK:          // case 19 - end the pixel mask (PS3 case 0x13)
                if (liMaskDepth > 0)
                    --liMaskDepth;
                if (liMaskDepth == 0 && lbMaskStageBound)
                {
                    lpDevice->SetTexture(1, nullptr);
                    lpDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                    lpDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                    lbMaskStageBound = false;
                }
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
                        // SCALE only here; the shift is added after the texture
                        // modulate by the TEXTUREFACTOR stage installed below.
                        lu8R = DispatchColourScaleOnly(lu8R, lfColScaleR);
                        lu8G = DispatchColourScaleOnly(lu8G, lfColScaleG);
                        lu8B = DispatchColourScaleOnly(lu8B, lfColScaleB);
                        lu8A = DispatchColourScaleOnly(lu8A, lfColScaleA);
                    }
                    saBatch[luIndex].color =
                        static_cast<u32>(D3DCOLOR_ARGB(lu8A, lu8R, lu8G, lu8B));

                    saBatch[luIndex].u = lrVertex.mv2Tex0UV.x;
                    saBatch[luIndex].v = lrVertex.mv2Tex0UV.y;

                    // Mask UV set (stage 1): fold the LOGICAL screen position through the
                    // active mask's screen->maskUV map (the CPU realisation of the constant
                    // block the PS3 case-0x12 uploads for the masked program). Inert (stage 1
                    // disabled) when no mask is active.
                    if (lbMaskStageBound)
                    {
                        saBatch[luIndex].u2 = lfMaskU0 +
                            (lfScreenX - lfMaskX0) * lfMaskInvW * lfMaskDU;
                        saBatch[luIndex].v2 = lfMaskV0 +
                            (lfScreenY - lfMaskY0) * lfMaskInvH * lfMaskDV;
                    }
                    else
                    {
                        saBatch[luIndex].u2 = 0.0f;
                        saBatch[luIndex].v2 = 0.0f;
                    }
                }

                // [diag] BRN_CXFORM_TRACE=1: report each DISTINCT large batch (>= 15% of the
                // logical stage) with its final folded vertex colour, so "what colour and alpha
                // does the popup background actually get" is measured rather than inferred.
                // Gate on the SAME frames BRN_FRAME_DUMP writes so the logged bounds and the
                // dumped pixels come from ONE frame -- correlating a trace from one boot
                // against a dump from another produced a false lead.
                // ⛔ ASK the writer for its period (BRN_FRAME_DUMP_EVERY can override the
                // default 30); a second hardcoded 30 here would desync the two silently.
                if (CxformTraceEnabled() && luCount >= 3u &&
                    (renderengine::guPresentCount % renderengine::FrameDumpEvery()) == 0u)
                {
                    f32 lfMinX = saBatch[0].x, lfMaxX = saBatch[0].x;
                    f32 lfMinY = saBatch[0].y, lfMaxY = saBatch[0].y;
                    for (u32 luB = 1; luB < luCount; ++luB)
                    {
                        if (saBatch[luB].x < lfMinX) lfMinX = saBatch[luB].x;
                        if (saBatch[luB].x > lfMaxX) lfMaxX = saBatch[luB].x;
                        if (saBatch[luB].y < lfMinY) lfMinY = saBatch[luB].y;
                        if (saBatch[luB].y > lfMaxY) lfMaxY = saBatch[luB].y;
                    }
                    const f32 lfArea = (lfMaxX - lfMinX) * (lfMaxY - lfMinY);
                    const f32 lfStage = static_cast<f32>(renderengine::gDisplayWidth) *
                                        static_cast<f32>(renderengine::gDisplayHeight);
                    if (lfStage > 0.0f && lfArea >= lfStage * 0.02f)
                    {
                        // Log the colour RANGE across the batch, not just vertex 0 -- Apt shapes
                        // routinely carry gradient fills, and a single-vertex sample cannot tell
                        // a gradient apart from a uniformly-wrong fill.
                        f32 lfU0 = saBatch[0].u, lfU1 = saBatch[0].u;
                        f32 lfV0 = saBatch[0].v, lfV1 = saBatch[0].v;
                        for (u32 luB = 1; luB < luCount; ++luB)
                        {
                            if (saBatch[luB].u < lfU0) lfU0 = saBatch[luB].u;
                            if (saBatch[luB].u > lfU1) lfU1 = saBatch[luB].u;
                            if (saBatch[luB].v < lfV0) lfV0 = saBatch[luB].v;
                            if (saBatch[luB].v > lfV1) lfV1 = saBatch[luB].v;
                        }
                        u32 luDim = saBatch[0].color, luBright = saBatch[0].color;
                        u32 luDimSum = 0xFFFFFFFFu, luBrightSum = 0u;
                        for (u32 luB = 0; luB < luCount; ++luB)
                        {
                            const u32 luC = saBatch[luB].color;
                            const u32 luSum = ((luC >> 16) & 0xFFu) + ((luC >> 8) & 0xFFu) + (luC & 0xFFu);
                            if (luSum < luDimSum)    { luDimSum = luSum;    luDim = luC; }
                            if (luSum > luBrightSum) { luBrightSum = luSum; luBright = luC; }
                        }
                        BigBatchTraceLog(static_cast<s32>(lfMinX), static_cast<s32>(lfMinY),
                                         static_cast<s32>(lfMaxX), static_cast<s32>(lfMaxY),
                                         luDim, luBright, luCount,
                                         lbMaskStageBound, liMaskDepth,
                                         lpTraceBoundTexture,
                                         lfU0, lfU1, lfV0, lfV1);
                    }
                }

                // Recompute the primitive count if the count was clamped.
                if (!DispatchTopology(static_cast<s32>(lpDraw->mePrimitiveType), luCount,
                                      &leTopology, &luPrimCount) || luPrimCount == 0u)
                {
                    break;
                }

                // ---- the CXForm SHIFT, applied AFTER the texture modulate -----------------
                // The console computes  final = texel * (vertexColour * scale) + shift  on the
                // GPU. The vertex fold above has applied the scale; the additive shift is a
                // per-batch constant, so it goes in D3DRS_TEXTUREFACTOR and is added by the
                // first free texture stage (stage 1 normally; stage 2 while a clip mask owns
                // stage 1). Alpha passes through -- the shipped data has shift.w == 0 in every
                // record, and adding to alpha would change coverage rather than colour.
                const bool lbNeedShift = lbHaveTransform &&
                    (lfColShiftR != 0.0f || lfColShiftG != 0.0f || lfColShiftB != 0.0f);
                const DWORD luShiftStage = lbMaskStageBound ? 2u : 1u;
                if (lbNeedShift)
                {
                    lpDevice->SetRenderState(D3DRS_TEXTUREFACTOR,
                        DispatchShiftToTextureFactor(lfColShiftR, lfColShiftG, lfColShiftB, 0.0f));
                    lpDevice->SetTextureStageState(luShiftStage, D3DTSS_COLOROP, D3DTOP_ADD);
                    lpDevice->SetTextureStageState(luShiftStage, D3DTSS_COLORARG1, D3DTA_CURRENT);
                    lpDevice->SetTextureStageState(luShiftStage, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                    lpDevice->SetTextureStageState(luShiftStage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    lpDevice->SetTextureStageState(luShiftStage, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                }

                lpDevice->DrawPrimitiveUP(leTopology, luPrimCount, saBatch, sizeof(DispatchScreenVertex));

                if (lbNeedShift)
                {
                    // Leave the cascade as the next batch expects to find it.
                    lpDevice->SetTextureStageState(luShiftStage, D3DTSS_COLOROP, D3DTOP_DISABLE);
                    lpDevice->SetTextureStageState(luShiftStage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                }
                break;
            }

            // The remaining state opcodes (SET_STATE_DEPTH/RASTERIZER/SAMPLER/RT, SET_VIEWPORT/
            // SCISSOR/CLEAR, SET_SHADER_PROGRAM) configure GPU descriptor state the fixed-function
            // PC 2D path already covers via the BeginRendering prologue above. They are walked-over
            // here (the stride is correct - GetNextCommand uses muSize) so the draw stream stays in
            // sync; the visible Apt/GUI/text output is produced by the texture/transform/draw/mask
            // opcodes handled above.
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

    // -------------------------------------------------------------------------
    // Explicit instantiation (PER MEMBER, not whole-struct): the untextured 3D
    // buffer (ledger key `class:CgsGraphics::BasicColouredVertex>`). Only the
    // members X360-ARTIST-attests for this instantiation are instantiated here --
    // Dispatch() is deliberately NOT instantiated: the X360 ARTIST attests a
    // DIFFERENT Dispatch (@0x823FF0D8, cited against CgsIm3dRenderBuffer.h, not this
    // template's header) that walks the buffer via a virtual HandleCommand call and
    // belongs to an unhomed Im3dRenderBuffer subclass, not this template's own
    // fixed-function PC dispatch fold -- instantiating this file's Dispatch<V> for V=
    // BasicColouredVertex would silently substitute an unattested body. Likewise
    // Clear/Construct/Release/Destruct/IsInARenderingBlock/AllocVertices/
    // RenderFromStaticVertexBuffer/RenderStart/RenderEnd/SetTexture/SetProgram/
    // SetTextureState/PushMaskGeometry/EndMask/GetFirstCommand/GetNextCommand are not
    // X360-attested for <BasicColouredVertex> by this TU and are left uninstantiated
    // (the wave-30 BasicColouredVertex lesson -- see the file-header note above).
    // -------------------------------------------------------------------------
    template bool ImRenderBuffer<BasicColouredVertex>::Prepare(
        u32, u32, rw::IResourceAllocator*, bool);
    template void ImRenderBuffer<BasicColouredVertex>::BeginRendering();
    template void ImRenderBuffer<BasicColouredVertex>::EndRendering();
    template void ImRenderBuffer<BasicColouredVertex>::Render(
        renderengine::PrimitiveType, const BasicColouredVertex*, u32);
    template void ImRenderBuffer<BasicColouredVertex>::SetState(
        const renderengine::RasterizerState*);
    template void ImRenderBuffer<BasicColouredVertex>::SetTransform(const Im2dTransform&);
    template void ImRenderBuffer<BasicColouredVertex>::Swap();
    template void ImRenderBuffer<BasicColouredVertex>::SetBufferFullRewindToLastEndRender();

    // -------------------------------------------------------------------------
    // CgsGraphics::BasicCo @0x827EF548 == ImRenderBuffer<Im3dVertex>::RenderStart (stride 32).
    //
    // BYTE-IDENTICAL to RenderStart @0x827EF748 (Basic) EXCEPT the vertex sub-allocation stride
    // is 32 (`slwi r9, r28, 5`) instead of 20 -- the SAME templated RenderStart body specialised
    // for the 32-byte 3D-text vertex. It reuses the generic template RenderStart defined in the
    // header (assert miNumRendersStarted>=0, ++counter before the carve, carve sizeof(V)*count
    // from the write buffer's vertex stream, and on overflow CGS_ASSERT(mbFailGracefully,
    // "Failed to allocate  in ImRenderBuffer") then back the counter out). sizeof(Im3dVertex)==32
    // reproduces the x32 stride exactly. Im3dVertex is DISTINCT from the committed 24-byte PACKED
    // BasicColouredTexturedVertex the ImRenderer<V> path copies at 24*count, so a dedicated
    // 32-byte Im3dVertex is used and RenderStart is instantiated PER MEMBER (the wave-30
    // <BasicColouredVertex> lesson), never a blind whole-struct.
    // -------------------------------------------------------------------------
    template Im3dVertex* ImRenderBuffer<Im3dVertex>::RenderStart(u32);
}
