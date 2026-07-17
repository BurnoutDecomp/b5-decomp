#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

#include <cstdint>
#include <cstring>

// BrnFlapt::MovieClip member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU (class:BrnFlapt::MovieClip) bodies the two X360-emitted functions:
//
//   GetKeyframeForFrame    @ 0x8246B098
//   FindLabelledFrameIndex @ 0x8246B190
//
// Both originally streamed a dynamic failure message into the assert buffer via
// CgsDev::StrStreamBase (the "Tried to get frame N when num frames=M" / "Tried to
// find missing frame labelled 0xID, string=..." text). Under the project's house
// CGS_ASSERT the message is forwarded as a single static string and the streamed
// construction is semantically vacuous (the assert is advisory and only runs on
// failure); the observable behaviour — the value returned on the success path — is
// unchanged. The X360-baked BrnFlaptFile.h file/line cites are discarded per
// project convention.

namespace BrnFlapt
{

FlaptFile* FlaptFile::mpFlaptFile = 0;

namespace
{
    template <class T>
    void FixPointer(T*& lrpPointer, uintptr_t luBase)
    {
        lrpPointer = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lrpPointer) + luBase);
    }

    template <class T>
    void FixPointerIfPresent(T*& lrpPointer, uintptr_t luBase)
    {
        if (lrpPointer != 0)
            FixPointer(lrpPointer, luBase);
    }

    template <class T>
    void FixPointerDown(T*& lrpPointer, uintptr_t luBase)
    {
        lrpPointer = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(lrpPointer) - luBase);
    }

    template <class T>
    void FixPointerDownIfPresent(T*& lrpPointer, uintptr_t luBase)
    {
        if (lrpPointer != 0)
            FixPointerDown(lrpPointer, luBase);
    }
}

// ---- GetKeyframeForFrame @ 0x8246B098 ------------------------------------
// Bound-check the requested frame against muNumFramesInTimeline (lhz 8(this)), then map it
// through the optional keyframe-remap table: if present, return the u16
// entry mpauFrameToKeyFrameMap[luFrame] (lhzx, zero-extended); otherwise the frame
// index passes through unchanged.
u32 MovieClip::GetKeyframeForFrame(u32 luFrame) const
{
    CGS_ASSERT(luFrame < muNumFramesInTimeline,
        "luFrame < muNumFramesInTimeline");

    if (mpauFrameToKeyFrameMap != 0)
    {
        return mpauFrameToKeyFrameMap[luFrame];
    }
    return luFrame;
}

// ---- FindLabelledFrameIndex @ 0x8246B190 ---------------------------------
// Linear-search the label table at +0x38 (8-byte MovieClipLabel entries) for an
// entry whose id (first dword) equals luLabelId, scanning up to mu8NumLabels
// (lbz 5(this)) entries. On a hit, return the matching frame id from the parallel
// u16 table mpau16LabelledFrameIds at +0x3C (lhzx, zero-extended). On miss — or if
// there are no labels at all — assert and return -1.
//
// The X360 re-reads mu8NumLabels (lbz 5(this)) on every loop iteration and asserts
// each visited entry pointer is non-null; reproduced as the natural index scan.
s32 MovieClip::FindLabelledFrameIndex(u32 luLabelId, const char* lpcLabelText) const
{
    (void)lpcLabelText;   // only consumed by the (advisory) failure message

    if (muNumLabelledFrames != 0)
    {
        u32 luIndex = 0;
        while (true)
        {
            CGS_ASSERT(&mpaFrameLabels[luIndex] != 0, "lpLabel");

            if (mpaFrameLabels[luIndex].muHash == luLabelId)
            {
                CGS_ASSERT(mpauLabelledFrameIds != 0, "mpauLabelledFrameIds");
                return mpauLabelledFrameIds[luIndex];
            }

            ++luIndex;
            if (luIndex >= muNumLabelledFrames)
            {
                break;
            }
        }
    }

    CGS_ASSERT(false,
        "Tried to find missing frame labelled ");
    return -1;
}

// ---- MovieClip::FixUp @ 0x82470D70 --------------------------------------
// Relocate the complete authored subgraph. MovieClip::mpFile is serialised as
// zero and deliberately fixes to the image base (the owning FlaptFile).
void MovieClip::FixUp(uintptr_t luBase)
{
    mpFile = reinterpret_cast<FlaptFile*>(luBase);
    FixPointerIfPresent(mpauFrameToKeyFrameMap, luBase);
    FixPointer(mpaRenderLayers, luBase);
    FixPointer(mpaKeyFrames, luBase);
    FixPointer(mpaKeyFrameAnims, luBase);
    FixPointerIfPresent(mpaFScriptStream, luBase);
    FixPointer(mpauChildMovieClips, luBase);
    FixPointer(mpaChildNames, luBase);
    FixPointer(mpaMeshes, luBase);
    FixPointer(mpaTextFields, luBase);
    FixPointer(mpaTextFieldNames, luBase);
    FixPointer(mpaFrameLabels, luBase);
    FixPointer(mpauLabelledFrameIds, luBase);
    FixPointerIfPresent(mpcComponentName, luBase);

    for (u32 luKeyFrame = 0; luKeyFrame < muNumKeyFrames; ++luKeyFrame)
    {
        KeyFrameAnims& lrAnims = mpaKeyFrameAnims[luKeyFrame];
        FixPointer(lrAnims.mpauTransformObjects, luBase);
        FixPointer(lrAnims.mpaTransforms, luBase);
        FixPointer(lrAnims.mpauColourTransformObjects, luBase);
        FixPointer(lrAnims.mpaColourTransforms, luBase);
    }

    for (u32 luChild = 0; luChild < muNumChildren; ++luChild)
        FixPointer(mpaChildNames[luChild].mpacDEBUGString, luBase);

    for (u32 luText = 0; luText < muNumTextFields; ++luText)
    {
        FixPointer(mpaTextFields[luText].mName.mpacDEBUGString, luBase);
        FixPointer(mpaTextFieldNames[luText].mpacDEBUGString, luBase);
    }

    for (u32 luLabel = 0; luLabel < muNumLabelledFrames; ++luLabel)
        FixPointer(mpaFrameLabels[luLabel].mpacDEBUGString, luBase);
}

// ---- MovieClip::FixDown @ 0x82470B08 ------------------------------------
void MovieClip::FixDown(uintptr_t luBase)
{
    FixPointerDownIfPresent(mpauFrameToKeyFrameMap, luBase);

    for (u32 luKeyFrame = 0; luKeyFrame < muNumKeyFrames; ++luKeyFrame)
    {
        KeyFrameAnims& lrAnims = mpaKeyFrameAnims[luKeyFrame];
        FixPointerDown(lrAnims.mpauTransformObjects, luBase);
        FixPointerDown(lrAnims.mpaTransforms, luBase);
        FixPointerDown(lrAnims.mpauColourTransformObjects, luBase);
        FixPointerDown(lrAnims.mpaColourTransforms, luBase);
    }

    FixPointerDownIfPresent(mpaFScriptStream, luBase);

    for (u32 luChild = 0; luChild < muNumChildren; ++luChild)
        FixPointerDown(mpaChildNames[luChild].mpacDEBUGString, luBase);

    for (u32 luText = 0; luText < muNumTextFields; ++luText)
    {
        FixPointerDown(mpaTextFields[luText].mName.mpacDEBUGString, luBase);
        FixPointerDown(mpaTextFieldNames[luText].mpacDEBUGString, luBase);
    }

    for (u32 luLabel = 0; luLabel < muNumLabelledFrames; ++luLabel)
        FixPointerDown(mpaFrameLabels[luLabel].mpacDEBUGString, luBase);

    FixPointerDownIfPresent(mpcComponentName, luBase);
    FixPointerDown(mpaRenderLayers, luBase);
    FixPointerDown(mpaKeyFrames, luBase);
    FixPointerDown(mpaKeyFrameAnims, luBase);
    FixPointerDown(mpauChildMovieClips, luBase);
    FixPointerDown(mpaChildNames, luBase);
    FixPointerDown(mpaMeshes, luBase);
    FixPointerDown(mpaTextFields, luBase);
    FixPointerDown(mpaTextFieldNames, luBase);
    FixPointerDown(mpaFrameLabels, luBase);
    FixPointerDown(mpauLabelledFrameIds, luBase);
    mpFile = reinterpret_cast<FlaptFile*>(reinterpret_cast<uintptr_t>(mpFile) - luBase);
}

// ---- FlaptFile::FixUp @ 0x824712F0 --------------------------------------
void FlaptFile::FixUp(const rw::Resource& lrResource)
{
    CGS_ASSERT(muVersion == 12, "Bad FlaptFile version when fixing up");
    const uintptr_t luBase = CgsResource::GetLoadBase64(lrResource);

    mpFlaptFile = this;

    FixPointer(mpaMovieClips, luBase);
    FixPointer(mpapTextures, luBase);
    FixPointer(mpaVerts, luBase);
    FixPointer(mpaFontStyles, luBase);
    FixPointer(mpaComponentNames, luBase);
    FixPointer(mpaComponentPaths, luBase);
    FixPointer(mpaTriggerParameters, luBase);
    FixPointer(mpapStrings, luBase);
    FixPointerIfPresent(mpapSpecialTextureNames, luBase);
    FixPointer(mDEBUGData.mpapStrings, luBase);

    for (u32 luClip = 0; luClip < muNumMovieClips; ++luClip)
        mpaMovieClips[luClip].FixUp(luBase);

    for (u32 luFont = 0; luFont < muNumFontStyles; ++luFont)
        FixPointer(mpaFontStyles[luFont].mpacFontName, luBase);

    for (u32 luComponent = 0; luComponent < muNumComponents; ++luComponent)
        FixPointer(mpaComponentNames[luComponent].mpacDEBUGString, luBase);

    for (u32 luTrigger = 0; luTrigger < muNumTriggerParameters; ++luTrigger)
    {
        for (u32 luParameter = 0; luParameter < 4; ++luParameter)
            FixPointerIfPresent(mpaTriggerParameters[luTrigger].mapcParameters[luParameter], luBase);
    }

    for (u32 luString = 0; luString < muNumStrings; ++luString)
        FixPointer(mpapStrings[luString], luBase);
    for (u32 luSpecial = 0; luSpecial < muNumSpecialTextures; ++luSpecial)
        FixPointer(mpapSpecialTextureNames[luSpecial], luBase);
    for (u32 luDebug = 0; luDebug < mDEBUGData.muNumStrings; ++luDebug)
        FixPointer(mDEBUGData.mpapStrings[luDebug], luBase);
}

// ---- FlaptFile::FixDown @ 0x82470FD8 ------------------------------------
void FlaptFile::FixDown(const rw::Resource& lrResource)
{
    CGS_ASSERT(muVersion == 12, "Bad FlaptFile version when fixing down");
    const uintptr_t luBase = CgsResource::GetLoadBase64(lrResource);

    for (u32 luClip = 0; luClip < muNumMovieClips; ++luClip)
        mpaMovieClips[luClip].FixDown(luBase);
    for (u32 luFont = 0; luFont < muNumFontStyles; ++luFont)
        FixPointerDown(mpaFontStyles[luFont].mpacFontName, luBase);
    for (u32 luComponent = 0; luComponent < muNumComponents; ++luComponent)
        FixPointerDown(mpaComponentNames[luComponent].mpacDEBUGString, luBase);
    for (u32 luTrigger = 0; luTrigger < muNumTriggerParameters; ++luTrigger)
    {
        for (u32 luParameter = 0; luParameter < 4; ++luParameter)
            FixPointerDownIfPresent(mpaTriggerParameters[luTrigger].mapcParameters[luParameter], luBase);
    }
    for (u32 luString = 0; luString < muNumStrings; ++luString)
        FixPointerDown(mpapStrings[luString], luBase);
    for (u32 luSpecial = 0; luSpecial < muNumSpecialTextures; ++luSpecial)
        FixPointerDown(mpapSpecialTextureNames[luSpecial], luBase);
    for (u32 luDebug = 0; luDebug < mDEBUGData.muNumStrings; ++luDebug)
        FixPointerDown(mDEBUGData.mpapStrings[luDebug], luBase);

    FixPointerDown(mDEBUGData.mpapStrings, luBase);
    FixPointerDownIfPresent(mpapSpecialTextureNames, luBase);
    FixPointerDown(mpapStrings, luBase);
    FixPointerDown(mpaTriggerParameters, luBase);
    FixPointerDown(mpaComponentPaths, luBase);
    FixPointerDown(mpaComponentNames, luBase);
    FixPointerDown(mpaFontStyles, luBase);
    FixPointerDown(mpaVerts, luBase);
    FixPointerDown(mpapTextures, luBase);
    FixPointerDown(mpaMovieClips, luBase);
}

// ---- FlaptFile::SetSpecialTexture @ 0x8246D750 ---------------------------
// FixUp publishes the currently loaded file in mpFlaptFile. Special textures
// occupy the final muNumSpecialTextures slots of the texture table; replace the
// slot whose authored name matches the requested name.
void FlaptFile::SetSpecialTexture(GuiTexture* lpFLAptCustomTexture,
                                  const char* lpcSpecialTextureName)
{
    if (mpFlaptFile == 0)
        return;

    for (u32 luSpecialTexture = 0;
         luSpecialTexture < mpFlaptFile->muNumSpecialTextures;
         ++luSpecialTexture)
    {
        if (std::strcmp(mpFlaptFile->mpapSpecialTextureNames[luSpecialTexture],
                        lpcSpecialTextureName) == 0)
        {
            const u32 luTexture = mpFlaptFile->muNumTextures
                                - mpFlaptFile->muNumSpecialTextures
                                + luSpecialTexture;
            mpFlaptFile->mpapTextures[luTexture] = lpFLAptCustomTexture;
            return;
        }
    }
}

}
