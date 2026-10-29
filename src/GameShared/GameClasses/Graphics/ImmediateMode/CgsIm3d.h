#pragma once

#include "BrnCommonTypes.h"  // Matrix44 (rw::math::vpu::Matrix44)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"            // CgsGraphics::ImRenderer<V>
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredVertex.h"  // CgsGraphics::BasicColouredVertex
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"  // CgsGraphics::BasicColouredTexturedVertex (Im3d)
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsPositionOnlyVertex.h" // CgsGraphics::PositionOnlyVertex
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // renderengine::ProgramVariableHandle
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"

namespace CgsGraphics
{
    template <typename V>
    struct Im3dBase : public ImRenderer<V>
    {
        void SetTransform(Matrix44 lTransform);
        void SetTransform(Matrix44 lModelToWorld, Matrix44 lViewProjection);

        renderengine::ProgramVariableHandle maWorldViewProjStateHandle[KI8_MAX_PROGRAMS];
        Matrix44 mCurrentTransform;
    };

    struct Im3dUntex : public Im3dBase<BasicColouredVertex>
    {
        void Construct(rw::IResourceAllocator* lpAllocator);
    };

    struct Im3dZOnly : public Im3dBase<PositionOnlyVertex>
    {
        void Construct(rw::IResourceAllocator* lpAllocator);
    };

    struct Im3d : public Im3dBase<BasicColouredTexturedVertex>
    {
        static const u32 KU_MAX_MASK_COUNT = 2;

        void Construct(rw::IResourceAllocator* lpAllocator);

        struct Im3dMask
        {
            struct Im3dMaskLayer
            {
                renderengine::ProgramVariableHandle mPixelMaskPositionMinMax;
                renderengine::ProgramVariableHandle mPixelMaskUVStartEnd;
                renderengine::ProgramVariableHandle mPixelMaskUVDifference;
                Vector4 mvPixelMaskPositionMinMax;
                Vector4 mvPixelMaskUVStartEnd;
                Vector4 mvPixelMaskUVDifference;
                renderengine::TextureState* mpMaskTextureState;
                renderengine::Texture* mpMaskTexture;
            };
            Im3dMaskLayer maPixelMaskHandles[2];
            renderengine::ProgramVariableHandle mPixelMaskUseHandle;
            Vector4 mvPixelMask;
        };
        Im3dMask maMask[1];
        virtual bool SetProgram(s8 li8Program);

        void PushMask(renderengine::TextureState* lpTextureState, renderengine::Texture* lpTexture,
                      const BasicColouredTexturedVertex* lpVertices);

        void SaveMaskShaderConstants(const BasicColouredTexturedVertex* lpVertices,
                                     renderengine::TextureState* lpTextureState, renderengine::Texture* lpTexture);
        void SetMaskPixelShaderState();

        u32 mu32NumMasks;
    };

}
