from pathlib import Path
import hashlib
import json
import struct
import sys

root = Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/rendercore-material-526adc')
out = Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-suncorona-a449')
out.mkdir(exist_ok=True)
sys.path.insert(0, r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/tools/re')
import x360rd

paths = ['src/GameSource/Graphics/BrnSunCorona.h', 'src/GameSource/Graphics/BrnSunCorona.cpp']
before = {}
for name in paths:
    p = root / name
    backup = out / ('original-' + p.name)
    assert not backup.exists(), 'Refusing to overwrite preserved original source'
    backup.write_bytes(p.read_bytes())
    before[name] = hashlib.sha256(p.read_bytes()).hexdigest()

high = x360rd.u32(0x82400b10) & 0xffff
low = x360rd.u32(0x82400b1c) & 0xffff
if low & 0x8000:
    low -= 0x10000
file_address = (high << 16) + low
filename = bytearray()
while True:
    c = x360rd.rd(file_address + len(filename), 1)[0]
    if not c:
        break
    filename.append(c)
file_literal = json.dumps(filename.decode('ascii'))

header = '''#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/rwcore_structs.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include <cstddef>

class CgsRenderTarget;

// Native ARTIST824009B0 construction and DWARF BrnSunCorona.h:38.
struct BrnSunCorona
{
    void Construct(rw::IResourceAllocator* lpAllocator);
    void Destruct();
    void ComputeSunPositionOnScreen(Matrix44 lViewProjectionMatrix,
                                    Vector3 lViewPosition, Vector3 lSunDir);
    void GenerateOcclusionBuffer(CgsRenderTarget* lpOcclusionRt,
                                 CgsRenderTarget* lpSourceDepthRt);
    void RenderOccludedFlare(CgsRenderTarget* lpOcclusionRt, Vector3 lSunColour,
                             f32 lfWhiteLevel, f32 lfDebugOverrideBrightness,
                             bool lbDebugOverrideBrightness);

    rw::IResourceAllocator* mpAllocator;                                  // 00
    rw::Resource mVertexDescriptorResource;                               // 04
    renderengine::VertexDescriptorData* mpVertexDescriptor;               // 18
    renderengine::ProgramBufferData* mpOcclusionVertexProgram;             // 1C
    renderengine::ProgramBufferData* mpOcclusionPixelProgram;              // 20
    renderengine::ProgramVariableHandle mOcclusionPixelVariableHandleUvStartOffset; // 24
    renderengine::ProgramBufferData* mpFlareVertexProgram;                 // 28
    renderengine::ProgramBufferData* mpFlarePixelProgram;                  // 2C
    renderengine::ProgramVariableHandle mFlarePixelVariableHandleColourAndPower; // 30
    f32 mfSunVectorYMultiplier;                                          // 34
    f32 mfOcclusionSize;                                                 // 38
    f32 mfSunFlarePow;                                                   // 3C
    f32 mfSunBrightness;                                                 // 40
    f32 mfSunSize;                                                       // 44
    f32 mfXPos;                                                          // 48
    f32 mfYPos;                                                          // 4C
    bool mbVisible;                                                      // 50
    bool mbRenderSunCorona;                                             // 51
};

CGS_STATIC_ASSERT((sizeof(BrnSunCorona) == 0x54), "ARTIST sun corona");
CGS_STATIC_ASSERT((offsetof(BrnSunCorona, mpVertexDescriptor) == 0x18), "ARTIST vertex descriptor");
CGS_STATIC_ASSERT((offsetof(BrnSunCorona, mFlarePixelVariableHandleColourAndPower) == 0x30), "ARTIST flare handle");
CGS_STATIC_ASSERT((offsetof(BrnSunCorona, mbRenderSunCorona) == 0x51), "ARTIST render flag");
'''

source = '''#include "GameSource/Graphics/BrnSunCorona.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include <vectorintrinsics.h>

namespace
{
'''
source += 'const char gszSourceFile[] = ' + file_literal + ';\n\n'
blob_manifest = []
for name, address, size in [
    ('gaOcclusionVertexProgram', 0x8203e118, 204),
    ('gaOcclusionPixelProgram', 0x8203e208, 524),
    ('gaFlareVertexProgram', 0x8203e438, 240),
    ('gaFlarePixelProgram', 0x8203e528, 464)
]:
    data = x360rd.rd(address, size)
    assert any(data) and len(data) == size
    words = struct.unpack('>' + str(size // 4) + 'I', data)
    source += '// ARTIST %08X, %d bytes.\nconst u32 %s[] =\n{\n' % (address, size, name)
    for index in range(0, len(words), 6):
        source += '    ' + ', '.join('0x%08Xu' % word for word in words[index:index + 6]) + ',\n'
    source += '};\n\n'
    blob_manifest.append(dict(name=name, address=hex(address), size=size, sha256=hashlib.sha256(data).hexdigest()))

source += '''void AssertSunCorona(bool lbCondition, const char* lpExpression, s32 liLine)
{
    if (!lbCondition)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lpExpression, gszSourceFile, liLine);
        CgsDev::Assert::EndAssert();
    }
}

renderengine::ProgramBufferData* CreateProgram(rw::IResourceAllocator* lpAllocator,
    const u32* lpBinary, u32 luType, u32 luSize)
{
    renderengine::ProgramBufferParameters lParameters;
    lParameters.muFunction = reinterpret_cast<u32>(lpBinary);
    lParameters.muShaderType = luType;
    lParameters.muReserved8 = luSize;
    lParameters.muMicrocodePart1 = 0;
    lParameters.muMicrocodePart3 = 0;
    lParameters.muConstantTableSize = 0;
    lParameters.muNumVariables = 0;

    rw::ResourceDescriptor lDescriptor;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lParameters);
    rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
    // ARTIST82400B0C/B98/C30/CB0 passes the actual five-lane resource.
    // ProgramBuffer::Initialize consumes its existing lane-zero/lane-two view.
    return renderengine::ProgramBuffer::Initialize(
        reinterpret_cast<renderengine::ProgramResourceLayout*>(&lResource), &lParameters);
}

struct SunCoronaVertex
{
    f32 mafPosition[3];
    f32 mafUv[2];
};
CGS_STATIC_ASSERT((sizeof(SunCoronaVertex) == 20), "ARTIST sun vertex stride");

void WriteVertex(SunCoronaVertex& lrVertex, f32 x, f32 y, f32 u, f32 v)
{
    lrVertex.mafPosition[0] = x;
    lrVertex.mafPosition[1] = y;
    lrVertex.mafPosition[2] = 0.0f;
    lrVertex.mafUv[0] = u;
    lrVertex.mafUv[1] = v;
}

f32 FirstLane(__vector4 lValue)
{
    __declspec(align(16)) f32 lafLanes[4];
    __stvx(lValue, lafLanes, 0);
    return lafLanes[0];
}
}

// ARTIST824009B0.
void BrnSunCorona::Construct(rw::IResourceAllocator* lpAllocator)
{
    mfXPos = 0.0f;
    mfYPos = 0.0f;
    mpAllocator = lpAllocator;
    mbVisible = false;
    mbRenderSunCorona = true;
    mfSunVectorYMultiplier = 0.5f;
    mfOcclusionSize = 2.0f;
    mfSunFlarePow = 2.0f;
    mfSunBrightness = 0.30000001192092896f;
    mfSunSize = 0.30000001192092896f;

    renderengine::VertexDescriptor::Parameters lVertexParameters;
    lVertexParameters.maElements[0].mu16Stream = 0;
    lVertexParameters.maElements[0].miOffset = 0x002A23B9;
    lVertexParameters.maElements[0].mu8UsageIndex = 1;
    lVertexParameters.maElements[1].mu16Stream = 0;
    lVertexParameters.maElements[1].miOffset = 0x002C23A5;
    lVertexParameters.maElements[1].mu8UsageIndex = 6;
    rw::ResourceDescriptor lVertexDescriptor;
    renderengine::VertexDescriptor::GetResourceDescriptor(&lVertexDescriptor, &lVertexParameters);
    mVertexDescriptorResource = lpAllocator->DoAllocate(lVertexDescriptor, 0);
    mpVertexDescriptor = renderengine::VertexDescriptor::Initialize(&mVertexDescriptorResource, &lVertexParameters);

    mpOcclusionVertexProgram = CreateProgram(lpAllocator, gaOcclusionVertexProgram, 0, sizeof(gaOcclusionVertexProgram));
    AssertSunCorona(mpOcclusionVertexProgram != 0, "NULL != mpOcclusionVertexProgram", 97);
    mpOcclusionPixelProgram = CreateProgram(lpAllocator, gaOcclusionPixelProgram, 1, sizeof(gaOcclusionPixelProgram));
    AssertSunCorona(mpOcclusionPixelProgram != 0, "NULL != mpOcclusionPixelProgram", 109);
    renderengine::ProgramBuffer::GetVariableHandleByName(mpOcclusionPixelProgram,
        reinterpret_cast<const u8*>("kUvStartAndOffset"), &mOcclusionPixelVariableHandleUvStartOffset);

    mpFlareVertexProgram = CreateProgram(lpAllocator, gaFlareVertexProgram, 0, sizeof(gaFlareVertexProgram));
    AssertSunCorona(mpFlareVertexProgram != 0, "NULL != mpFlareVertexProgram", 132);
    mpFlarePixelProgram = CreateProgram(lpAllocator, gaFlarePixelProgram, 1, sizeof(gaFlarePixelProgram));
    AssertSunCorona(mpFlarePixelProgram != 0, "NULL != mpFlarePixelProgram", 144);
    renderengine::ProgramBuffer::GetVariableHandleByName(mpFlarePixelProgram,
        reinterpret_cast<const u8*>("kColourAndPower"), &mFlarePixelVariableHandleColourAndPower);
}

// ARTIST823F8298. Only the descriptor resource is released here.
void BrnSunCorona::Destruct()
{
    AssertSunCorona(mpAllocator != 0, "mpAllocator", 170);
    renderengine::VertexDescriptor::Release(mpVertexDescriptor);
    mpAllocator->DoFree(mVertexDescriptorResource);
}

// ARTIST823FD798. Preserve the native VMX estimates, refinement and fused math.
void BrnSunCorona::ComputeSunPositionOnScreen(Matrix44 lViewProjectionMatrix,
    Vector3 lViewPosition, Vector3 lSunDir)
{
    const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
    const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
    const __vector4 lMultiplier = { mfSunVectorYMultiplier, 0.0f, 0.0f, 0.0f };
    __vector4 lLocalSunDir = __lvx(&lSunDir, 0);
    lLocalSunDir = __vrlimi(lLocalSunDir,
        __vmulfp(__vspltw(lLocalSunDir, 1), __vspltw(lMultiplier, 0)), 4, 0);
    const __vector4 lLengthSquared = __vmsum3fp(lLocalSunDir, lLocalSunDir);
    __vector4 lInverseLength = __vrsqrtefp(lLengthSquared);
    __vector4 lSquared = __vmulfp(lInverseLength, lInverseLength);
    __vector4 lHalfInverse = __vmulfp(lInverseLength, lHalf);
    lInverseLength = __vmaddfp(lHalfInverse, __vnmsubfp(lLengthSquared, lSquared, lOne), lInverseLength);
    lSquared = __vmulfp(lInverseLength, lInverseLength);
    lHalfInverse = __vmulfp(lInverseLength, lHalf);
    lInverseLength = __vmaddfp(lHalfInverse, __vnmsubfp(lLengthSquared, lSquared, lOne), lInverseLength);
    const __vector4 lDistance = { -10000.0f, -10000.0f, -10000.0f, -10000.0f };
    __vector4 lSunPosition = __vmaddfp(__vmulfp(lLocalSunDir, lInverseLength), lDistance, __lvx(&lViewPosition, 0));
    lSunPosition = __vrlimi(lSunPosition, lOne, 1, 0);

    __vector4 lProjected = __vmulfp(__lvx(&lViewProjectionMatrix, 0), __vspltw(lSunPosition, 0));
    lProjected = __vmaddfp(__lvx(&lViewProjectionMatrix, 16), __vspltw(lSunPosition, 1), lProjected);
    lProjected = __vmaddfp(__lvx(&lViewProjectionMatrix, 32), __vspltw(lSunPosition, 2), lProjected);
    lProjected = __vmaddfp(__lvx(&lViewProjectionMatrix, 48), __vspltw(lSunPosition, 3), lProjected);
    const __vector4 lW = __vspltw(lProjected, 3);
    __vector4 lInverseX = __vrefp(lW);
    __vector4 lInverseY = __vrefp(lW);
    lInverseX = __vmaddfp(lInverseX, __vnmsubfp(lInverseX, lW, lOne), lInverseX);
    lInverseY = __vmaddfp(lInverseY, __vnmsubfp(lInverseY, lW, lOne), lInverseY);
    lInverseX = __vmaddfp(lInverseX, __vnmsubfp(lInverseX, lW, lOne), lInverseX);
    lInverseY = __vmaddfp(lInverseY, __vnmsubfp(lInverseY, lW, lOne), lInverseY);
    const f32 lfZNonProjected = FirstLane(__vspltw(lProjected, 2));
    const f32 lfX = FirstLane(__vmulfp(lInverseX, __vspltw(lProjected, 0)));
    const f32 lfY = FirstLane(__vmulfp(lInverseY, __vspltw(lProjected, 1)));
    mfXPos = lfX * 0.5f + 0.5f;
    mfYPos = lfY * -0.5f + 0.5f;
    mbVisible = lfZNonProjected > 0.0f;
}

// ARTIST82400CF8.
void BrnSunCorona::GenerateOcclusionBuffer(CgsRenderTarget* lpOcclusionRt,
    CgsRenderTarget* lpSourceDepthRt)
{
    AssertSunCorona(lpOcclusionRt != 0, "lpOcclusionRt != NULL", 250);
    AssertSunCorona(lpSourceDepthRt != 0, "lpSourceDepthRt != NULL", 251);
    if (!mbRenderSunCorona)
        return;
    rw::graphics::postfx::RenderTarget* const lpDepthRenderTarget = lpSourceDepthRt->GetRenderTarget();
    AssertSunCorona(lpDepthRenderTarget != 0, "lpDepthRenderTarget", 260);
    lpOcclusionRt->Begin();
    shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF));
    shadow::Device::SetState(CgsRasterizerStateFactory::GetState(E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE));
    shadow::Device::SetState(CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA));
    shadow::Device::SetVertexProgram(mpOcclusionVertexProgram);
    shadow::Device::SetPixelProgram(mpOcclusionPixelProgram);
    const f32 lfStepU = (1.0f / static_cast<f32>(lpSourceDepthRt->GetWidth())) * mfOcclusionSize;
    const f32 lfStepV = (1.0f / static_cast<f32>(lpSourceDepthRt->GetHeight())) * mfOcclusionSize;
    const f32 lfStartU = mfXPos;
    const f32 lfStartV = mfYPos;
    renderengine::FloatShaderStateIterator lIterator;
    renderengine::Device::BeginShaderStates(mOcclusionPixelVariableHandleUvStartOffset, lIterator);
    lIterator.m_dest->x = lfStartU;
    lIterator.m_dest->y = lfStartV;
    lIterator.m_dest->z = lfStepU;
    lIterator.m_dest->w = lfStepV;
    shadow::Device::SetState(lpDepthRenderTarget->mDepthTarget.mpTextureState, 0);
    shadow::Device::SetVertexDescriptor(mpVertexDescriptor);
    shadow::Device::FlushVertexProgramState();
    D3DDevice* const lpDevice = ::gpD3DDevice;
    SunCoronaVertex* const lpVertex = static_cast<SunCoronaVertex*>(
        D3DDevice_BeginVertices(lpDevice, D3DPT_TRIANGLESTRIP, 4, sizeof(SunCoronaVertex)));
    WriteVertex(lpVertex[0], -1.0f, -1.0f, 0.0f, 0.0f);
    WriteVertex(lpVertex[1], 1.0f, -1.0f, 0.0f, 0.0f);
    WriteVertex(lpVertex[2], -1.0f, 1.0f, 0.0f, 0.0f);
    WriteVertex(lpVertex[3], 1.0f, 1.0f, 0.0f, 0.0f);
    D3DDevice_EndVertices(lpDevice);
    lpOcclusionRt->End();
}

// ARTIST824010A8. The current target has already been opened by the caller.
void BrnSunCorona::RenderOccludedFlare(CgsRenderTarget* lpOcclusionRt,
    Vector3 lSunColour, f32 lfWhiteLevel, f32 lfDebugOverrideBrightness,
    bool lbDebugOverrideBrightness)
{
    AssertSunCorona(lpOcclusionRt != 0, "lpOcclusionRt != NULL", 369);
    if (!mbVisible || !mbRenderSunCorona)
        return;
    rw::graphics::postfx::RenderTarget* const lpOcclusionRenderTarget = lpOcclusionRt->GetRenderTarget();
    AssertSunCorona(lpOcclusionRenderTarget != 0, "lpOcclusionRenderTarget", 378);
    shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF));
    shadow::Device::SetState(CgsRasterizerStateFactory::GetState(E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE));
    shadow::Device::SetState(CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_RGB_NO_ALPHA_TEST_DEST_RGB));
    shadow::Device::SetVertexProgram(mpFlareVertexProgram);
    shadow::Device::SetPixelProgram(mpFlarePixelProgram);
    const f32 lfBrightness = lbDebugOverrideBrightness ? lfDebugOverrideBrightness : mfSunBrightness;
    const __vector4 lWhite = { lfWhiteLevel, lfWhiteLevel, lfWhiteLevel, lfWhiteLevel };
    const __vector4 lBrightness = { lfBrightness, lfBrightness, lfBrightness, lfBrightness };
    const __vector4 lColour = __vmulfp(__vmulfp(__lvx(&lSunColour, 0), lWhite), lBrightness);
    const f32 lfRed = FirstLane(__vspltw(lColour, 0));
    const f32 lfGreen = FirstLane(__vspltw(lColour, 1));
    const f32 lfBlue = FirstLane(__vspltw(lColour, 2));
    const f32 lfPower = mfSunFlarePow;
    renderengine::FloatShaderStateIterator lIterator;
    renderengine::Device::BeginShaderStates(mFlarePixelVariableHandleColourAndPower, lIterator);
    lIterator.m_dest->x = lfRed;
    lIterator.m_dest->y = lfGreen;
    lIterator.m_dest->z = lfBlue;
    lIterator.m_dest->w = lfPower;
    shadow::Device::SetState(lpOcclusionRenderTarget->maColourTargets[0].mpTextureState, 0);
    shadow::Device::SetVertexDescriptor(mpVertexDescriptor);
    shadow::Device::FlushVertexProgramState();
    const f32 lfLeft = (mfXPos - mfSunSize) * 2.0f - 1.0f;
    const f32 lfRight = (mfXPos + mfSunSize) * 2.0f - 1.0f;
    const f32 lfHalfHeight = mfSunSize * 1.7777777910232544f;
    const f32 lfTop = ((1.0f - mfYPos) + lfHalfHeight) * 2.0f - 1.0f;
    const f32 lfBottom = ((1.0f - mfYPos) - lfHalfHeight) * 2.0f - 1.0f;
    D3DDevice* const lpDevice = ::gpD3DDevice;
    SunCoronaVertex* const lpVertex = static_cast<SunCoronaVertex*>(
        D3DDevice_BeginVertices(lpDevice, D3DPT_TRIANGLESTRIP, 4, sizeof(SunCoronaVertex)));
    WriteVertex(lpVertex[0], lfLeft, lfTop, -1.0f, 1.0f);
    WriteVertex(lpVertex[1], lfRight, lfTop, 1.0f, 1.0f);
    WriteVertex(lpVertex[2], lfLeft, lfBottom, -1.0f, -1.0f);
    WriteVertex(lpVertex[3], lfRight, lfBottom, 1.0f, -1.0f);
    D3DDevice_EndVertices(lpDevice);
    lpOcclusionRt->End();
}
'''

(root / paths[0]).write_text(header)
(root / paths[1]).write_text(source)
(out / 'recovery-inputs.json').write_text(json.dumps(dict(original_source_sha256=before,
    original_source_file_address=hex(file_address), source_file=filename.decode('ascii'),
    shader_blobs=blob_manifest), indent=2) + '\n')
print('Wrote native SunCorona candidate; original shader blobs:', blob_manifest)
