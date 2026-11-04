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
// Dispatch @0x827F9BA0 consumes the complete native stream, including the
// 32-byte clear record and its by-value colour at +16. Derived writers retain
// their native block assertions, overflow rewind and cache prewarm.
// =============================================================================

#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm2dRenderBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/pixelbuffer.h"
#include <ppcintrinsics.h>

namespace CgsGraphics
{

    // ARTIST827F9BA0: walk the frozen stream and issue each native command.
    void Im2dRenderBuffer::Dispatch(Im2d* lpRenderer) const
    {
        if (!mpDispatchBuffer->muCommandBufferWritePos) return;
        const u8* const lpBase = mpDispatchBuffer->mpu8CommandBuffer;
        if (!lpBase) return;
        u32 luPos = 0;
        while (luPos < mpDispatchBuffer->muCommandBufferWritePos)
        {
            const ImCommand* lpCommand = reinterpret_cast<const ImCommand*>(lpBase + luPos);
            switch (lpCommand->muType)
            {
            case 0: lpRenderer->BeginRendering(); break;
            case 1: lpRenderer->EndRendering(); break;
            case 2:
            {
                const ImCommandRenderPrimitives<Basic2dColouredTexturedVertex>* p = static_cast<const ImCommandRenderPrimitives<Basic2dColouredTexturedVertex>*>(lpCommand);
                lpRenderer->Render(p->mePrimitiveType, p->mpVertices, p->muNumVertices); break;
            }
            case 3: lpRenderer->SetState(static_cast<const ImCommandSetStateBlend*>(lpCommand)->mpBlendState); break;
            case 4: lpRenderer->SetState(static_cast<const ImCommandSetStateDepthStencil*>(lpCommand)->mpDepthStencilState); break;
            case 5:
            {
                const ImCommandSetStateDepthStencilStencilRef* p = static_cast<const ImCommandSetStateDepthStencilStencilRef*>(lpCommand);
                lpRenderer->SetState(p->mpDepthStencilState, p->muStencilRef); break;
            }
            case 6: lpRenderer->SetState(static_cast<const ImCommandSetStateRasterizer*>(lpCommand)->mpRasterizerState); break;
            case 7: lpRenderer->SetState(static_cast<const ImCommandSetStateRenderTarget*>(lpCommand)->mpRenderTargetState); break;
            case 8: lpRenderer->SetState(static_cast<const ImCommandSetStateSampler*>(lpCommand)->mpSamplerState); break;
            case 9: lpRenderer->SetState(static_cast<const ImCommandSetStateTexture*>(lpCommand)->mpTextureState); break;
            case 10: lpRenderer->SetTexture(static_cast<const ImCommandSetTexture*>(lpCommand)->mpTexture); break;
            case 11:
            {
                const ImCommandSetViewport* p = static_cast<const ImCommandSetViewport*>(lpCommand);
                D3DVIEWPORTF9 v;
                v.X = static_cast<float>(p->mu32StartPosX); v.Y = static_cast<float>(p->mu32StartPosY);
                v.Width = static_cast<float>(p->mu32Width); v.Height = static_cast<float>(p->mu32Height);
                v.MinZ = 0.0f; v.MaxZ = 1.0f; v.Flags = 0;
                D3DDevice_SetViewportF(gpD3DDevice, &v); break;
            }
            case 12:
            {
                const ImCommandSetScissor* p = static_cast<const ImCommandSetScissor*>(lpCommand);
                RECT r; r.left = p->mu32StartPosX; r.top = p->mu32StartPosY;
                r.right = p->mu32StartPosX + p->mu32Width; r.bottom = p->mu32StartPosY + p->mu32Height;
                D3DDevice_SetScissorRect(gpD3DDevice, &r); break;
            }
            case 13:
                renderengine::Device::Clear(static_cast<const ImCommandSetClear*>(lpCommand)->mClearParams,
                    renderengine::RenderTargetState::TARGETID_COLOR_ALL); break;
            case 14:
            {
                const ImCommandResolve* p = static_cast<const ImCommandResolve*>(lpCommand);
                if (p->mbResolve) p->mpPixelBuffer->Xbox2ResolveTo(p->mpTexture, 0, 0, 0, 0, 0, 0, 1.0f, 0);
                break;
            }
            case 15: lpRenderer->SetProgram(static_cast<const ImCommandSetShaderProgram*>(lpCommand)->mi8ShaderProgram); break;
            case 16: lpRenderer->SetTransform(static_cast<const ImCommandSetTransform*>(lpCommand)->mTransform); break;
            case 17:
            {
                const ImCommandPushMaskTextureState<Basic2dColouredTexturedVertex>* p = static_cast<const ImCommandPushMaskTextureState<Basic2dColouredTexturedVertex>*>(lpCommand);
                lpRenderer->PushMask(const_cast<renderengine::TextureState*>(p->mpTextureState), 0, p->mpVertices); break;
            }
            case 18:
            {
                const ImCommandPushMaskTexture<Basic2dColouredTexturedVertex>* p = static_cast<const ImCommandPushMaskTexture<Basic2dColouredTexturedVertex>*>(lpCommand);
                lpRenderer->PushMask(0, p->mpTexture, p->mpVertices); break;
            }
            case 19: lpRenderer->PopMask(); break;
            case 20:
            {
                const ImCommandSetTransformBasic* p = static_cast<const ImCommandSetTransformBasic*>(lpCommand);
                Im2dTransform t;
                t.mOriginXYZ.Set(p->mOrigin.mV.x, p->mOrigin.mV.y, p->mfZ, 0.0f);
                t.mRightUp.Set(p->mRight.mV.x, p->mRight.mV.y, p->mUp.mV.x, p->mUp.mV.y);
                t.mColourShift.SetZero(); t.mColourScale.Set(1.0f, 1.0f, 1.0f, 1.0f);
                lpRenderer->SetTransform(t); break;
            }
            case 21:
            {
                const ImCommandPushBoostBarColours* p = static_cast<const ImCommandPushBoostBarColours*>(lpCommand);
                lpRenderer->PushBoostBarColours(rw::math::vpu::Vector3(p->maColours[0].mV), rw::math::vpu::Vector3(p->maColours[1].mV)); break;
            }
            case 22:
            {
                const ImCommandBatchTransformTextureBlendRender<Basic2dColouredTexturedVertex>* p = static_cast<const ImCommandBatchTransformTextureBlendRender<Basic2dColouredTexturedVertex>*>(lpCommand);
                lpRenderer->SetTransform(p->mTransform);
                if (p->mu8Flags & 1) lpRenderer->SetTexture(p->mpTexture);
                if (p->mu8Flags & 2) lpRenderer->SetState(p->mpBlendState);
                lpRenderer->Render(p->mePrimitiveType, p->mpVertices, p->muNumVertices); break;
            }
            default: break;
            }
            CGS_ASSERT(luPos < mpDispatchBuffer->muCommandBufferWritePos,
                "luCurrentCommandPos >= 0 && luCurrentCommandPos < mpDispatchBuffer->muCommandBufferWritePos");
            luPos += lpCommand->muSize;
        }
    }

    // ARTIST8244FF30 emits the same opcode-16/80-byte record as the
    // generic buffer writer, on the base subobject at this+4.
    void Im2dRenderBuffer::SetTransform(const Im2dTransform& transform)
    {
        Im2dColouredTexturedRenderBuffer::SetTransform(transform);
    }

    // ARTIST824501C8: the command buffer is the base subobject at this+4.
    void Im2dRenderBuffer::PopMask()
    {
        const char* lpFile = "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBuffer.h";
        if (!mbInRenderBlock)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Command called outside of a BeginRendering/EndRendering block", lpFile, 1216);
            CgsDev::Assert::EndAssert();
        }
        __dcbz128(static_cast<int>(mpWriteBuffer->muCommandBufferWritePos + 128u),
                  mpWriteBuffer->mpu8CommandBuffer);
        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        const u32 luNext = luPos + 16u;
        if (luNext <= muCommandBufferSize)
        {
            u32* lpCommand = reinterpret_cast<u32*>(mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand[1] = 16;
            lpCommand[0] = 19;
            mpWriteBuffer->muCommandBufferWritePos = luNext;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
            if (!mbFailGracefully)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("ImRenderBuffer command buffer is full", lpFile, 1254);
                CgsDev::Assert::EndAssert();
            }
        }
    }

    void Im2dRenderBuffer::BatchTransformTextureBlendRenderStatic(
        const Im2dTransform& lrTransform, renderengine::Texture* lpTexture,
        const renderengine::BlendState* lpBlendState, renderengine::PrimitiveType lePrimitiveType,
        const Basic2dColouredTexturedVertex* lpVertices, u32 luNumVertices, u8 lu8Flags)
    {
        const char* lpFile = "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBuffer.h";
        if (!mbInRenderBlock)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Command called outside of a BeginRendering/EndRendering block", lpFile, 1216);
            CgsDev::Assert::EndAssert();
        }
        __dcbz128(static_cast<int>(mpWriteBuffer->muCommandBufferWritePos + 128u), mpWriteBuffer->mpu8CommandBuffer);
        const u32 pos = mpWriteBuffer->muCommandBufferWritePos;
        const u32 next = pos + 112u;
        if (next <= muCommandBufferSize)
        {
            ImCommandBatchTransformTextureBlendRender<Basic2dColouredTexturedVertex>* p =
                reinterpret_cast<ImCommandBatchTransformTextureBlendRender<Basic2dColouredTexturedVertex>*>(mpWriteBuffer->mpu8CommandBuffer + pos);
            p->muSize = 112u; p->muType = IM_CMD_BATCH_TRANSFORM_TEXTURE_BLEND_RENDER;
            mpWriteBuffer->muCommandBufferWritePos = next;
            p->mTransform = lrTransform; p->mpTexture = lpTexture; p->mpBlendState = lpBlendState;
            p->mePrimitiveType = lePrimitiveType; p->mpVertices = lpVertices;
            p->muNumVertices = luNumVertices; p->mu8Flags = lu8Flags;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
            if (!mbFailGracefully)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("ImRenderBuffer command buffer is full", lpFile, 1254);
                CgsDev::Assert::EndAssert();
            }
        }
    }

    // ARTIST8246F638: append the command before reserving two vertices.
    void Im2dRenderBuffer::PushMask(renderengine::Texture* lpTexture,
        const Basic2dColouredTexturedVertex* lpaLogicalCorners)
    {
        const char* lpFile = "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBuffer.h";
        if (!mbInRenderBlock)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Command called outside of a BeginRendering/EndRendering block", lpFile, 1216);
            CgsDev::Assert::EndAssert();
        }
        __dcbz128(static_cast<int>(mpWriteBuffer->muCommandBufferWritePos + 128u),
                  mpWriteBuffer->mpu8CommandBuffer);
        const u32 luCommandPos = mpWriteBuffer->muCommandBufferWritePos;
        const u32 luCommandNext = luCommandPos + 16u;
        u32* lpCommand = 0;
        if (luCommandNext <= muCommandBufferSize)
        {
            lpCommand = reinterpret_cast<u32*>(mpWriteBuffer->mpu8CommandBuffer + luCommandPos);
            lpCommand[1] = 16;
            lpCommand[0] = 18;
            mpWriteBuffer->muCommandBufferWritePos = luCommandNext;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
            if (!mbFailGracefully)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("ImRenderBuffer command buffer is full", lpFile, 1254);
                CgsDev::Assert::EndAssert();
            }
        }
        const u32 luVertexNext = mpWriteBuffer->muVertexBufferWritePos + 40u;
        Basic2dColouredTexturedVertex* lpVertices = 0;
        if (luVertexNext <= muVertexBufferSize)
        {
            __dcbz128(128, mpWriteBuffer->mpu8VertexBuffer + mpWriteBuffer->muVertexBufferWritePos);
            lpVertices = reinterpret_cast<Basic2dColouredTexturedVertex*>(
                mpWriteBuffer->mpu8VertexBuffer + mpWriteBuffer->muVertexBufferWritePos);
            mpWriteBuffer->muVertexBufferWritePos = luVertexNext;
        }
        if (!lpVertices)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Im2dRenderBuffer: Push Mask, we have run out of vertex buffer space...",
                "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/ImRenderBuffer/CgsIm2dRenderBuffer.h", 200);
            CgsDev::Assert::EndAssert();
        }
        if (lpCommand && lpVertices)
        {
            lpCommand[2] = static_cast<u32>(reinterpret_cast<uintptr_t>(lpTexture));
            lpCommand[3] = static_cast<u32>(reinterpret_cast<uintptr_t>(lpVertices));
            const u32* lpSource = reinterpret_cast<const u32*>(lpaLogicalCorners);
            u32* lpDestination = reinterpret_cast<u32*>(lpVertices);
            for (u32 i = 0; i < 10; ++i)
                lpDestination[i] = lpSource[i];
        }
    }

    // -------------------------------------------------------------------------
    // PushMask @0x82450030 - push a clip mask built from a 2-vertex screen-space
    // corner run bound to lpTextureState. The X360 body appends the 16-byte
    // {muType=17} command FIRST (or rewinds on command-buffer overflow, leaving the
    // command slot null), then carves the 2-vertex (40-byte) corner run from the
    // vertex stream (the inline equivalent of AllocVertices(2); a vertex overflow
    // leaves the run null), and only when BOTH the command slot and the run are
    // valid stamps the state @ +8, the run pointer @ +12, and copies the two
    // corners into the run. The +8 slot is the DWARF :191 TextureState binding --
    // the one X360 caller, BrnGui::SetMaskRect @0x82450BE0, passes a resolved
    // renderengine::TextureState* (see the header note).
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::PushMask(const renderengine::TextureState* lpTextureState,
                                     const V* lpaMaskVertices)
    {
        // RECORD SIZE: console literal 16 (two 4-byte payload words); both payload members
        // are POINTERS, so the x64 record is 24 and the reserved stride 32. Same widening
        // trap as PushMaskGeometry - see ImCommandRecord in CgsImRenderBufferTemplate.h.
        const u32 luRecord = ImCommandRecord<ImCommandPushMaskTextureState<V> >::KU_BYTES;

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        ImCommandPushMaskTextureState<V>* lpCommand = 0;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            lpCommand = reinterpret_cast<ImCommandPushMaskTextureState<V>*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = luRecord;
            lpCommand->muType = IM_CMD_PUSH_MASK;                             // 17
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
        if (lpCommand != 0 && lpRun != 0)
        {
            lpCommand->mpTextureState = lpTextureState;
            lpCommand->mpVertices     = lpRun;
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
        const u32 luRecord = ImCommandRecord<ImCommandPopMask>::KU_BYTES;   // 16, as on console

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommand* lpCommand = reinterpret_cast<ImCommand*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = luRecord;
            lpCommand->muType = IM_CMD_END_MASK;                             // 19 (== pop/clear mask)
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // -------------------------------------------------------------------------
    // Shared command writer for the boost-bar colour pair: append a 48-byte
    // {muType=21, muSize=48} record carrying two 16-byte vectors at +16/+32.
    // The derived ARTIST API at 0x824502A8 takes Vector3 values in V1/V2 and
    // forwards their complete lanes here; current host callers keep Vector4 refs.
    // -------------------------------------------------------------------------
    template <typename V>
    void ImRenderBuffer<V>::PushBoostBarColours(const rw::math::vpu::Vector4& lrColour0,
                                                const rw::math::vpu::Vector4& lrColour1)
    {
        CGS_STATIC_ASSERT((ImCommandRecord<ImCommandPushBoostBarColours>::KU_BYTES == 48u),
                      "console record size 48");
        const u32 luRecord = ImCommandRecord<ImCommandPushBoostBarColours>::KU_BYTES;

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommandPushBoostBarColours* lpCommand = reinterpret_cast<ImCommandPushBoostBarColours*>(
                mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muSize = luRecord;
            lpCommand->muType = IM_CMD_PUSH_BOOST_BAR_COLOURS;               // 21
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
            lpCommand->maColours[0] = lrColour0;   // v1 -> +16
            lpCommand->maColours[1] = lrColour1;   // v2 -> +32
        }
        else
        {
            SetBufferFullRewindToLastEndRender();
        }
    }

    // ARTIST 0x824502A8 takes Vector3 values in V1/V2, copies both complete
    // 128-bit registers, and stores them at +16/+32 in the 48-byte command. The
    // host Vector3 is the same 16-byte VPU storage (its W lane is unused, but the
    // console copies it); preserve every lane through the existing typed writer.
    void Im2dRenderBuffer::PushBoostBarColours(rw::math::vpu::Vector3 lrColour0,
                                               rw::math::vpu::Vector3 lrColour1)
    {
        CGS_STATIC_ASSERT((sizeof(rw::math::vpu::Vector3) == 16u),
                      "X360 PushBoostBarColours copies one 16-byte VPU register per colour");
        const u32 luRecord = ImCommandRecord<ImCommandPushBoostBarColours>::KU_BYTES;
        if (!mbInRenderBlock)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Command called outside of a BeginRendering/EndRendering block",
                "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBuffer.h",
                1216);
            CgsDev::Assert::EndAssert();
        }

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        const bool lbCommandBufferFull = luPos + luRecord > muCommandBufferSize;
        const rw::math::vpu::Vector4 lrPayload0(lrColour0.mV);
        const rw::math::vpu::Vector4 lrPayload1(lrColour1.mV);
        Im2dColouredTexturedRenderBuffer::PushBoostBarColours(lrPayload0, lrPayload1);

        if (lbCommandBufferFull && !mbFailGracefully)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "ImRenderBuffer command buffer is full",
                "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBuffer.h",
                1254);
            CgsDev::Assert::EndAssert();
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
        // RECORD SIZE: console literal 112. Three of the six trailing payload members are
        // POINTERS, so the x64 record is 120 and the reserved stride 128 - transcribing 112
        // would let the vertex count + flag byte spill into the next record. See
        // ImCommandRecord in CgsImRenderBufferTemplate.h.
        const u32 luRecord = ImCommandRecord<ImCommandBatchTransformTextureBlendRender<V> >::KU_BYTES;

        const u32 luPos = mpWriteBuffer->muCommandBufferWritePos;
        if (muCommandBufferSize >= luPos + luRecord)
        {
            ImCommandBatchTransformTextureBlendRender<V>* lpCommand =
                reinterpret_cast<ImCommandBatchTransformTextureBlendRender<V>*>(
                    mpWriteBuffer->mpu8CommandBuffer + luPos);
            lpCommand->muType = IM_CMD_BATCH_TRANSFORM_TEXTURE_BLEND_RENDER;  // 22
            lpCommand->muSize = luRecord;
            mpWriteBuffer->muCommandBufferWritePos = luPos + luRecord;
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
    // TU owns are instantiated. Generic members are instantiated individually in
    // CgsImRenderBufferTemplate.cpp; no whole-struct instantiation crosses TU bodies.
    // -------------------------------------------------------------------------
    template void ImRenderBuffer<Basic2dColouredTexturedVertex>::PushMask(
        const renderengine::TextureState*, const Basic2dColouredTexturedVertex*);
    template void ImRenderBuffer<Basic2dColouredTexturedVertex>::PopMask();
    template void ImRenderBuffer<Basic2dColouredTexturedVertex>::PushBoostBarColours(
        const rw::math::vpu::Vector4&, const rw::math::vpu::Vector4&);
    template void ImRenderBuffer<Basic2dColouredTexturedVertex>::BatchTransformTextureBlendRenderStatic(
        const Im2dTransform&, renderengine::Texture*, const renderengine::BlendState*,
        renderengine::PrimitiveType, const Basic2dColouredTexturedVertex*, u32, u8);
}
