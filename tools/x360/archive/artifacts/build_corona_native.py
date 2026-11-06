from pathlib import Path
import sys
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\tools\re')
import x360rd
assert x360rd.u32(0x82815570)==0x7c8a2378 and x360rd.u32(0x82f264a8)==4
p=Path('src/SDKs/RenderEngineClub/MAIN/components/src/coronas/rwgcoronarenderer.cpp')
old=p.read_text(encoding='utf-8'); block=old[old.index('    u32                       CoronaRenderer::s_numSubImages'):old.index('    // =============================================================================================\n    // CoronaRenderer::Initialize')]
block=block.replace(' = {};',' = { 0, 0, 0, 0 };')
def blob(name,addr,size):
 words=[x360rd.u32(addr+i) for i in range(0,size,4)]
 return '// ARTIST 0x%08X, %d bytes.\nconst u32 %s[] =\n{\n'%(addr,size,name)+'\n'.join('    '+', '.join('0x%08Xu'%v for v in words[i:i+6])+',' for i in range(0,len(words),6))+'\n};\n'
prefix='''#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronarenderer.h"
#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include <cstring>

namespace
{
'''+blob('gaCoronaPixelProgram',0x8200f1b8,228)+blob('gaCoronaVertexProgram',0x8200f2a0,788)+'''
rw::Resource gsPixelProgramResource;
rw::Resource gsVertexProgramResource;
rw::Resource gsVertexDescriptorResource;

struct CoronaVertex
{
    f32 position[4];
    f32 direction[3];
    f32 corner[4];
    renderengine::RGBA8 colour;
};
CGS_STATIC_ASSERT((sizeof(CoronaVertex) == 48), "ARTIST corona vertex stride");
}

namespace renderengine
{
'''
body='''
void CoronaRenderer::Initialize(rw::IResourceAllocator& lrAllocator)
{
    ProgramBufferParameters lProgram;
    lProgram.muFunction = reinterpret_cast<u32>(gaCoronaPixelProgram);
    lProgram.muShaderType = 1;
    lProgram.muReserved8 = sizeof(gaCoronaPixelProgram);
    lProgram.muMicrocodePart1 = 0;
    lProgram.muMicrocodePart3 = 0;
    lProgram.muConstantTableSize = 0;
    lProgram.muNumVariables = 0;
    rw::ResourceDescriptor lDescriptor;
    ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lProgram);
    gsPixelProgramResource = lrAllocator.DoAllocate(lDescriptor, 0);
    s_pixelProgram = ProgramBuffer::Initialize(&gsPixelProgramResource, &lProgram);

    lProgram.muFunction = reinterpret_cast<u32>(gaCoronaVertexProgram);
    lProgram.muShaderType = 0;
    lProgram.muReserved8 = sizeof(gaCoronaVertexProgram);
    ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lProgram);
    gsVertexProgramResource = lrAllocator.DoAllocate(lDescriptor, 0);
    s_vertexProgram = ProgramBuffer::Initialize(&gsVertexProgramResource, &lProgram);
    ProgramBuffer::GetVariableHandleByName(s_vertexProgram, reinterpret_cast<const u8*>("viewProjectionMatrix"), &s_viewProjectionMatrixHandle);
    ProgramBuffer::GetVariableHandleByName(s_vertexProgram, reinterpret_cast<const u8*>("cameraPositionPlusBrightness"), &s_cameraPositionHandle);
    ProgramBuffer::GetVariableHandleByName(s_vertexProgram, reinterpret_cast<const u8*>("viewXyScale"), &s_viewXyScaleHandle);

    VertexDescriptor::Parameters lVertexDecl;
    lVertexDecl.maElements[0].mu16Stream = 0;
    lVertexDecl.maElements[0].miOffset = 0x1A23A6;
    lVertexDecl.maElements[0].mu8UsageIndex = 1;
    lVertexDecl.maElements[1].mu16Stream = 0;
    lVertexDecl.maElements[1].miOffset = 0x2A23B9;
    lVertexDecl.maElements[1].mu8UsageIndex = 3;
    lVertexDecl.maElements[2].mu16Stream = 0;
    lVertexDecl.maElements[2].miOffset = 0x1A23A6;
    lVertexDecl.maElements[2].mu8UsageIndex = 6;
    lVertexDecl.maElements[3].mu16Stream = 0;
    lVertexDecl.maElements[3].miOffset = 0x14C86;
    lVertexDecl.maElements[3].mu8UsageIndex = 4;
    VertexDescriptor::GetResourceDescriptor(&lDescriptor, &lVertexDecl);
    gsVertexDescriptorResource = lrAllocator.DoAllocate(lDescriptor, 0);
    s_vertexDescriptor = VertexDescriptor::Initialize(&gsVertexDescriptorResource, &lVertexDecl);
}

void CoronaRenderer::SetBlendState(const BlendMaterialState* lpState) { s_blendState = lpState; }
void CoronaRenderer::SetDepthStencilStates(const DepthStencilState* lpEnabled, const DepthStencilState* lpDisabled)
{
    s_enabledZTestState = lpEnabled;
    s_disabledZTestState = lpDisabled;
}
void CoronaRenderer::SetTextureAtlas(const TextureState* lpState, u32 luCount, const Vector2* lpUVs)
{
    s_textureState = lpState;
    s_numSubImages = luCount;
    s_atlasUVs = lpUVs;
}

void CoronaRenderer::Begin(const RenderParameters& lrParameters)
{
    s_beginLock = true;
    shadow::Device::SetState(s_textureState, 0);
    shadow::Device::SetState(s_blendState);
    shadow::Device::SetVertexProgram(s_vertexProgram);
    shadow::Device::SetPixelProgram(s_pixelProgram);
    FloatShaderStateIterator lIterator;
    Device::BeginShaderStates(s_viewProjectionMatrixHandle, lIterator);
    lIterator.m_dest[0] = lrParameters.mViewProjectionMatrix.xAxis;
    lIterator.m_dest[1] = lrParameters.mViewProjectionMatrix.yAxis;
    lIterator.m_dest[2] = lrParameters.mViewProjectionMatrix.zAxis;
    lIterator.m_dest[3] = lrParameters.mViewProjectionMatrix.wAxis;
    Device::BeginShaderStates(s_cameraPositionHandle, lIterator);
    *lIterator.m_dest = lrParameters.mvCameraPositionPlusWhiteLevel;
    Device::BeginShaderStates(s_viewXyScaleHandle, lIterator);
    *lIterator.m_dest = lrParameters.mvViewXyScale;
}
void CoronaRenderer::End() { s_beginLock = false; }

void CoronaRenderer::Dispatch(const BatchParameters& lrParameters, CoronaBuffer* lpBuffer)
{
    shadow::Device::SetState((lrParameters.muFlags & BatchParameters::FLAG_OCCLUSION_ZTEST)
                            ? s_enabledZTestState : s_disabledZTestState);
    u32 luMaxVertices = (Device::m_parameterCopy.secondaryRingBufferSize >> 2) / 48;
    if (luMaxVertices > 0xffff) luMaxVertices = 0xffff;
    const u32 luMaxCoronas = luMaxVertices >> 2;
    u32 luLeft = lrParameters.muNumCoronas;
    const Corona* lpCorona = lpBuffer->GetCoronas();
    shadow::Device::SetVertexDescriptor(s_vertexDescriptor);
    shadow::Device::FlushVertexProgramState();
    D3DDevice* const lpDevice = gpD3DDevice;
    while (luLeft)
    {
        const u32 luCount = luLeft <= luMaxCoronas ? luLeft : luMaxCoronas;
        if (192 * luCount > 0x80000) D3DDevice_InsertFence(lpDevice);
        CoronaVertex* lpVertex = static_cast<CoronaVertex*>(D3DDevice_BeginVertices(lpDevice, D3DPT_QUADLIST, 4 * luCount, 48));
        for (u32 i = 0; i < luCount; ++i, ++lpCorona)
        {
            const f32 x = lpCorona->mvSize.GetX().GetFloat();
            const f32 y = lpCorona->mvSize.GetY().GetFloat();
            const Vector2* const lpUV = s_atlasUVs + (lpCorona->miTextureID << 2);
            const f32 afX[4] = {-x, x, x, -x};
            const f32 afY[4] = {y, y, -y, -y};
            for (u32 j = 0; j < 4; ++j, ++lpVertex)
            {
                lpVertex->position[0] = lpCorona->mvPosition.GetX().GetFloat();
                lpVertex->position[1] = lpCorona->mvPosition.GetY().GetFloat();
                lpVertex->position[2] = lpCorona->mvPosition.GetZ().GetFloat();
                lpVertex->position[3] = lpCorona->mfDistance;
                lpVertex->direction[0] = lpCorona->mvDirection.GetX().GetFloat();
                lpVertex->direction[1] = lpCorona->mvDirection.GetY().GetFloat();
                lpVertex->direction[2] = lpCorona->mvDirection.GetZ().GetFloat();
                lpVertex->corner[0] = lpUV[j].GetX().GetFloat();
                lpVertex->corner[1] = lpUV[j].GetY().GetFloat();
                lpVertex->corner[2] = afX[j];
                lpVertex->corner[3] = afY[j];
                lpVertex->colour = lpCorona->muColour;
            }
        }
        D3DDevice_EndVertices(lpDevice);
        luLeft -= luCount;
    }
}
}
'''
p.write_text(prefix+block+body,encoding='utf-8',newline='\r\n')
