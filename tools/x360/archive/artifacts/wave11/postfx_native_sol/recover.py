from pathlib import Path
import re,json,struct,hashlib,sys
root=Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root')
p=root/'src/GameSource/Graphics/PostFx/BrnPostFxShader.cpp'
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave11/postfx_native_sol')
s=p.read_text(encoding='utf-8');(out/'BrnPostFxShader.pre.cpp').write_text(s,encoding='utf-8')
# Retain active native math and state construction while removing the PC availability branches.
s=re.sub(r'//[^\n]*','',s)
lines=[];active=[True];conditions=[]
for l in s.splitlines():
 if l.startswith('#define BRN_POSTFX_'):continue
 if l.startswith('#if '):
  cond=not l.startswith('#if !'); conditions.append(cond);active.append(active[-1] and cond);continue
 if l.startswith('#else'):
  active[-1]=active[-2] and not conditions[-1];continue
 if l.startswith('#endif'):
  active.pop();conditions.pop();continue
 if active[-1]:lines.append(l.rstrip())
s='\n'.join(lines)+'\n'
s=s.replace('#include "GameShared/GameClasses/Development/Log/CgsLog.h"','').replace('#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"','')
s=re.sub(r'namespace renderengine\s*\{.*?\}\s*extern "C".*?(?=namespace\s*\{)', '', s, flags=re.S)
s=re.sub(r'    void ReportOnce\(.*?\n    }\n', '', s, flags=re.S)
s=re.sub(r'    rw::math::vpu::Vector4 MultiplyPerLane\(.*?\n    }\n', '', s, flags=re.S)
s=s.replace('MultiplyPerLane(', 'rw::math::vpu::Mult(')
s=re.sub(r'    u32 MotionBlurSamplerMaxAnisotropy\(.*?\n    }\n', '', s, flags=re.S)
# Native program creation, matching823FD970: no adoption, null guard, PS3 register writes or fallback.
start=s.index('void BrnPostFxShader::Shader::Construct(');end=s.index('void BrnPostFxShader::Shader::Destruct(',start)
s=s[:start]+'''// ARTIST823FD970..823FDAFC: each slot owns two allocator-backed native program buffers.
void BrnPostFxShader::Shader::Construct(rw::IResourceAllocator* lpAllocator,
                                       const void* lpVertexProgram, u32 luVertexProgramSize,
                                       const void* lpPixelProgram, u32 luPixelProgramSize,
                                       u8 luPs3RegisterCount)
{
    renderengine::ProgramBufferParameters lVsParameters;
    lVsParameters.muFunction = reinterpret_cast<u32>(lpVertexProgram);
    lVsParameters.muShaderType = 0;
    lVsParameters.muReserved8 = luVertexProgramSize;
    lVsParameters.muMicrocodePart1 = 0;
    lVsParameters.muMicrocodePart3 = 0;
    lVsParameters.muConstantTableSize = 0;
    lVsParameters.muNumVariables = 0;
    rw::ResourceDescriptor lVsResDesc;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lVsResDesc, &lVsParameters);
    rw::Resource lVsResource = lpAllocator->DoAllocate(lVsResDesc, 0);
    mpVertexProgram = renderengine::ProgramBuffer::Initialize(&lVsResource, &lVsParameters);
    CGS_ASSERT(mpVertexProgram != 0, "NULL != mpVertexProgram");

    renderengine::ProgramBufferParameters lPsParameters;
    lPsParameters.muFunction = reinterpret_cast<u32>(lpPixelProgram);
    lPsParameters.muShaderType = 1;
    lPsParameters.muReserved8 = luPixelProgramSize;
    lPsParameters.muMicrocodePart1 = 0;
    lPsParameters.muMicrocodePart3 = 0;
    lPsParameters.muConstantTableSize = 0;
    lPsParameters.muNumVariables = 0;
    rw::ResourceDescriptor lPsResDesc;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lPsResDesc, &lPsParameters);
    rw::Resource lPsResource = lpAllocator->DoAllocate(lPsResDesc, 0);
    mpPixelProgram = renderengine::ProgramBuffer::Initialize(&lPsResource, &lPsParameters);
    CGS_ASSERT(mpPixelProgram != 0, "NULL != mpPixelProgram");

    for (u32 luVsConstant = 0; luVsConstant < E_VS_VAR_COUNT; ++luVsConstant)
        renderengine::ProgramBuffer::GetVariableHandleByName(mpVertexProgram,
            reinterpret_cast<const u8*>(gaacVertexVariableNames[luVsConstant]),
            &maVertexVariableHandle[luVsConstant]);
    for (u32 luPsConstant = 0; luPsConstant < E_PS_VAR_COUNT; ++luPsConstant)
        renderengine::ProgramBuffer::GetVariableHandleByName(mpPixelProgram,
            reinterpret_cast<const u8*>(gaacPixelVariableNames[luPsConstant]),
            &maPixelVariableHandle[luPsConstant]);
    (void)luPs3RegisterCount;
}

'''+s[end:]
start=s.index('void BrnPostFxShader::Shader::Destruct(');end=s.index('void BrnPostFxShader::Construct(',start)
s=s[:start]+'''// ARTIST823FDB08..823FDBA8: release both buffers unconditionally, then free their resources.
void BrnPostFxShader::Shader::Destruct(rw::IResourceAllocator* lpAllocator)
{
    renderengine::ProgramBuffer::Release(mpVertexProgram);
    rw::Resource lVertexProgramResource;
    lVertexProgramResource.m_baseResources[0] = mpVertexProgram;
    lpAllocator->DoFree(lVertexProgramResource);
    mpVertexProgram = 0;
    renderengine::ProgramBuffer::Release(mpPixelProgram);
    rw::Resource lPixelProgramResource;
    lPixelProgramResource.m_baseResources[0] = mpPixelProgram;
    lpAllocator->DoFree(lPixelProgramResource);
    mpPixelProgram = 0;
}

// ARTIST823FDBB0..823FDD10; DecFIGS BrnPostFxShader.cpp:249 names the two iterators.
void BrnPostFxShader::Shader::SetProgram(
    const rw::math::vpu::Vector4* lpaVertexShaderConstants,
    const rw::math::vpu::Vector4* lpaPixelShaderConstants)
{
    shadow::Device::SetVertexProgram(mpVertexProgram);
    shadow::Device::SetPixelProgram(mpPixelProgram);
    for (u32 luVsConstant = 0; luVsConstant < E_VS_VAR_COUNT; ++luVsConstant)
    {
        if (maVertexVariableHandle[luVsConstant].mu8RegisterCount)
        {
            renderengine::FloatShaderStateIterator lVsIterator;
            renderengine::Device::BeginShaderStates(maVertexVariableHandle[luVsConstant], lVsIterator);
            *lVsIterator.m_dest++ = lpaVertexShaderConstants[luVsConstant];
        }
    }
    for (u32 luPsConstant = 0; luPsConstant < E_PS_VAR_COUNT; ++luPsConstant)
    {
        if (maPixelVariableHandle[luPsConstant].mu8RegisterCount)
        {
            renderengine::FloatShaderStateIterator lPsIterator;
            renderengine::Device::BeginShaderStates(maPixelVariableHandle[luPsConstant], lPsIterator);
            *lPsIterator.m_dest++ = lpaPixelShaderConstants[luPsConstant];
        }
    }
}

'''+s[end:]
# Use the full native descriptor/resource dispatch rather than the PC three-lane Create adapter.
s=re.sub(r'    u8 lauVertexDescriptorResDesc\[144\] = {};', '    rw::ResourceDescriptor lVertexDescriptorResDesc;',s)
s=s.replace('GetResourceDescriptor(lauVertexDescriptorResDesc,','GetResourceDescriptor(&lVertexDescriptorResDesc,')
s=re.sub(r'    CgsGraphics::ResourceAllocatorCreate\(lpAllocator, &mVertexDescriptorResource,\s*lauVertexDescriptorResDesc\);','    mVertexDescriptorResource = lpAllocator->DoAllocate(lVertexDescriptorResDesc, 0);',s)
# Recover native vector calls with genuine four-lane setters/getters; Vector2 exposes its VMX register.
s=s.replace('lHalfPixelOffset.x','lHalfPixelOffset.mV.x').replace('lHalfPixelOffset.y','lHalfPixelOffset.mV.y')
s=re.sub(r'\.(x|y|z|w)\s*=\s*([^;\n]+);',lambda m:'.Set'+m[1].upper()+'('+m[2].rstrip()+');',s)
s=re.sub(r'\.(x|y|z|w)\b',lambda m:'.'+m[1].upper()+'().GetFloat()',s)
# Undo Vector2 VMX direct field conversion; it is genuinely a register.
s=s.replace('lHalfPixelOffset.mV.X().GetFloat()','lHalfPixelOffset.mV.x').replace('lHalfPixelOffset.mV.Y().GetFloat()','lHalfPixelOffset.mV.y')
s=re.sub(r'(const rw::math::vpu::Vector4 \w+) = \{ ([^}]+) };',r'\1(\2);',s)
# No PC-only duplicate sampler state variables; original binds the real state object.
s=re.sub(r'    u32 luSourceFilter[^\n]*\n','',s);s=re.sub(r'    u32 luSourceMaxAnisotropy[^\n]*\n','',s)
s=re.sub(r'        luSourceFilter[^\n]*\n','',s)
s=re.sub(r'        luSourceMaxAnisotropy =\s*MotionBlurSamplerMaxAnisotropy\([^;]+;\n','',s)
s=s.replace('    shadow::Device::SetState(lp3dTintTexture, KU_SAMPLER_TINT_3D);','    shadow::Device::SetState(lp3dTintTexture, KU_SAMPLER_TINT_3D);\n    shadow::Device::SetState(lpDepthTexture, KU_SAMPLER_DEPTH);')
s=s.replace('lpDevice, KU_COMPOSITE_PRIMITIVE_TYPE,','lpDevice, D3DPT_TRIANGLESTRIP,')
# BeginVertices has no null early-out in ARTIST82409970..82409B3C.
s=s.replace('    if (lpRing != 0)\n    {','    {')
s=s.replace('const u32 KU_COMPOSITE_PRIMITIVE_TYPE = 6;','')
s=s.replace('#include <cmath>', '#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"\n#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"\n#include "SDKs/RenderEngineClub/MAIN/components/src/states/depthstencilstate.h"\n#include <cmath>')
# Recover every package from the calibrated original image. u32 literals emit identical big-endian bytes.
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd as rd
assert rd.u32(0x82815570)==0x7c8a2378 and rd.u32(0x82f264a8)==4
vs=[0x8203F630,0x8203F0B8,0x82040180,0x8203FB40,0x82040E48,0x82040778,0x82041C50,0x820414D8,0x82042A50,0x82042380,0x82043858,0x820430E0]
ps=[0x8203F888,0x8203F310,0x820403D8,0x8203FD98,0x820410A0,0x820409D0,0x82041EA8,0x82041730,0x82042CA8,0x820425D8,0x82043AB0,0x82043338]
sizes=[696,796,928,996,1076,1140,1240,1308,1076,1140,1240,1308]
data=[];manifest=[]
for i in range(12):
 for stage,a,n in [('VS',vs[i],596),('PS',ps[i],sizes[i])]:
  b=rd.rd(a,n);name='gaPostFx'+str(i)+stage
  words=struct.unpack('>'+str(n//4)+'I',b)
  data+=['    // ARTIST0x%08X: %d bytes; Shader::Construct call823FDD18.'%(a,n),'    const u32 '+name+'[] =','    {']
  for k in range(0,len(words),8):data+=['        '+', '.join('0x%08Xu'%v for v in words[k:k+8])+',']
  data+=['    };','']
  (out/(name+'.bin')).write_bytes(b);manifest.append(dict(name=name,address=hex(a),size=n,sha256=hashlib.sha256(b).hexdigest()))
s=s.replace('namespace\n{','namespace\n{\n'+ '\n'.join(data),1)
calls='\n'.join('    maShaders[%d].Construct(lpAllocator, gaPostFx%dVS, sizeof(gaPostFx%dVS), gaPostFx%dPS, sizeof(gaPostFx%dPS), 0);'%(i,i,i,i,i) for i in [1,0,3,2,5,4,7,6,9,8,11,10])
s=s.replace('void BrnPostFxShader::Construct(rw::IResourceAllocator* lpAllocator)\n{','// ARTIST823FDD18: original permutation emission order and native state creation.\nvoid BrnPostFxShader::Construct(rw::IResourceAllocator* lpAllocator)\n{\n'+calls)
s=re.sub(r'\n\s*\n\s*\n','\n\n',s)
s='''// Native X360 post-fx composite. ARTIST is authoritative: Shader::Construct823FD970,
// Destruct823FDB08, SetProgram823FDBB0, Construct823FDD18, Destruct823FE3F0,
// Render82408F08, MotionBlurState::Update823F8490. DecFIGS supplies declaration shape.
// Shader packages below are exact native image bytes, stored as big-endian u32 words.
// Constant-name tables were read from ARTIST82043F88 (2) and82043F90 (10).
'''+s
p.write_text(s,encoding='utf-8',newline='\r\n');(out/'shader_packages.json').write_text(json.dumps(manifest,indent=2))
(out/'source_hashes.json').write_text(json.dumps({'pre':hashlib.sha256((out/'BrnPostFxShader.pre.cpp').read_bytes()).hexdigest(),'post':hashlib.sha256(p.read_bytes()).hexdigest()},indent=2))
print('wrote',len(s.splitlines()),'lines')
