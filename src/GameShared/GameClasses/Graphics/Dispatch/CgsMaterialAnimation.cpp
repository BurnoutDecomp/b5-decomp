#include "GameShared/GameClasses/Graphics/Dispatch/CgsMaterialAnimation.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdlib>
#include <new>

// Existing GPU transport: Xenos ALU constant-window writes become D3D9
// SetVertexShaderConstantF through the shared constant cache on PC.
namespace renderengine
{
    void WorldShaderConstants_Set(bool lbPixel, u32 luRegister,
                                   const void* lpData, u32 luNumRegisters);
}

namespace
{
    // ARTIST CRT thunk 82C6BE80 hashes 11 bytes at 820D8754 ("AnimPrivate")
    // through CgsHash::CalculateHash and stores the result at 83011114.
    const u32 MATERIAL_VERTEX_SHADER_CONSTANT_NAME_HASH = 0x77691959u;

}

// The original function is inlined in FixupAnimatedMaterial @827FBB10..30.
ICPUShader* MaterialAnimationFactory::Create(f32 lfPeriod, f32 lfStepsU, f32 lfStepsV)
{
    if (lfPeriod == 0.0f) return 0;
    if (lfStepsU == 0.0f) return &mSmoothLoopShader;
    if (lfStepsV == 0.0f) return &mSteppedLoopShader;
    return &mSteppedGridLoopShader;
}

MaterialAnimationFactory& MaterialAnimationFactory::Instance()
{
    return *spInstance;
}

void ShaderConstantsCPU::SetCPUShader(ICPUShader* lpShader,
                                     const CgsGraphics::MaterialAssembly* lpMaterial)
{
    mpCPUShader.muSlot = static_cast<u32>(reinterpret_cast<uintptr_t>(lpShader));
    lpShader->Attach(lpMaterial);
}

void ShaderConstantsCPU::Dispatch(f32 lfTime, const CgsGraphics::MaterialAssembly* lpMaterial,
                                 CgsGraphics::MaterialTechnique* lpTechnique) const
{
    if (mpCPUShader.Get()) mpCPUShader->Execute(lfTime, lpMaterial, lpTechnique);
}

// ARTIST 827E9740..97FC: search the technique's VERTEX bindings by the
// material's constant-name hash, then write only the matching register range.
void ICPUShader::SetShaderConstant(const CgsGraphics::MaterialAssembly* lpMaterial,
                                  CgsGraphics::MaterialTechnique* lpTechnique,
                                  const u32& luNameHash, const Vector4& lrValue) const
{
    const ShaderConstantsInternal* lpConstants = lpMaterial->GetVertexShaderConstants();
    for (u32 luIndex = 0;
         luIndex < static_cast<u8>(lpTechnique->mi8NumVertexShaderInternalConstants); ++luIndex)
    {
        const ShaderConstantHandle& lrHandle = lpTechnique->mpaVertexShaderInternalConstantsHandles[luIndex];
        if (lpConstants->mpauNamesHash[lrHandle.muValueIndex] == luNameHash)
        {
            return;
        }
    }
}

// ARTIST 827EC330: time is f1, material r5, technique r6. The virtual
// OnCalculateOffset receives material r4 and destination r6, retaining f1.
void UVOffsetAnimationShader::Execute(f32 lfTime,
                                      const CgsGraphics::MaterialAssembly* lpMaterial,
                                      CgsGraphics::MaterialTechnique* lpTechnique) const
{
    Vector4 lOffset;
    OnCalculateOffset(lpMaterial, lfTime, lOffset);
    SetShaderConstant(lpMaterial, lpTechnique, MATERIAL_VERTEX_SHADER_CONSTANT_NAME_HASH, lOffset);

    // Passive witness, once per material per half-second of render time.
}

// ARTIST 827F0210: fmod is double, then frsp before the single-precision divide.
void SmoothLoopAnimation::OnCalculateOffset(const CgsGraphics::MaterialAssembly* lpMaterial,
                                            f32 lfTime, Vector4& lrOffset) const
{
    Vector4 lValue;
    lpMaterial->GetCPUShaderConstants()->GetValue("AnimDuration", lValue);
    const f32 lfRemainder = static_cast<f32>(std::fmod(static_cast<double>(lfTime),
                                                     static_cast<double>(lValue.x)));
    lrOffset = Vector4{lfRemainder / lValue.x, 0.0f, 0.0f, 0.0f};
}

// ARTIST vtable entry 820D6B84 -> unnamed 827F0290..3338 (also DecFIGS AE07D4).
void SteppedLoopAnimation::OnCalculateOffset(const CgsGraphics::MaterialAssembly* lpMaterial,
                                             f32 lfTime, Vector4& lrOffset) const
{
    Vector4 lValue;
    const ShaderConstantsCPU* lpConstants = lpMaterial->GetCPUShaderConstants();
    lpConstants->GetValue("AnimDuration", lValue);
    const f32 lfDuration = lValue.x;
    lpConstants->GetValue("AnimNumberOfFramesU", lValue);
    const f32 lfInvSteps = 1.0f / lValue.x;
    const f32 lfRemainder = static_cast<f32>(std::fmod(static_cast<double>(lfTime),
                                                     static_cast<double>(lfDuration)));
    const f32 lfFrame = static_cast<f32>(std::floor(static_cast<double>(
        lfRemainder / (lfInvSteps * lfDuration))));
    lrOffset = Vector4{lfFrame * lfInvSteps, 0.0f, 0.0f, 0.0f};
}

// ARTIST 827F0340..431: U loops over duration; V over framesV * duration.
// Preserve fdivs/fmuls rounding at every step instead of an integer frame index.
void SteppedGridLoopAnimation::OnCalculateOffset(const CgsGraphics::MaterialAssembly* lpMaterial,
                                                 f32 lfTime, Vector4& lrOffset) const
{
    Vector4 lValue;
    const ShaderConstantsCPU* lpConstants = lpMaterial->GetCPUShaderConstants();
    lpConstants->GetValue("AnimDuration", lValue);
    const f32 lfDuration = lValue.x;
    lpConstants->GetValue("AnimNumberOfFramesU", lValue);
    const f32 lfInvU = 1.0f / lValue.x;
    lpConstants->GetValue("AnimNumberOfFramesV", lValue);
    const f32 lfFramesV = lValue.x;
    const f32 lfRemainderU = static_cast<f32>(std::fmod(static_cast<double>(lfTime),
                                                      static_cast<double>(lfDuration)));
    const f32 lfFrameU = static_cast<f32>(std::floor(static_cast<double>(
        lfRemainderU / (lfInvU * lfDuration))));
    const f32 lfOffsetU = lfFrameU * lfInvU;
    const f32 lfDurationV = lfFramesV * lfDuration;
    const f32 lfRemainderV = static_cast<f32>(std::fmod(static_cast<double>(lfTime),
                                                      static_cast<double>(lfDurationV)));
    const f32 lfInvV = 1.0f / lfFramesV;
    const f32 lfFrameV = static_cast<f32>(std::floor(static_cast<double>(
        lfRemainderV / (lfInvV * lfDurationV))));
    lrOffset = Vector4{lfOffsetU, lfFrameV * lfInvV, 0.0f, 0.0f};
}
