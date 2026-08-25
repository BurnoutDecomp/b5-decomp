#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"

#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldInstance.h"
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"
#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"

// The live FLApt clip node. Construct and GetNthChild are reconstructed from
// ARTIST @0x82470040 / @0x8246C450; declaration names come from DecFIGS.
namespace BrnFlapt
{

MovieClipInstance::SoundTriggerCallback MovieClipInstance::gpSoundTriggerCallback = 0;
void* MovieClipInstance::gpSoundTriggerUserData = 0;

namespace
{
    void CopyVector(rw::math::vpu::Vector4& lrDestination,
                    const FloatVector4& lrSource)
    {
        lrDestination.x = lrSource.x;
        lrDestination.y = lrSource.y;
        lrDestination.z = lrSource.z;
        lrDestination.w = lrSource.w;
    }

    // Compose a draw item's local Im2dTransform onto the renderer's current (parent)
    // world transform and push the product as the new stack top. This is the de-optimised
    // form of the VMX128 compose the X360 inlines at every MovieClipInstance::Render draw
    // site (the lvx128/vmaddfp/vperm/stvx128 block around 0x82471960): an affine 2x2 +
    // translation concatenation (parent applied after child) plus the Flapt colour
    // transform (scale multiplies, shift adds). Written as named-component scalar math per
    // the project's x64 semantic-parity-by-named-members rule.
    //
    // mRightUp lane order: {m00, m10, m01, m11} -- the SERIALISED flapt/apt 2x2 {a,b,c,d}
    // raw (right basis = (.x,.y), up basis = (.z,.w)), the same packing the Apt path uses
    // (AptCallbackRender::SetVertexMatrix + the dispatch fold). Point fold (CgsIm2d.cpp):
    //   out.x = mOriginXYZ.x + mRightUp.x*p.x + mRightUp.z*p.y
    //   out.y = mOriginXYZ.y + mRightUp.y*p.x + mRightUp.w*p.y
    // GROUND-TRUTHED via the FLAPTHUD save-icon spin keyframes (2026-07-20): the serialised
    // lanes are byte-identical to the GUIAPT B5SAVEICONCOMPONENT apt matrices ({a,b,c,d} SWF
    // order, +12.3 deg/frame CLOCKWISE); the earlier transposed reading ({m00,m01,m10,m11},
    // inferred from the diagonal-only screen-transform vperm, which cannot distinguish the
    // two) spun every flapt rotation BACKWARDS while leaving axis-aligned content intact.
    void ComposeDrawTransform(FlaptRenderer* lpRenderer,
                              const CgsGraphics::Im2dTransform& lrLocal)
    {
        const CgsGraphics::Im2dTransform& lrParent = lpRenderer->maTransformStack.Peek();
        CgsGraphics::Im2dTransform* const lpWorld  = lpRenderer->maTransformStack.Grow();

        // Affine basis: world 2x2 = parent 2x2 * child 2x2 (column-vector convention,
        // M = [x z; y w]).
        lpWorld->mRightUp.x = lrParent.mRightUp.x * lrLocal.mRightUp.x
                            + lrParent.mRightUp.z * lrLocal.mRightUp.y;
        lpWorld->mRightUp.y = lrParent.mRightUp.y * lrLocal.mRightUp.x
                            + lrParent.mRightUp.w * lrLocal.mRightUp.y;
        lpWorld->mRightUp.z = lrParent.mRightUp.x * lrLocal.mRightUp.z
                            + lrParent.mRightUp.z * lrLocal.mRightUp.w;
        lpWorld->mRightUp.w = lrParent.mRightUp.y * lrLocal.mRightUp.z
                            + lrParent.mRightUp.w * lrLocal.mRightUp.w;

        // Translation: world origin = parent 2x2 * child origin + parent origin.
        lpWorld->mOriginXYZ.x = lrParent.mRightUp.x * lrLocal.mOriginXYZ.x
                              + lrParent.mRightUp.z * lrLocal.mOriginXYZ.y
                              + lrParent.mOriginXYZ.x;
        lpWorld->mOriginXYZ.y = lrParent.mRightUp.y * lrLocal.mOriginXYZ.x
                              + lrParent.mRightUp.w * lrLocal.mOriginXYZ.y
                              + lrParent.mOriginXYZ.y;
        lpWorld->mOriginXYZ.z = lrParent.mOriginXYZ.z;   // 2D depth inherited from parent
        lpWorld->mOriginXYZ.w = 0.0f;

        // Colour transform: scale multiplies, shift adds (component-wise).
        lpWorld->mColourScale.x = lrParent.mColourScale.x * lrLocal.mColourScale.x;
        lpWorld->mColourScale.y = lrParent.mColourScale.y * lrLocal.mColourScale.y;
        lpWorld->mColourScale.z = lrParent.mColourScale.z * lrLocal.mColourScale.z;
        lpWorld->mColourScale.w = lrParent.mColourScale.w * lrLocal.mColourScale.w;
        lpWorld->mColourShift.x = lrParent.mColourShift.x + lrLocal.mColourShift.x;
        lpWorld->mColourShift.y = lrParent.mColourShift.y + lrLocal.mColourShift.y;
        lpWorld->mColourShift.z = lrParent.mColourShift.z + lrLocal.mColourShift.z;
        lpWorld->mColourShift.w = lrParent.mColourShift.w + lrLocal.mColourShift.w;
    }
}

void MovieClipInstance::Construct(const MovieClip* lpMovieClip, const char* lpcName,
                                  MovieClipInstance* lpParent,
                                  CgsMemory::LinearMalloc* lpAlloc,
                                  const FlaptRenderer* lpRenderer,
                                  const RGBA* lpAlternateTextColours,
                                  s32 liNumAlternateColours)
{
    CGS_ASSERT(lpMovieClip != 0, "lpMovieClip");
    CGS_ASSERT(lpAlloc != 0, "lpAlloc");

    mpMovieClip      = lpMovieClip;
    mpcDEBUGName     = lpcName;
    mpParentInstance = lpParent;

    if (mpMovieClip->muNumChildren != 0)
    {
        mpaChildInstances = static_cast<MovieClipInstance*>(
            lpAlloc->Malloc(sizeof(MovieClipInstance) * mpMovieClip->muNumChildren));
        CGS_ASSERT(mpaChildInstances != 0, "mpaChildInstances");
    }
    else
    {
        mpaChildInstances = 0;
    }

    const u32 luNumTransforms = static_cast<u32>(mpMovieClip->muNumChildren)
                              + static_cast<u32>(mpMovieClip->muNumMeshes)
                              + static_cast<u32>(mpMovieClip->muNumTextFields);
    if (luNumTransforms != 0)
    {
        mpaTransforms = static_cast<CgsGraphics::Im2dTransform*>(
            lpAlloc->Malloc(sizeof(CgsGraphics::Im2dTransform) * luNumTransforms));
        CGS_ASSERT(mpaTransforms != 0, "mpaTransforms");
    }
    else
    {
        mpaTransforms = 0;
    }

    if (mpMovieClip->muNumTextFields != 0)
    {
        mpaTextFieldInstances = static_cast<TextFieldInstance*>(
            lpAlloc->Malloc(sizeof(TextFieldInstance) * mpMovieClip->muNumTextFields));
        CGS_ASSERT(mpaTextFieldInstances != 0, "mpaTextFieldInstances");
    }
    else
    {
        mpaTextFieldInstances = 0;
    }

    if (mpMovieClip->muNumChildren != 0)
    {
        CGS_ASSERT(mpMovieClip->mpauChildMovieClips != 0,
                   "mpMovieClip->mpauChildMovieClips");
        for (u32 luChild = 0; luChild < mpMovieClip->muNumChildren; ++luChild)
        {
            const u32 luChildMovieClip = mpMovieClip->mpauChildMovieClips[luChild];
            CGS_ASSERT(luChildMovieClip < mpMovieClip->mpFile->muNumMovieClips,
                       "luChildMovieClip < mpMovieClip->mpFile->muNumMovieClips");
            mpaChildInstances[luChild].Construct(
                &mpMovieClip->mpFile->mpaMovieClips[luChildMovieClip],
                mpMovieClip->mpaChildNames[luChild].mpacDEBUGString,
                this, lpAlloc, lpRenderer, lpAlternateTextColours,
                liNumAlternateColours);
        }
    }

    for (u32 luText = 0; luText < mpMovieClip->muNumTextFields; ++luText)
    {
        mpaTextFieldInstances[luText].Construct(
            mpMovieClip, luText, lpRenderer, lpAlternateTextColours,
            liNumAlternateColours);
    }

    // ARTIST copies the four-vector identity constant into every transform.
    for (u32 luTransform = 0; luTransform < luNumTransforms; ++luTransform)
    {
        CgsGraphics::Im2dTransform& lrTransform = mpaTransforms[luTransform];
        lrTransform.mOriginXYZ.x = 0.0f;
        lrTransform.mOriginXYZ.y = 0.0f;
        lrTransform.mOriginXYZ.z = 0.0f;
        lrTransform.mOriginXYZ.w = 0.0f;
        lrTransform.mRightUp.x = 1.0f;
        lrTransform.mRightUp.y = 0.0f;
        lrTransform.mRightUp.z = 0.0f;
        lrTransform.mRightUp.w = 1.0f;
        lrTransform.mColourShift.x = 0.0f;
        lrTransform.mColourShift.y = 0.0f;
        lrTransform.mColourShift.z = 0.0f;
        lrTransform.mColourShift.w = 0.0f;
        lrTransform.mColourScale.x = 1.0f;
        lrTransform.mColourScale.y = 1.0f;
        lrTransform.mColourScale.z = 1.0f;
        lrTransform.mColourScale.w = 1.0f;
    }

    mfTimeTillNextFrame   = mpMovieClip->mpFile->mfTimePerFrame;
    muCurrentFrame        = static_cast<u16>(-1);
    muLastKeyFrameApplied = static_cast<u16>(-1);
    mpFrameTriggerCallback = 0;
    mpFrameTriggerUserData = 0;
    for (u32 luWord = 0; luWord < 3; ++luWord)
        maxPlacedChildren[luWord] = 0;
    mxFlags = 2;
    muParameterIndex = static_cast<u16>(-1);
}

void MovieClipInstance::GetNthChild(u32 luChild, MovieClipRef* lpOutRef) const
{
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");
    CGS_ASSERT(mpaChildInstances != 0, "mpaChildInstances");
    CGS_ASSERT(lpOutRef != 0, "lpOutMovieClipRef");
    CGS_ASSERT(luChild < mpMovieClip->muNumChildren,
               "luChild < mpMovieClip->muNumChildren");

    lpOutRef->mpMovieClipInst = &mpaChildInstances[luChild];
    lpOutRef->mpTransform     = &mpaTransforms[luChild];
}

void MovieClipInstance::FindChildMovieClip(u32 luNameHash,
                                           MovieClipRef* lpOutRef,
                                           const char* lpcName)
{
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");
    CGS_ASSERT(mpMovieClip->mpaChildNames != 0,
               "mpMovieClip->mpaChildNames");
    CGS_ASSERT(mpaChildInstances != 0, "mpaChildInstances");
    CGS_ASSERT(lpOutRef != 0, "lpOutMovieClipRef");

    for (u32 luChild = 0; luChild < mpMovieClip->muNumChildren; ++luChild)
    {
        if (mpMovieClip->mpaChildNames[luChild].muHash == luNameHash)
        {
            lpOutRef->mpMovieClipInst = &mpaChildInstances[luChild];
            lpOutRef->mpTransform = &mpaTransforms[luChild];
            return;
        }
    }

    (void)lpcName;
    CGS_ASSERT(false, "Unable to find child movieclip");
}

void MovieClipInstance::FindChildMovieClipOnFrame(u32 luNameHash,
                                                  MovieClipRef* lpOutRef,
                                                  const char* lpcName)
{
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");
    CGS_ASSERT(mpMovieClip->mpaChildNames != 0,
               "mpMovieClip->mpaChildNames");
    CGS_ASSERT(mpaChildInstances != 0, "mpaChildInstances");
    CGS_ASSERT(lpOutRef != 0, "lpOutMovieClipRef");
    CGS_ASSERT(mpMovieClip->muNumChildren < 96,
               "mpMovieClip->muNumChildren < KU_MAX_CHILDREN_PER_MOVIECLIP");

    for (u32 luChild = 0; luChild < mpMovieClip->muNumChildren; ++luChild)
    {
        const u32 luMask = 1u << (luChild % 32u);
        if (mpMovieClip->mpaChildNames[luChild].muHash == luNameHash
            && (maxPlacedChildren[luChild / 32u] & luMask) != 0)
        {
            lpOutRef->mpMovieClipInst = &mpaChildInstances[luChild];
            lpOutRef->mpTransform = &mpaTransforms[luChild];
            return;
        }
    }

    (void)lpcName;
    CGS_ASSERT(false, "Unable to find child movieclip on frame");
}

void MovieClipInstance::FindChildTextField(u32 luNameHash,
                                           TextFieldRef* lpOutRef,
                                           const char* lpcName)
{
    CGS_ASSERT(lpOutRef != 0, "lpOutTextFieldRef");
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");
    CGS_ASSERT(mpMovieClip->mpaTextFieldNames != 0,
               "mpMovieClip->mpaTextFieldNames");
    CGS_ASSERT(mpaTextFieldInstances != 0, "mpaTextFieldInstances");
    CGS_ASSERT(mpaTransforms != 0, "mpaTransforms");

    for (u32 luTextField = 0;
         luTextField < mpMovieClip->muNumTextFields;
         ++luTextField)
    {
        if (mpMovieClip->mpaTextFieldNames[luTextField].muHash == luNameHash)
        {
            const u32 luTransform = static_cast<u32>(mpMovieClip->muNumChildren)
                                  + static_cast<u32>(mpMovieClip->muNumMeshes)
                                  + luTextField;
            lpOutRef->Construct(&mpaTextFieldInstances[luTextField], this,
                                &mpaTransforms[luTransform]);
            return;
        }
    }

    (void)lpcName;
    CGS_ASSERT(false, "Unable to find child textfield");
}

bool MovieClipInstance::TryFindChildComponentRecursively(
    u32 luNameHash, MovieClipRef* lpOutRef, const char* lpcName)
{
    CGS_ASSERT(lpOutRef != 0, "lpOutMovieClipRef");
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");

    for (u32 luChild = 0; luChild < mpMovieClip->muNumChildren; ++luChild)
    {
        MovieClipInstance& lrChild = mpaChildInstances[luChild];
        if (mpMovieClip->mpaChildNames[luChild].muHash == luNameHash)
        {
            CGS_ASSERT((lrChild.mpMovieClip->mxFlags & 1u) != 0,
                       "Found matching movieclip which is not a component");
            lpOutRef->mpMovieClipInst = &lrChild;
            lpOutRef->mpTransform = &mpaTransforms[luChild];
            return true;
        }

        if ((lrChild.mpMovieClip->mxFlags & 1u) == 0
            && lrChild.TryFindChildComponentRecursively(luNameHash,
                                                        lpOutRef, lpcName))
        {
            return true;
        }
    }

    return false;
}

void MovieClipInstance::GetParent(MovieClipRef* lpOutRef)
{
    CGS_ASSERT(lpOutRef != 0, "lpOutMovieClipRef");
    CGS_ASSERT(mpParentInstance != 0,
               "Tried to get the parent movieclip for a top-level movieclip");
    CGS_ASSERT(mpParentInstance->mpParentInstance != 0,
               "Tried to get the grandparent movieclip for a top-level movieclip");

    MovieClipInstance* lpGrandParent = mpParentInstance->mpParentInstance;
    CGS_ASSERT(lpGrandParent->mpMovieClip != 0,
               "mpParentInstance->mpParentInstance->mpMovieClip");

    u32 luParentIndex = 0;
    while (luParentIndex < lpGrandParent->mpMovieClip->muNumChildren
           && &lpGrandParent->mpaChildInstances[luParentIndex] != mpParentInstance)
    {
        ++luParentIndex;
    }
    CGS_ASSERT(luParentIndex != lpGrandParent->mpMovieClip->muNumChildren,
               "luParentIndex != lpGrandParentClip->muNumChildren");

    lpOutRef->mpMovieClipInst = mpParentInstance;
    lpOutRef->mpTransform = &lpGrandParent->mpaTransforms[luParentIndex];
}

MovieClipInstance* MovieClipInstance::GetNthChild(u32 luChild) const
{
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");
    CGS_ASSERT(mpaChildInstances != 0, "mpaChildInstances");
    CGS_ASSERT(luChild < mpMovieClip->muNumChildren,
               "luChild < mpMovieClip->muNumChildren");
    return &mpaChildInstances[luChild];
}

void MovieClipInstance::GotoFrame(u32 luFrame)
{
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");
    CGS_ASSERT(luFrame < mpMovieClip->muNumFramesInTimeline,
               "luFrame < mpMovieClip->muNumFramesInTimeline");

    muCurrentFrame = static_cast<u16>(luFrame);
    const u32 luNumObjects = static_cast<u32>(mpMovieClip->muNumChildren)
                           + static_cast<u32>(mpMovieClip->muNumMeshes)
                           + static_cast<u32>(mpMovieClip->muNumTextFields);
    for (u32 luObject = 0; luObject < luNumObjects; ++luObject)
    {
        CgsGraphics::Im2dTransform& lrTransform = mpaTransforms[luObject];
        lrTransform.mColourShift.x = 0.0f;
        lrTransform.mColourShift.y = 0.0f;
        lrTransform.mColourShift.z = 0.0f;
        lrTransform.mColourShift.w = 0.0f;
        lrTransform.mColourScale.x = 1.0f;
        lrTransform.mColourScale.y = 1.0f;
        lrTransform.mColourScale.z = 1.0f;
        lrTransform.mColourScale.w = 1.0f;
    }

    ApplyKeyFrame(mpMovieClip->GetKeyframeForFrame(luFrame));
    mxFlags = static_cast<u8>(mxFlags | 0x10u);
}

void MovieClipInstance::ResetTimeline()
{
    muLastKeyFrameApplied = static_cast<u16>(-1);
    mxFlags = static_cast<u8>(mxFlags & 0xFEu);
    for (u32 luWord = 0; luWord < 3; ++luWord)
        maxPlacedChildren[luWord] = 0;
    GotoFrame(0);
}

void MovieClipInstance::GotoAndPlayLabel(u32 luLabelHash,
                                         const char* lpcDEBUGName)
{
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");
    const u32 luFrame = static_cast<u32>(
        mpMovieClip->FindLabelledFrameIndex(luLabelHash, lpcDEBUGName));
    CGS_ASSERT(luFrame < mpMovieClip->muNumFramesInTimeline,
               "luFrame < mpMovieClip->muNumFramesInTimeline");
    muLastKeyFrameApplied = static_cast<u16>(-1);
    mxFlags = static_cast<u8>(mxFlags & 0xFEu);
    GotoFrame(luFrame);
}

void MovieClipInstance::GotoAndStopLabel(u32 luLabelHash,
                                         const char* lpcLabel)
{
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");
    const u32 luFrame = static_cast<u32>(
        mpMovieClip->FindLabelledFrameIndex(luLabelHash, lpcLabel));
    // [DIAG] NOT IN THE X360 BINARY -- a label that does not exist in the bound clip
    // resolves to an out-of-range frame and the CGS_ASSERT below is a NO-OP in this
    // build, so the goto silently does nothing and the component never appears. Say so.
    if (luFrame >= mpMovieClip->muNumFramesInTimeline && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[flapt] GotoAndStopLabel: label '" << (lpcLabel != 0 ? lpcLabel : "?")
            << "' NOT FOUND in clip (frames=" << mpMovieClip->muNumFramesInTimeline
            << ")\n";
    }
    CGS_ASSERT(luFrame < mpMovieClip->muNumFramesInTimeline,
               "luFrame < mpMovieClip->muNumFramesInTimeline");
    muLastKeyFrameApplied = static_cast<u16>(-1);
    mxFlags = static_cast<u8>(mxFlags | 1u);
    GotoFrame(luFrame);
}

void MovieClipInstance::SetFrameTriggerCallback(FrameTriggerCallback lpCallback,
                                                void* lpUserData)
{
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");
    CGS_ASSERT((mpMovieClip->mxFlags & 1u) != 0,
               "Frame trigger callbacks can only be set for components");
    CGS_ASSERT(lpCallback != 0, "lpCallback");
    mpFrameTriggerCallback = lpCallback;
    mpFrameTriggerUserData = lpUserData;
}

void MovieClipInstance::SetSoundTriggerHandler(SoundTriggerCallback lpCallback,
                                               void* lpUserData)
{
    CGS_ASSERT(lpCallback != 0, "lpCallback");
    gpSoundTriggerCallback = lpCallback;
    gpSoundTriggerUserData = lpUserData;
}

const TriggerParameters* MovieClipInstance::GetTriggerParameters() const
{
    CGS_ASSERT(mpMovieClip != 0, "NULL != mpMovieClip");
    CGS_ASSERT(mpMovieClip->mpFile != 0, "NULL != mpMovieClip->mpFile");
    CGS_ASSERT(muParameterIndex != 0xFFFFu, "0xFFFF != muParameterIndex");
    CGS_ASSERT(muParameterIndex < mpMovieClip->mpFile->muNumTriggerParameters,
               "muParameterIndex < mpMovieClip->mpFile->muNumTriggerParameters");
    CGS_ASSERT(mpMovieClip->mpFile->mpaTriggerParameters != 0,
               "NULL != mpMovieClip->mpFile->mpaTriggerParameters");
    return &mpMovieClip->mpFile->mpaTriggerParameters[muParameterIndex];
}

void MovieClipInstance::ProcessFScript(const FScriptCommand* lpStream,
                                       u32 luCommandCount,
                                       u32 luCurrentKeyFrame)
{
    CGS_ASSERT(lpStream != 0, "lpStream");

    for (u32 luCommand = 0; luCommand < luCommandCount; ++luCommand)
    {
        const FScriptCommand& lrCommand = lpStream[luCommand];
        switch (lrCommand.muCommand)
        {
        case 0:
            mxFlags = static_cast<u8>(mxFlags | 1u);
            break;

        case 1:
        case 2:
        {
            MovieClipInstance* lpTarget = this;
            if (lrCommand.muParam8 != 0xFFu)
            {
                CGS_ASSERT(lrCommand.muParam8 < mpMovieClip->muNumChildren,
                           "lpCommand->muParam8 < mpMovieClip->muNumChildren");
                lpTarget = &mpaChildInstances[lrCommand.muParam8];
            }

            if (lrCommand.muCommand == 1)
                lpTarget->mxFlags = static_cast<u8>(lpTarget->mxFlags & 0xFEu);
            else
                lpTarget->mxFlags = static_cast<u8>(lpTarget->mxFlags | 1u);

            CGS_ASSERT(lpTarget->mpMovieClip->GetKeyframeForFrame(
                           lrCommand.muParam16) != luCurrentKeyFrame,
                       "MovieClip tried to jump to the frame it's already on");
            lpTarget->GotoFrame(lrCommand.muParam16);
            break;
        }

        case 3:
        {
            CGS_ASSERT(lrCommand.muParam8 == 0 || lrCommand.muParam8 == 1,
                       "lpCommand->muParam8 == 0 || lpCommand->muParam8 == 1");
            MovieClipInstance* lpTarget = this;
            if (lrCommand.muParam16 != 0xFFFFu)
            {
                CGS_ASSERT(lrCommand.muParam16 < mpMovieClip->muNumChildren,
                           "lpCommand->muParam16 < mpMovieClip->muNumChildren");
                lpTarget = &mpaChildInstances[lrCommand.muParam16];
            }
            lpTarget->mxFlags = lrCommand.muParam8 != 0
                ? static_cast<u8>(lpTarget->mxFlags | 0x02u)
                : static_cast<u8>(lpTarget->mxFlags & 0xFDu);
            break;
        }

        case 4:
            CGS_ASSERT(lrCommand.muParam8 < FScriptCommand::E_TRIGGERTYPE_COUNT,
                       "lpCommand->muParam8 < FScriptCommand::E_TRIGGERTYPE_COUNT");
            FireTrigger(lrCommand.muParam8, lrCommand.muParam16);
            break;

        case 5:
            CGS_ASSERT(mpaChildInstances != 0, "Child movie clip pointer is NULL");
            CGS_ASSERT(lrCommand.muParam8 < mpMovieClip->muNumChildren,
                       "Trying to set animation parameters on a missing child movie clip");
            mpaChildInstances[lrCommand.muParam8].muParameterIndex =
                lrCommand.muParam16;
            break;

        default:
            CGS_ASSERT(false, "Got invalid keyframe command");
            break;
        }
    }
}

void MovieClipInstance::FireTrigger(s32 leTriggerType, u16 luArg) const
{
    const MovieClipInstance* lpComponent = this;
    while (lpComponent != 0
           && (lpComponent->mpMovieClip->mxFlags & 1u) == 0)
    {
        lpComponent = lpComponent->mpParentInstance;
    }

    if (lpComponent == 0)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "FLApt WARNING: Couldn't find a component to send a frame trigger to, from movie clip "
                << mpcDEBUGName << "\n";
        }
        return;
    }

    switch (leTriggerType)
    {
    case FScriptCommand::E_TRIGGER_SOUND:
    {
        CGS_ASSERT(gpSoundTriggerCallback != 0, "gpSoundTriggerCallback");
        CGS_ASSERT(gpSoundTriggerUserData != 0, "gpSoundTriggerUserData");
        CGS_ASSERT(luArg < mpMovieClip->mpFile->muNumTriggerParameters,
                   "luArg < mpMovieClip->mpFile->muNumTriggerParameters");
        CGS_ASSERT(mpMovieClip->mpFile->mpaTriggerParameters != 0,
                   "mpMovieClip->mpFile->mpaTriggerParameters");
        CGS_ASSERT(lpComponent->mpMovieClip->mpcComponentName != 0,
                   "lpComponent->mpMovieClip->mpcComponentName");
        const TriggerParameters& lrParameters =
            mpMovieClip->mpFile->mpaTriggerParameters[luArg];
        gpSoundTriggerCallback(gpSoundTriggerUserData,
                               lpComponent->mpMovieClip->mpcComponentName,
                               lrParameters.mapcParameters[0],
                               lrParameters.mapcParameters[1],
                               lrParameters.mapcParameters[2]);
        break;
    }

    case FScriptCommand::E_TRIGGER_TRANSITIONCOMPLETE:
        if (lpComponent->mpFrameTriggerCallback != 0)
        {
            lpComponent->mpFrameTriggerCallback(
                lpComponent->mpFrameTriggerUserData, luArg);
        }
        else if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "FLApt WARNING: Component "
                << lpComponent->mpcDEBUGName
                << " doesn't have a callback to receive a frame trigger\n";
        }
        break;

    default:
        CGS_ASSERT(false, "Invalid trigger type for FLApt");
        break;
    }
}

void MovieClipInstance::ApplyKeyFrame(u32 luKeyFrameIndex)
{
    CGS_ASSERT(luKeyFrameIndex < mpMovieClip->muNumKeyFrames,
               "luKeyFrameIndex < mpMovieClip->muNumKeyFrames");
    CGS_ASSERT(mpMovieClip->mpaKeyFrameAnims != 0,
               "mpMovieClip->mpaKeyFrameAnims");

    if (muLastKeyFrameApplied == luKeyFrameIndex)
        return;

    mxFlags = static_cast<u8>(mxFlags & 0xEFu);
    const KeyFrameAnims& lrAnims =
        mpMovieClip->mpaKeyFrameAnims[luKeyFrameIndex];
    if (lrAnims.muCommandCount != 0)
    {
        ProcessFScript(&mpMovieClip->mpaFScriptStream[lrAnims.muCommandOffset],
                       lrAnims.muCommandCount, luKeyFrameIndex);
    }

    if ((mxFlags & 0x10u) != 0)
        return;

    CGS_ASSERT(mpMovieClip->muNumChildren < 96,
               "mpMovieClip->muNumChildren < KU_MAX_CHILDREN_PER_MOVIECLIP");
    for (u32 luChild = 0; luChild < mpMovieClip->muNumChildren; ++luChild)
    {
        const u32 luMask = 1u << (luChild % 32u);
        const u32 luWord = luChild / 32u;
        if ((lrAnims.maxPlacedChildren[luWord] & luMask) != 0
            && (maxPlacedChildren[luWord] & luMask) == 0)
        {
            mpaChildInstances[luChild].ResetTimeline();
        }
    }
    for (u32 luWord = 0; luWord < 3; ++luWord)
        maxPlacedChildren[luWord] = lrAnims.maxPlacedChildren[luWord];

    const u32 luNumObjects = static_cast<u32>(mpMovieClip->muNumChildren)
                           + static_cast<u32>(mpMovieClip->muNumMeshes)
                           + static_cast<u32>(mpMovieClip->muNumTextFields);
    for (u32 luTransform = 0; luTransform < lrAnims.muNumTransforms;
         ++luTransform)
    {
        CGS_ASSERT(lrAnims.mpauTransformObjects != 0,
                   "NULL != lpAnims->mpauTransformObjects");
        const u32 luObject = lrAnims.mpauTransformObjects[luTransform];
        CGS_ASSERT(luObject < luNumObjects,
                   "luObject < mpMovieClip->GetNumObjects()");
        CopyVector(mpaTransforms[luObject].mOriginXYZ,
                   lrAnims.mpaTransforms[luTransform].mOriginXYZ);
        CopyVector(mpaTransforms[luObject].mRightUp,
                   lrAnims.mpaTransforms[luTransform].mRightUp);
    }

    for (u32 luColour = 0; luColour < lrAnims.muNumColourTransforms;
         ++luColour)
    {
        CGS_ASSERT(lrAnims.mpauColourTransformObjects != 0,
                   "NULL != lpAnims->mpauColourTransformObjects");
        const u32 luObject = lrAnims.mpauColourTransformObjects[luColour];
        CGS_ASSERT(luObject < luNumObjects,
                   "luObject < mpMovieClip->GetNumObjects()");
        CopyVector(mpaTransforms[luObject].mColourScale,
                   lrAnims.mpaColourTransforms[luColour].mScale);
        CopyVector(mpaTransforms[luObject].mColourShift,
                   lrAnims.mpaColourTransforms[luColour].mTranslate);
    }

    muLastKeyFrameApplied = static_cast<u16>(luKeyFrameIndex);
}

void MovieClipInstance::Update(f32 lfTimeStep)
{
    if ((mxFlags & 1u) == 0)
    {
        CGS_ASSERT(muCurrentFrame < mpMovieClip->muNumFramesInTimeline,
                   "muCurrentFrame < mpMovieClip->muNumFramesInTimeline");
        mfTimeTillNextFrame -= lfTimeStep;
        if (mfTimeTillNextFrame <= 0.0f)
        {
            ++muCurrentFrame;
            mfTimeTillNextFrame += mpMovieClip->mpFile->mfTimePerFrame;
            CGS_ASSERT(mfTimeTillNextFrame > 0.0f,
                       "Skipped a frame in FLApt anim");
            if (muCurrentFrame >= mpMovieClip->muNumFramesInTimeline)
            {
                mxFlags = static_cast<u8>(mxFlags | 1u);
                return;
            }
            ApplyKeyFrame(mpMovieClip->GetKeyframeForFrame(muCurrentFrame));
        }
    }

    CGS_ASSERT(mpMovieClip->muNumChildren < 96,
               "mpMovieClip->muNumChildren < KU_MAX_CHILDREN_PER_MOVIECLIP");
    for (u32 luChild = 0; luChild < mpMovieClip->muNumChildren; ++luChild)
    {
        const u32 luMask = 1u << (luChild % 32u);
        MovieClipInstance& lrChild = mpaChildInstances[luChild];
        if ((maxPlacedChildren[luChild / 32u] & luMask) != 0
            && (lrChild.mxFlags & 0x02u) != 0)
        {
            lrChild.Update(lfTimeStep);
        }
    }
}

s32 MovieClipInstance::GetCurrentFrameOneBased() const
{
    return static_cast<s32>(muCurrentFrame) + 1;
}

MovieClipRef* MovieClipRef::SetVisible(bool lbVisible)
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    const u8 luFlags = mpMovieClipInst->mxFlags;
    mpMovieClipInst->mxFlags = lbVisible
        ? static_cast<u8>(luFlags | 0x02u)
        : static_cast<u8>(luFlags & 0xFDu);
    return this;
}

// ---- Render @0x824718F0 ---------------------------------------------------
// Draw this clip node and (recursively) its children into the Flapt render buffer. Walk
// the clip's render layers in order; within each layer, for the current keyframe's
// per-category enabled bitmask, draw the layer's child clips, meshes, and text fields --
// each through its own local Im2dTransform composed onto the renderer's transform stack
// (peek parent, grow a slot for the world transform, render, pop). A layer may bracket
// its contents in a stencil mask: StartDrawingMask flags the renderer so RenderMesh
// routes the mask-shape meshes to RenderMask; the walk clears the mask bit once the shape
// is drawn (so the layer's content draws clipped), then PopMask closes it.
//
// mpaTransforms is the flat [children | meshes | text fields] slot array Construct builds
// (numChildren + numMeshes + numTextFields entries), so a mesh's local transform lives at
// numChildren + meshIndex and a text field's at numChildren + numMeshes + fieldIndex.
// Children are additionally culled when their local transform is fully transparent
// (colour alpha scale+shift <= 0) or their instance visible-flag (mxFlags bit1, set by
// MovieClipRef::SetVisible) is clear -- both tests the X360 makes before recursing.
void MovieClipInstance::Render(FlaptRenderer* lpRenderer)
{
    CGS_ASSERT(lpRenderer != 0, "lpRenderer");
    CGS_ASSERT(mpMovieClip != 0, "mpMovieClip");
    CGS_ASSERT(mpMovieClip->mpaRenderLayers != 0, "mpMovieClip->mpaRenderLayers");
    CGS_ASSERT(mpMovieClip->mpaKeyFrames != 0, "mpMovieClip->mpaKeyFrames");
    CGS_ASSERT(mpMovieClip->mpaMeshes != 0, "mpMovieClip->mpaMeshes");
    CGS_ASSERT(muLastKeyFrameApplied < mpMovieClip->muNumKeyFrames,
               "muLastKeyFrameApplied < mpMovieClip->muNumKeyFrames");

    const MovieClip* const lpClip = mpMovieClip;
    const u32 luNumLayers = lpClip->muNumRenderLayers;

    for (u32 luLayer = 0; luLayer < luNumLayers; ++luLayer)
    {
        const RenderLayer* const lpLayer = &lpClip->mpaRenderLayers[luLayer];
        const RenderLayerKeyFrame* const lpKeyFrame =
            &lpClip->mpaKeyFrames[muLastKeyFrameApplied * luNumLayers + luLayer];

        const bool lbLayerOpensMask = (lpLayer->mxFlags & 1u) != 0;
        if (lbLayerOpensMask)
        {
            lpRenderer->StartDrawingMask();
        }

        // --- child movie clips ---
        {
            const u32 luFirst = lpLayer->muMovieClipOffset;
            const u32 luEnd   = luFirst + lpLayer->muMovieClipCount;
            u32 luBit = 1u;
            for (u32 luChild = luFirst; luChild < luEnd; ++luChild, luBit <<= 1)
            {
                CGS_ASSERT(luChild < lpClip->muNumChildren,
                           "luMovieClip < lpMovieClip->muNumChildren");
                if ((lpKeyFrame->mxEnabledMovieClips & luBit) == 0)
                {
                    continue;
                }

                MovieClipInstance* const lpChild = &mpaChildInstances[luChild];
                const CgsGraphics::Im2dTransform& lrLocal = mpaTransforms[luChild];

                // Cull fully-transparent (colour alpha scale+shift <= 0) or hidden children.
                const f32 lfAlpha = lrLocal.mColourScale.w + lrLocal.mColourShift.w;
                if (lfAlpha > 0.0f && (lpChild->mxFlags & 2u) != 0)
                {
                    ComposeDrawTransform(lpRenderer, lrLocal);
                    lpChild->Render(lpRenderer);
                    CGS_ASSERT(lpRenderer->maTransformStack.GetLength() > 1,
                               "mTransformStack.GetLength() > 1");
                    lpRenderer->maTransformStack.Pop();
                }
            }
        }

        // --- meshes ---
        {
            const u32 luFirst = lpLayer->muMeshOffset;
            const u32 luEnd   = luFirst + lpLayer->muMeshCount;
            u32 luBit = 1u;
            for (u32 luMesh = luFirst; luMesh < luEnd; ++luMesh, luBit <<= 1)
            {
                CGS_ASSERT(luMesh < lpClip->muNumMeshes,
                           "luMesh < lpMovieClip->muNumMeshes");
                if ((lpKeyFrame->mxEnabledMeshes & luBit) == 0)
                {
                    continue;
                }

                const Mesh* const lpMesh = &lpClip->mpaMeshes[luMesh];
                const CgsGraphics::Im2dTransform& lrLocal =
                    mpaTransforms[lpClip->muNumChildren + luMesh];

                ComposeDrawTransform(lpRenderer, lrLocal);
                lpRenderer->RenderMesh(lpMesh, lpClip->mpFile);
                CGS_ASSERT(lpRenderer->maTransformStack.GetLength() > 1,
                           "mTransformStack.GetLength() > 1");
                lpRenderer->maTransformStack.Pop();
            }
        }

        // --- text fields ---
        {
            const u32 luFirst = lpLayer->muTextFieldOffset;
            const u32 luEnd   = luFirst + lpLayer->muTextFieldCount;
            u32 luBit = 1u;
            for (u32 luText = luFirst; luText < luEnd; ++luText, luBit <<= 1)
            {
                CGS_ASSERT(luText < lpClip->muNumTextFields,
                           "luTextField < lpMovieClip->muNumTextFields");
                if ((lpKeyFrame->mxEnabledTextFields & luBit) == 0)
                {
                    continue;
                }

                TextFieldInstance* const lpText = &mpaTextFieldInstances[luText];
                const CgsGraphics::Im2dTransform& lrLocal =
                    mpaTransforms[lpClip->muNumChildren + lpClip->muNumMeshes + luText];

                ComposeDrawTransform(lpRenderer, lrLocal);
                lpRenderer->RenderTextField(&lpText->GetTextObject());
                CGS_ASSERT(lpRenderer->maTransformStack.GetLength() > 1,
                           "mTransformStack.GetLength() > 1");
                lpRenderer->maTransformStack.Pop();
            }
        }

        // --- close the layer's mask, if it opened one ---
        if (lbLayerOpensMask)
        {
            CGS_ASSERT((lpRenderer->mxFlags & 1u) != 0, "IsRenderingMask()");
            lpRenderer->mxFlags &= ~1u;
        }
        if ((lpLayer->mxFlags & 2u) != 0)
        {
            lpRenderer->PopMask();
        }
    }
}

}
