#ifndef BRN_FLAPT_MOVIE_CLIP_REF_H
#define BRN_FLAPT_MOVIE_CLIP_REF_H

#include "types.hpp"

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h
//
// BrnFlapt::MovieClipRef -- a lightweight, returned-by-value handle onto a live
// MovieClipInstance plus the Im2dTransform that positions it. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; member names/types/shape taken from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h) and
// gated on the X360 ledger.
//
// Layout (two pointers, proven by every X360 accessor: `lwz 0(this)` reads
// mpMovieClipInst, `lwz 4(this)` reads mpTransform):
//   +0x00  mpMovieClipInst : the referenced live MovieClipInstance
//   +0x04  mpTransform     : its Im2dTransform (null for a root clip; the
//                            transform-mutating accessors assert it non-null)
//
// This is the single declaration home for the type so the various MovieClipRef
// TUs share one definition (no ODR fork).
//
// The accessors body in BrnFlaptMovieClipRef.cpp are the navigation / lookup /
// playback forwarders. The TRANSFORM-mutating accessors -- SetPosition,
// SetPositionY, SetColour, SetColourScale, SetSizeScale, SetRotation and
// GetPosition (the "class:BrnFlapt::MovieClipRef" TU) -- are NOT declared here
// yet: they take Vector2 / Vector4 / VecFloat by value and rewrite the rows of
// the CgsGraphics::Im2dTransform via VMX intrinsics. Those rw vector-math types
// and the Im2dTransform home header are not yet reconstructed in b5-decomp/src,
// so declaring (let alone bodying) them cleanly is blocked until those types
// exist -- see the TU block reason. They will be added additively here then.
// ============================================================================

namespace BrnFlapt
{
    // Used by-pointer only below; real declarations live in their home headers
    // (BrnFlaptMovieClipInstance.h, BrnFlaptTextFieldRef.h, CgsGuiShared).
    struct MovieClipInstance;
    struct TextFieldRef;
    struct TriggerParameters;

    struct MovieClipRef
    {
        // ---- named-child lookups -------------------------------------------
        // FindChildMovieClip @ 0x8246C740 : hash lpcName, forward to the
        // instance, which writes the located child's MovieClipRef into lpOutRef
        // (sret modeled, per the module house style, as a written-into buffer).
        MovieClipRef* FindChildMovieClip(MovieClipRef* lpOutRef,
                                         const char* lpcName) const;

        // FindChildMovieClipOnFrame @ 0x8246C8B0 : as above, restricted to the
        // children present on the instance's current frame.
        MovieClipRef* FindChildMovieClipOnFrame(MovieClipRef* lpOutRef,
                                                const char* lpcName) const;

        // FindChildTextField @ 0x8246C7F8 : hash lpcName, return a TextFieldRef
        // onto the named child text field (by value into lpOutRef).
        TextFieldRef* FindChildTextField(TextFieldRef* lpOutRef,
                                         const char* lpcName) const;

        // TryFindChildComponentRecursively @ 0x8246C968 : depth-first search for
        // a named component; on a hit writes lpOutMovieClipRef and returns true.
        bool TryFindChildComponentRecursively(const char* lpcName,
                                              MovieClipRef* lpOutMovieClipRef) const;

        // GetParent @ 0x8246CA40 : return a MovieClipRef onto the parent clip
        // (by value into lpOutRef).
        MovieClipRef* GetParent(MovieClipRef* lpOutRef) const;

        // ---- visibility ----------------------------------------------------
        // SetVisible @ 0x8246CA80 : set/clear the referenced clip instance's
        // visible flag (bit 0x02 of its flags byte) and return this Ref. Bodied
        // in BrnFlaptMovieClipInstance.cpp.
        MovieClipRef* SetVisible(bool lbVisible);

        // ---- timeline playback ---------------------------------------------
        // GotoAndPlayLabel @ 0x8246F388 : jump to a pre-hashed label and play.
        void GotoAndPlayLabel(u32 luLabelHash, const char* lpcDEBUGName) const;

        // GotoAndStopLabel @ 0x8246F498 : hash lpcLabel, jump to it and stop.
        void GotoAndStopLabel(const char* lpcLabel) const;

        // ---- trigger callbacks ---------------------------------------------
        // SetFrameTriggerCallback @ 0x8246CAB8 : install a per-frame callback.
        // lpCallback is MovieClipInstance::FrameTriggerCallback (void(*)(void*,
        // u16)); modeled as void* here to avoid pulling the instance header into
        // this widely-included declaration home -- the .cpp forwards the typed
        // pointer through.
        void SetFrameTriggerCallback(void* lpCallback, void* lpUserData) const;

        // GetTriggerParameters @ 0x8246CAB0 : the referenced clip's current
        // trigger parameters (tail-call forward to the instance).
        const TriggerParameters* GetTriggerParameters();

        BrnFlapt::MovieClipInstance* mpMovieClipInst;   // +0x00
        void*                        mpTransform;        // +0x04  Im2dTransform*
    };
}

#endif // BRN_FLAPT_MOVIE_CLIP_REF_H
