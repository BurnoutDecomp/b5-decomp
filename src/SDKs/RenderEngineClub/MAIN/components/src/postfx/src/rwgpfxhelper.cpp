#include "types.hpp"

#include <cmath>    // cosf / sinf / fabsf
#include <cstring>  // memset
#include <cstdint>  // intptr_t

#include "rw/rwcore_structs.h"  // rw::IResourceAllocator / rw::ResourceDescriptor / rw::Resource
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::PfxHelper::CreateProgram                 @ 0x823FE480
//   rw::graphics::postfx::PfxHelper::CreateStates                  @ 0x82402D88
//   rw::graphics::postfx::PfxHelper::InitWeights_Blur16WithBilinear@ 0x823F8E78
//   rw::graphics::postfx::PfxHelper::InitWeights_Blur16            @ 0x823F8F50
//   rw::graphics::postfx::PfxHelper::InitWeights_DirBlur9Quadratic @ 0x823F8C70
//
// PfxHelper is the post-fx singleton: it owns the full-screen-quad geometry, the opaque-blend /
// depth-stencil render states, and the blur/copy shader programs every effect compiles against.
// CreateProgram is the shader-program factory the post-fx effects call directly; CreateStates builds
// the shared opaque blend state. Both follow the standard render-engine idiom: build a parameters
// block -> GetResourceDescriptor sizes it -> the allocator's DoAllocate carves the storage ->
// Initialize constructs the runtime object in place.
//
// The InitWeights_* generators are pure float math that fill a caller-owned vec4 weight table; every
// literal below is the immediate the asm loads. The flt_82001C98 / flt_82001CC0 constants are
// 1.0 / 0.0 (the same pair the sibling DepthOfField TU documents).
//
// NOT IN THIS PASS: the PfxHelper constructor (0x82408348) and Release (0x82402E58) build/tear down
// the geometry + state resources through a wide set of renderengine vendor callees
// (VertexDescriptor / VertexBuffer + VertexBufferHelper::Lock/Unlock / MeshHelper /
// DepthStencilState and the SafeRelease<> template) whose parameter structs and signatures are not
// yet reconstructed in the committed tree. They are declared on the class but intentionally left
// undefined here until those callee headers exist, to avoid fabricating vendor types.

namespace
{
    // The shared post-fx singleton (off_82FAEE80). CreateProgram reaches for it when no explicit
    // allocator override is supplied. Set by the constructor; cleared by Release.
    rw::graphics::postfx::PfxHelper* gpPfxHelperSingleton = nullptr;

    // The renderengine allocator keeps its dispatch pointer as the first word; the helper stores that
    // word (mpAllocator) and routes DoAllocate through it.
    rw::Resource AllocateResource(rw::IResourceAllocator* lpAllocator,
                                  const rw::BaseResourceDescriptors<5>& lrDescriptor)
    {
        return lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lrDescriptor), nullptr);
    }
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // X360 0x823FE480.
    renderengine::ProgramBufferData* PfxHelper::CreateProgram(s32 leType, const void* lpMicrocode,
                                                              u32 luSize, rw::IResourceAllocator* lpReserved)
    {
        // When no allocator override is given (every call site passes 0) the asm dereferences the
        // singleton and reads its first word -- the allocator the helper itself was built with.
        rw::IResourceAllocator* lpAllocator = lpReserved;
        if (lpAllocator == nullptr)
        {
            lpAllocator = *reinterpret_cast<rw::IResourceAllocator**>(gpPfxHelperSingleton);
        }

        // Build the program parameters: function = microcode blob, shader-type = leType, the size in
        // the third word; the remaining words start zeroed.
        renderengine::ProgramBufferParameters lParams;
        std::memset(&lParams, 0, sizeof(lParams));
        lParams.muFunction   = static_cast<u32>(reinterpret_cast<uintptr_t>(lpMicrocode));
        lParams.muShaderType = static_cast<u32>(leType);
        lParams.muReserved8  = luSize;

        rw::BaseResourceDescriptors<5> lDescriptor;
        renderengine::ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lParams);

        rw::Resource lResource = AllocateResource(lpAllocator, lDescriptor);
        return renderengine::ProgramBuffer::Initialize(
            reinterpret_cast<renderengine::ProgramResourceLayout*>(&lResource), &lParams);
    }

    // X360 0x82402D88.
    s32 PfxHelper::CreateStates()
    {
        // Opaque-blend material parameters. The four leading words are the per-channel blend factors
        // (0x07060706 each, from the asm's lis 0x706 / ori 0x706 build), then the channel-write masks
        // (0xF x4), the blend op (7), the flag word (0x87 == 135), and a -1 word; the boolean tail and
        // the remaining state words are zero.
        renderengine::BlendStateParameters lParams;
        std::memset(&lParams, 0, sizeof(lParams));
        lParams.maBlendFactor[0] = 0x07060706u;  // 117835526
        lParams.maBlendFactor[1] = 0x07060706u;
        lParams.maBlendFactor[2] = 0x07060706u;
        lParams.maBlendFactor[3] = 0x07060706u;
        // Set by which params WORD the asm writes (0x82402DC0..0x82402DE8), NOT by the
        // Hex-Rays v5[] index names: blendstate.h orders word4=muState15 .. word11=muState9.
        lParams.muState15        = 7;            // word4 = v5[4]
        lParams.muState4         = 15;           // word5 = v5[5]
        lParams.muState5         = 15;           // word6 = v5[6]
        lParams.muState6         = 15;           // word7 = v5[7]
        lParams.muState7         = 15;           // word8 = v5[8]
        lParams.muState8         = 135;          // word9 = v5[9] = 0x87
        lParams.muState17        = 0;            // word10 = v5[10]
        lParams.muState9         = static_cast<u32>(-1); // word11 = v5[11]

        rw::BaseResourceDescriptors<5> lDescriptor;
        renderengine::BlendState::GetResourceDescriptor(&lDescriptor);

        rw::Resource lResource = AllocateResource(mpAllocator, lDescriptor);
        void* lpState = renderengine::BlendState::Initialize(
            reinterpret_cast<renderengine::BlendMaterialState**>(&lResource), &lParams);
        mpOpaqueBlendState = lpState;
        return static_cast<s32>(reinterpret_cast<intptr_t>(lpState));
    }

    // X360 0x823F8E78. 16-tap (4x4) box blur with bilinear sampling: for each of the 4x4 sample
    // positions emit (offsetX, offsetY, 0.25, 1.0). offsetX/offsetY step by 2 (bilinear pairs).
    void PfxHelper::InitWeights_Blur16WithBilinear(f32* lpBlurWeights, s32 liWidth, s32 liHeight)
    {
        const f32 lfInvWidth  = 1.0f / static_cast<f32>(liWidth);
        const f32 lfInvHeight = 1.0f / static_cast<f32>(liHeight);

        f32* lpOut = lpBlurWeights;
        for (s32 liY = 0; liY < 4; liY += 2)
        {
            const f32 lfOffsetY = static_cast<f32>(liY - 1) * lfInvHeight;
            for (s32 liX = 0; liX < 4; liX += 2)
            {
                const f32 lfOffsetX = static_cast<f32>(liX - 1) * lfInvWidth;
                lpOut[0] = lfOffsetX;
                lpOut[1] = lfOffsetY;
                lpOut[2] = 0.25f;
                lpOut[3] = 1.0f;
                lpOut += 4;
            }
        }
    }

    // X360 0x823F8F50. 16-tap (4x4) box blur. Each of the four rows emits four samples at x-offsets
    // {-1.5, -0.5, 0.5, 1.5}/width and a constant y-offset of (row+0.5)/height, with a flat 0.0625
    // weight in lane 2. A second pass normalises those 16 weights so they sum to 1.
    void PfxHelper::InitWeights_Blur16(f32* lpBlurWeights, s32 liWidth, s32 liHeight)
    {
        static const f32 KAF_OFFSETS_X[4] = { -1.5f, -0.5f, 0.5f, 1.5f };

        const f32 lfInvWidth  = 1.0f / static_cast<f32>(liWidth);
        const f32 lfInvHeight = 1.0f / static_cast<f32>(liHeight);

        f32 lfTotalWeight = 0.0f;
        f32* lpOut = lpBlurWeights;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            const f32 lfOffsetY = (static_cast<f32>(liRow) + 0.5f) * lfInvHeight;
            for (s32 liCol = 0; liCol < 4; ++liCol)
            {
                lpOut[0] = lfInvWidth * KAF_OFFSETS_X[liCol];
                lpOut[1] = lfOffsetY;
                lpOut[2] = 0.0625f;
                lfTotalWeight += 0.0625f;
                lpOut += 4;
            }
        }

        // Normalise the weight lane (lane 2 of each vec4) by the accumulated total.
        const f32 lfInvTotal = 1.0f / lfTotalWeight;
        for (s32 liSample = 0; liSample < 16; ++liSample)
        {
            lpBlurWeights[liSample * 4 + 2] *= lfInvTotal;
        }
    }

    // X360 0x823F8C70. 9-tap directional blur with a quadratic falloff. The 9 taps step along
    // (cos angle, sin angle); each tap is centred about index 4 and quantised in 2/9 (0.22222222)
    // steps. The weight is (1 - d^2)/radius + 0.05 (d == normalised step). The weight lane is then
    // normalised by the accumulated total and scaled by weightFactor.
    void PfxHelper::InitWeights_DirBlur9Quadratic(f32* lpBlurWeights, s32 liWidth, s32 liHeight,
                                                  f32 lfAngle, f32 lfRadius, f32 lfWeightFactor)
    {
        const f32 lfInvWidth  = 1.0f / static_cast<f32>(liWidth);
        const f32 lfInvHeight = 1.0f / static_cast<f32>(liHeight);

        // XMVectorCos / XMVectorSin splat the scalar cos/sin of the angle.
        f32 lfCos = cosf(lfAngle);
        f32 lfSin = sinf(lfAngle);
        // Snap near-axis directions to the axis (asm: |cos| < 0.001 -> 0, |sin| > 0.999 -> 1).
        if (fabsf(lfCos) < 0.001f)
            lfCos = 0.0f;
        if (fabsf(lfSin) > 0.99900001f)
            lfSin = 1.0f;

        const f32 lfInvRadius = 1.0f / lfRadius;

        f32 lfTotalWeight = 0.0f;
        f32* lpOut = lpBlurWeights;
        for (s32 liSample = 0; liSample < 9; ++liSample)
        {
            const f32 lfStep = (static_cast<f32>(liSample) - 4.0f) * lfRadius * 0.22222222f;
            const f32 lfNorm = lfStep * lfInvRadius;
            const f32 lfWeight = -((lfNorm * lfNorm) - 1.0f);

            // asm 0x823F8D90-DA8: lane0 = step*cos*(1/width), lane1 = step*sin*(1/height).
            lpOut[0] = (lfStep * lfCos) * lfInvWidth;
            lpOut[1] = (lfStep * lfSin) * lfInvHeight;
            lpOut[2] = lfWeight * lfInvRadius + 0.050000001f;
            lfTotalWeight += lpOut[2];
            lpOut += 4;
        }

        // Normalise the weight lane and fold in weightFactor.
        const f32 lfInvTotal = 1.0f / lfTotalWeight;
        for (s32 liSample = 0; liSample < 9; ++liSample)
        {
            lpBlurWeights[liSample * 4 + 2] = lpBlurWeights[liSample * 4 + 2] * lfInvTotal * lfWeightFactor;
        }
    }
}
}
}
