#pragma once

#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsMaterialTechnique.h"

// ARTIST vtables @820D6B68/78/88 establish all four slots, including the
// otherwise unnamed SteppedLoopAnimation::OnCalculateOffset @827F0290.
// Declaration shape: DecFIGS CgsShaderConstants.h and CgsMaterialAnimation.h.
class ICPUShader
{
public:
    void Attach(const CgsGraphics::MaterialAssembly* lpMaterial) { OnAttach(lpMaterial); }
    virtual u32 GetNumConstantInstances() const = 0;
    virtual void Execute(f32 lfTime, const CgsGraphics::MaterialAssembly* lpMaterial,
                         CgsGraphics::MaterialTechnique* lpTechnique) const = 0;
protected:
    virtual void OnAttach(const CgsGraphics::MaterialAssembly* lpMaterial) = 0;
    void SetShaderConstant(const CgsGraphics::MaterialAssembly* lpMaterial,
                           CgsGraphics::MaterialTechnique* lpTechnique,
                           const u32& luNameHash, const Vector4& lrValue) const;
};

class UVOffsetAnimationShader : public ICPUShader
{
public:
    u32 GetNumConstantInstances() const override { return 1; } // ICF @82C296C8
    void Execute(f32 lfTime, const CgsGraphics::MaterialAssembly* lpMaterial,
                 CgsGraphics::MaterialTechnique* lpTechnique) const override;
protected:
    void OnAttach(const CgsGraphics::MaterialAssembly*) override {} // ICF @8284CB38
    virtual void OnCalculateOffset(const CgsGraphics::MaterialAssembly* lpMaterial,
                                    f32 lfTime, Vector4& lrOffset) const = 0;
};

class SmoothLoopAnimation : public UVOffsetAnimationShader
{
protected:
    void OnCalculateOffset(const CgsGraphics::MaterialAssembly* lpMaterial,
                            f32 lfTime, Vector4& lrOffset) const override;
};
class SteppedLoopAnimation : public UVOffsetAnimationShader
{
protected:
    void OnCalculateOffset(const CgsGraphics::MaterialAssembly* lpMaterial,
                            f32 lfTime, Vector4& lrOffset) const override;
};
class SteppedGridLoopAnimation : public UVOffsetAnimationShader
{
protected:
    void OnCalculateOffset(const CgsGraphics::MaterialAssembly* lpMaterial,
                            f32 lfTime, Vector4& lrOffset) const override;
};

class MaterialAnimationFactory
{
public:
    static MaterialAnimationFactory& Instance();
    ICPUShader* Create(f32 lfPeriod, f32 lfStepsU, f32 lfStepsV);
private:
    SmoothLoopAnimation mSmoothLoopShader;
    SteppedLoopAnimation mSteppedLoopShader;
    SteppedGridLoopAnimation mSteppedGridLoopShader;
};
