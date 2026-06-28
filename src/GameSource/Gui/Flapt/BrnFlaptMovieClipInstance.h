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
// This header presently declares only the entry points that the BrnFlapt::
// MovieClipRef forwarders body in BrnFlaptMovieClipRef.cpp need to compile:
// the named-lookup, navigation, label-playback and trigger-callback methods.
// The MovieClipInstance member layout and its full method set (Construct /
// Update / Render / ApplyKeyFrame / ...) live with this class's own TU; grow
// this header additively as those land. The instance pointer is therefore an
// incomplete-by-design but method-complete declaration for the Ref TU.
//
// Calling convention (verified from the ARTIST asm at the BrnFlaptMovieClipRef
// call sites): each method is invoked on the live instance pointer the Ref
// holds in its first slot; the sret-returning lookups (FindChild*, GetParent)
// take the caller's out handle by pointer and write it, so the DWARF renders
// them `void`.
// ============================================================================

namespace BrnFlapt
{
    struct MovieClipRef;
    struct TextFieldRef;
    struct TriggerParameters;

    struct MovieClipInstance
    {
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
        // SetFrameTriggerCallback @ 0x8246CAB8 call site.
        void SetFrameTriggerCallback(FrameTriggerCallback lpCallback,
                                     void* lpUserData);

        // GetTriggerParameters @ 0x8246CAB0 call site (tail-call forward).
        const TriggerParameters* GetTriggerParameters() const;
    };
}

#endif // BRN_FLAPT_MOVIE_CLIP_INSTANCE_H
