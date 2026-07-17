#ifndef BRN_FLAPT_MOVIE_CLIP_INSTANCE_H
#define BRN_FLAPT_MOVIE_CLIP_INSTANCE_H

#include "types.hpp"

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h
//
// BrnFlapt::MovieClipInstance -- a live, instanced movie-clip node in a Flapt
// timeline tree. Declaration home for the type. Reconstructed from
// BURNOUT_X360_ARTIST.XEX, with declaration shapes taken from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h)
// and gated on the X360 ledger.
//
// The live instance layout and the lifecycle, lookup, navigation, keyframe,
// FScript, and trigger methods are declared here and implemented in the class TU.
// Rendering remains a separate milestone because it depends on the renderer body.
//
// Calling convention (verified from the ARTIST asm at the BrnFlaptMovieClipRef
// call sites): each method is invoked on the live instance pointer the Ref
// holds in its first slot; the sret-returning lookups (FindChild*, GetParent)
// take the caller's out handle by pointer and write it, so the DWARF renders
// them `void`.
// ============================================================================

struct RGBA;
namespace CgsMemory { class LinearMalloc; }
namespace CgsGraphics { struct Im2dTransform; }

namespace BrnFlapt
{
    struct FlaptRenderer;
    struct MovieClip;
    struct MovieClipRef;
    class TextFieldInstance;
    struct TextFieldRef;
    struct TriggerParameters;

    struct MovieClipInstance
    {
        typedef void (*SoundTriggerCallback)(void* lpUserData,
                                             const char* lpcComponentName,
                                             const char* lpcParameter0,
                                             const char* lpcParameter1,
                                             const char* lpcParameter2);

        // ---- lifecycle ------------------------------------------------------
        // Construct / GotoFrame (DWARF BrnFlaptMovieClipInstance.h:73/:109): bind
        // this instance to its serialised MovieClip timeline and rewind to a frame.
        // Called by FlaptFileInstance::SetData @0x82471620 on the root clip
        // (clip = &mpFile->mpaMovieClips[0], name "_root", no parent). Declared here
        // for the root clip (clip 0, name "_root", no parent).
        void Construct(const MovieClip* lpClip, const char* lpcName,
                       MovieClipInstance* lpParent, CgsMemory::LinearMalloc* lpAllocator,
                       const FlaptRenderer* lpRenderer, const RGBA* lpAlternateTextColours,
                       s32 liNumAlternateColours);
        void GotoFrame(u32 luFrame);

        // ---- per-frame drive ------------------------------------------------
        // Update / Render: advance / draw this clip node and its children. Called
        // by FlaptFileInstance::Update @0x82471820 / Render @0x82472480 on the
        // root clip.
        void Update(f32 lfTimeStep);
        void Render(FlaptRenderer* lpRenderer);

        // BrnFlaptTypes.h:36 -- per-frame trigger callback signature
        // (DWARF: void (*)(void*, uint16_t)).
        typedef void (*FrameTriggerCallback)(void* lpUserData, u16 luFrame);

        // ---- named-child lookups (sret out handle written in place) --------
        // FindChildMovieClip        @ 0x8246C740 call site (instance @ ledger)
        void FindChildMovieClip(u32 luNameHash, MovieClipRef* lpOutRef,
                                const char* lpcName);

        // FindChildMovieClipOnFrame @ 0x8246C8B0 call site
        void FindChildMovieClipOnFrame(u32 luNameHash, MovieClipRef* lpOutRef,
                                       const char* lpcName);

        // FindChildTextField        @ 0x8246C7F8 call site
        void FindChildTextField(u32 luNameHash, TextFieldRef* lpOutRef,
                                const char* lpcName);

        // TryFindChildComponentRecursively @ 0x8246C968 call site -- returns
        // true when a descendant component matched.
        bool TryFindChildComponentRecursively(u32 luNameHash,
                                              MovieClipRef* lpOutRef,
                                              const char* lpcName);

        // GetParent @ 0x8246CA40 call site -- sret parent handle into lpOutRef.
        void GetParent(MovieClipRef* lpOutRef);

        // GetNthChild @0x8246C450 / inline pointer form @0x8246C560.
        // The Ref form pairs the live child with its transform slot.
        void GetNthChild(u32 luChild, MovieClipRef* lpOutRef) const;
        MovieClipInstance* GetNthChild(u32 luChild) const;

        // ---- timeline playback --------------------------------------------
        // ResetTimeline -- rewind/reset this clip instance's timeline to its
        // initial frame. Called on the bound instance by BrnFlaptComponent::Prepare
        // (X360 @ 0x8240E670: lwz r3,4(component) -> bl ResetTimeline). X360-attested
        // in the ledger (BrnFlapt::MovieClipInstance::ResetTimeline); the asm
        // propagates r3 as a tail-call artifact, so the logical return type is void.
        void ResetTimeline();

        // GotoAndPlayLabel @ 0x8246F388 call site.
        void GotoAndPlayLabel(u32 luLabelHash, const char* lpcDEBUGName);

        // GotoAndStopLabel @ 0x8246F498 call site (Ref hashes the label first).
        void GotoAndStopLabel(u32 luLabelHash, const char* lpcLabel);

        // ---- trigger callbacks --------------------------------------------
        // The clip's current timeline frame, ONE-BASED (the accessor NAME is given
        // by BoostMessageSlot's :266 assert text). CONTRACT PIN: the X360 field at
        // instance +0x28 is ZERO-based -- the inlined accessor returns field+1, and
        // the BoostMessageSlot consumers compare the ONE-based values (10/19/27/34,
        // 13/45/66, <=34). The body MUST add one. GROWN for BoostMessageSlot::Update
        // @0x82420E60; DECLARATION-ONLY (the timeline TU owns the layout).
        s32 GetCurrentFrameOneBased() const;

        // SetFrameTriggerCallback @ 0x8246CAB8 call site.
        void SetFrameTriggerCallback(FrameTriggerCallback lpCallback,
                                     void* lpUserData);

        static void SetSoundTriggerHandler(SoundTriggerCallback lpCallback,
                                           void* lpUserData);

        // GetTriggerParameters @ 0x8246CAB0 call site (tail-call forward).
        const TriggerParameters* GetTriggerParameters() const;

        void ApplyKeyFrame(u32 luKeyFrameIndex);
        void ProcessFScript(const struct FScriptCommand* lpStream,
                            u32 luCommandCount, u32 luCurrentKeyFrame);
        void FireTrigger(s32 leTriggerType, u16 luArg) const;

        static SoundTriggerCallback gpSoundTriggerCallback;
        static void* gpSoundTriggerUserData;

        const MovieClip* mpMovieClip;
        const char* mpcDEBUGName;
        u32 maxPlacedChildren[3];
        MovieClipInstance* mpParentInstance;
        MovieClipInstance* mpaChildInstances;
        CgsGraphics::Im2dTransform* mpaTransforms;
        TextFieldInstance* mpaTextFieldInstances;
        f32 mfTimeTillNextFrame;
        u16 muCurrentFrame;
        u16 muLastKeyFrameApplied;
        FrameTriggerCallback mpFrameTriggerCallback;
        void* mpFrameTriggerUserData;
        u16 muParameterIndex;
        u8 mxFlags;
    };
}

#endif // BRN_FLAPT_MOVIE_CLIP_INSTANCE_H
