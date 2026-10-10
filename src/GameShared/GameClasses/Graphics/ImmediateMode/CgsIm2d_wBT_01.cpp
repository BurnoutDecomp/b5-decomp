// CgsIm2d_wBT_01.cpp -- the console Im2d's shader-driven pixel mask and boost-bar colour
// constants (CgsGraphics::Im2d), plus the two stage-indexed ImRendererBase binders the mask
// upload reaches. CgsIm2d.cpp holds the PC 2D fold of the shared Im2dBase / ImRenderer API.
//
// Layout of the mask state (CgsIm2d.h): maMask[4] holds one Im2dMask per program pair -- the
// masked variant of pair k is program 2k+1 and reads maMask[k]. Each Im2dMask carries two layers
// (V_IM2D_MAX_MASK_COUNT nesting), each with its rectangle {minX, minY, maxX, maxY}, its UV
// rectangle {u0, v0, u1, v1}, its reciprocal extents {1/(maxX-minX), 1/(maxY-minY), 0, 0} and
// the texture / texture state that holds the mask shape. mvPixelMask has lane i set to 1 while
// layer i is live; mPixelMaskUseHandle uploads it.

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"   // shadow::Device::SetState / SetResource
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "BrnCommonTypes.h"                                           // Vector3 / Vector4

#include <cstring>

// renderengine::Device::BeginShaderStates (pc/gcm/renderengine/ImmediateMode.cpp): open the
// constant row a program-variable handle names and return the write cursor.
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

namespace CgsGraphics
{
namespace
{
    // The console writes one lane of a vector member through the whole-register round trip:
    // spill the 16 bytes to a stack union, overwrite one float, reload, store all 16 back.
    void SetVectorLane(Vector4& lrVector, u32 luLane, f32 lfValue)
    {
        f32 lafLanes[4];
        std::memcpy(lafLanes, &lrVector, sizeof(lafLanes));
        lafLanes[luLane] = lfValue;
        std::memcpy(&lrVector, lafLanes, sizeof(lafLanes));
    }

    // Open the constant row lrHandle names and store one 16-byte vector into it.
    void UploadVectorConstant(renderengine::ProgramVariableHandle& lrHandle, const void* lpValue)
    {
        void* lpConstant = nullptr;
        RenderEngineDeviceBeginShaderStates(&lrHandle, &lpConstant);
        std::memcpy(lpConstant, lpValue, 16);
    }
}

// -------------------------------------------------------------------------------------------------
// ImRendererBase::SetState(const TextureState*, u32) / SetTexture(Texture*, u32)
// The stage-indexed binders: assert this renderer is the active one, then bind through the
// shadow device at the given sampler unit.
// -------------------------------------------------------------------------------------------------
void ImRendererBase::SetState(const renderengine::TextureState* lpTextureState, u32 luStage)
{
    CGS_ASSERT(mgpActiveRenderer == this, "mgpActiveRenderer == this");
    shadow::Device::SetState(lpTextureState, luStage);
}

void ImRendererBase::SetTexture(renderengine::Texture* lpTexture, u32 luStage)
{
    CGS_ASSERT(mgpActiveRenderer == this, "mgpActiveRenderer == this");
    shadow::Device::SetResource(lpTexture, luStage);
}

// -------------------------------------------------------------------------------------------------
// Im2d::SaveMaskShaderConstants
// Record the new layer (index mu32NumMasks) into every program pair's mask block and mark lanes
// 0..mu32NumMasks of each block's mvPixelMask live. lpaMaskVertices is the {min, max} corner pair.
// -------------------------------------------------------------------------------------------------
void Im2d::SaveMaskShaderConstants(const Basic2dColouredTexturedVertex* lpaMaskVertices,
                                   renderengine::TextureState* lpMaskTextureState,
                                   renderengine::Texture* lpMaskTexture)
{
    const Basic2dColouredTexturedVertex& lrMin = lpaMaskVertices[0];
    const Basic2dColouredTexturedVertex& lrMax = lpaMaskVertices[1];

    const Vector4 lvMinPosition = { lrMin.mv2Pos.x, lrMin.mv2Pos.y, 0.0f, 0.0f };
    const Vector4 lvMaxPosition = { lrMax.mv2Pos.x, lrMax.mv2Pos.y, 0.0f, 0.0f };

    CGS_ASSERT(mu32NumMasks < KU_IM2D_MAX_MASK_COUNT, "mu32NumMasks < V_IM2D_MAX_MASK_COUNT");

    const Vector4 lvExtent = { lvMaxPosition.x - lvMinPosition.x, lvMaxPosition.y - lvMinPosition.y,
                               lvMaxPosition.z - lvMinPosition.z, lvMaxPosition.w - lvMinPosition.w };
    const Vector4 lvReciprocalExtent = { 1.0f / lvExtent.x, 1.0f / lvExtent.y, 0.0f, 0.0f };

    for (u32 luMask = 0; luMask < KU_IM2D_NUM_MASK_PROGRAMS; ++luMask)
    {
        Im2dMask& lrMask = maMask[luMask];
        Im2dMask::Im2dMaskLayer& lrLayer = lrMask.maPixelMaskHandles[mu32NumMasks];

        const Vector4 lvPositionMinMax = { lvMinPosition.x, lvMinPosition.y, lvMaxPosition.x, lvMaxPosition.y };
        const Vector4 lvUVStartEnd     = { lrMin.mv2Tex0UV.x, lrMin.mv2Tex0UV.y, lrMax.mv2Tex0UV.x, lrMax.mv2Tex0UV.y };

        lrLayer.mvPixelMaskPositionMinMax = lvPositionMinMax;
        lrLayer.mvPixelMaskUVStartEnd     = lvUVStartEnd;
        lrLayer.mvPixelMaskUVDifference   = lvReciprocalExtent;
        lrLayer.mpMaskTextureState        = lpMaskTextureState;
        lrLayer.mpMaskTexture             = lpMaskTexture;

        for (u32 luLane = 0; luLane < mu32NumMasks + 1; ++luLane)
        {
            SetVectorLane(lrMask.mvPixelMask, luLane, 1.0f);
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Im2d::SetMaskPixelShaderState
// Upload the top layer of the current program pair's mask block (rectangle, UVs, reciprocal
// extents, live-lane mask) and bind its mask texture state -- or, without one, its bare texture --
// at sampler unit mu32NumMasks.
// -------------------------------------------------------------------------------------------------
void Im2d::SetMaskPixelShaderState()
{
    CGS_ASSERT(mu32NumMasks > 0, "mu32NumMasks > 0");

    Im2dMask& lrMask = maMask[static_cast<s8>((mi8CurrentProgram - 1) / 2)];
    Im2dMask::Im2dMaskLayer& lrLayer = lrMask.maPixelMaskHandles[mu32NumMasks - 1];

    UploadVectorConstant(lrLayer.mPixelMaskPositionMinMax, &lrLayer.mvPixelMaskPositionMinMax);
    UploadVectorConstant(lrLayer.mPixelMaskUVStartEnd,     &lrLayer.mvPixelMaskUVStartEnd);
    UploadVectorConstant(lrLayer.mPixelMaskUVDifference,   &lrLayer.mvPixelMaskUVDifference);
    UploadVectorConstant(lrMask.mPixelMaskUseHandle,       &lrMask.mvPixelMask);

    if (lrLayer.mpMaskTextureState != nullptr)
    {
        SetState(lrLayer.mpMaskTextureState, mu32NumMasks);
    }
    else if (lrLayer.mpMaskTexture != nullptr)
    {
        SetTexture(lrLayer.mpMaskTexture, mu32NumMasks);
    }
}

// -------------------------------------------------------------------------------------------------
// Im2d::PushMask
// The first layer switches to the masked variant of the current program (slot + 1) and, when that
// bind took, re-uploads the current transform to it. Then record and upload the new layer.
// -------------------------------------------------------------------------------------------------
void Im2d::PushMask(renderengine::TextureState* lpMaskTextureState,
                    renderengine::Texture* lpMaskTexture,
                    const Basic2dColouredTexturedVertex* lpaMaskVertices)
{
    CGS_ASSERT(mu32NumMasks < KU_IM2D_MAX_MASK_COUNT, "mu32NumMasks < V_IM2D_MAX_MASK_COUNT");

    if (mu32NumMasks == 0
        && ImRenderer<Basic2dColouredTexturedVertex>::SetProgram(static_cast<s8>(mi8CurrentProgram + 1)))
    {
        ImRenderer<Basic2dColouredTexturedVertex>::SetTransform(&mCurrentTransform);
    }

    SaveMaskShaderConstants(lpaMaskVertices, lpMaskTextureState, lpMaskTexture);
    ++mu32NumMasks;
    SetMaskPixelShaderState();
}

// -------------------------------------------------------------------------------------------------
// Im2d::PopMask
// Drop the top layer: clear its live lane in every mask block, re-upload the current pair's lane
// mask, and on the last pop return to the unmasked variant (slot - 1) and re-upload the transform.
// -------------------------------------------------------------------------------------------------
void Im2d::PopMask()
{
    CGS_ASSERT(mu32NumMasks, "mu32NumMasks");

    --mu32NumMasks;
    for (u32 luMask = 0; luMask < KU_IM2D_NUM_MASK_PROGRAMS; ++luMask)
    {
        SetVectorLane(maMask[luMask].mvPixelMask, mu32NumMasks, 0.0f);
    }

    Im2dMask& lrMask = maMask[static_cast<s8>((mi8CurrentProgram - 1) / 2)];
    UploadVectorConstant(lrMask.mPixelMaskUseHandle, &lrMask.mvPixelMask);

    if (mu32NumMasks == 0
        && ImRenderer<Basic2dColouredTexturedVertex>::SetProgram(static_cast<s8>(mi8CurrentProgram - 1)))
    {
        ImRenderer<Basic2dColouredTexturedVertex>::SetTransform(&mCurrentTransform);
    }
}

// -------------------------------------------------------------------------------------------------
// Im2d::PushBoostBarColours
// Upload the outer then the inner colour to each of the two program slots' boost-bar constants.
// -------------------------------------------------------------------------------------------------
void Im2d::PushBoostBarColours(Vector3 lvOuterColour, Vector3 lvInnerColour)
{
    for (u32 luSlot = 0; luSlot < 2; ++luSlot)
    {
        UploadVectorConstant(maBoostBarOuterColours[luSlot], &lvOuterColour);
        UploadVectorConstant(maBoostBarInnerColours[luSlot], &lvInnerColour);
    }
}

} // namespace CgsGraphics
