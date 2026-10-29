#pragma once

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredVertex.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"   // renderengine::ProgramVariableHandle
#include "rw/math/vpu/types.h"                                                 // rw::math::vpu::Vector3 / Vector4

namespace CgsGraphics
{
    extern s32 gLastIm2dMaskRect[4];

    template <typename V>
    struct Im2dBase : public ImRenderer<V>
    {
        void Construct(rw::IResourceAllocator* lpAllocator,
                       const void* const* lapVertexProgramBinary, const u32* lauVertexProgramSize,
                       const void* const* lapPixelProgramBinary, const u32* lauPixelProgramSize,
                       s8 li8NumberPrograms);
        void SetTransform(const Im2dTransform& lTransform);

    protected:
        renderengine::ProgramVariableHandle maHandleOffsetXYZ[KI8_MAX_PROGRAMS];
        renderengine::ProgramVariableHandle maHandleRightUp[KI8_MAX_PROGRAMS];
        renderengine::ProgramVariableHandle maHandleColourShift[KI8_MAX_PROGRAMS];
        renderengine::ProgramVariableHandle maHandleColourScale[KI8_MAX_PROGRAMS];
        Im2dTransform mCurrentTransform;
    };

    struct Im2dUntex : public Im2dBase<Basic2dColouredVertex>
    {
        void Construct(rw::IResourceAllocator* lpAllocator);
    };

    struct Im2d : public Im2dBase<Basic2dColouredTexturedVertex>
    {
        void Construct(rw::IResourceAllocator* lpAllocator);
        virtual bool SetProgram(s8 li8Program);

        static const u32 KU_IM2D_MAX_MASK_COUNT = 2;
        static const u32 KU_IM2D_NUM_MASK_PROGRAMS = 4;

        struct Im2dMask
        {
            struct Im2dMaskLayer
            {
                renderengine::ProgramVariableHandle mPixelMaskPositionMinMax;
                renderengine::ProgramVariableHandle mPixelMaskUVStartEnd;
                renderengine::ProgramVariableHandle mPixelMaskUVDifference;
                rw::math::vpu::Vector4                             mvPixelMaskPositionMinMax;
                rw::math::vpu::Vector4                             mvPixelMaskUVStartEnd;
                rw::math::vpu::Vector4                             mvPixelMaskUVDifference;
                renderengine::TextureState*         mpMaskTextureState;
                renderengine::Texture*              mpMaskTexture;
            };

            Im2dMaskLayer                       maPixelMaskHandles[KU_IM2D_MAX_MASK_COUNT];
            rw::math::vpu::Vector4                             mvPixelMask;
            renderengine::ProgramVariableHandle mPixelMaskUseHandle;
        };

        void PushMask(renderengine::TextureState* lpMaskTextureState,
                      renderengine::Texture* lpMaskTexture,
                      const Basic2dColouredTexturedVertex* lpaMaskVertices);
        void PopMask();
        void PushBoostBarColours(rw::math::vpu::Vector3 lvOuterColour, rw::math::vpu::Vector3 lvInnerColour);

    protected:
        void SaveMaskShaderConstants(const Basic2dColouredTexturedVertex* lpaMaskVertices,
                                     renderengine::TextureState* lpMaskTextureState,
                                     renderengine::Texture* lpMaskTexture);
        void SetMaskPixelShaderState();

        Im2dMask                            maMask[KU_IM2D_NUM_MASK_PROGRAMS];
        u32                                 mu32NumMasks;
        renderengine::ProgramVariableHandle maBoostBarOuterColours[2];
        renderengine::ProgramVariableHandle maBoostBarInnerColours[2];
    };
}
