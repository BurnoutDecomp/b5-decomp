#ifndef BRN_FLAPT_MOVIE_CLIP_REF_H
#define BRN_FLAPT_MOVIE_CLIP_REF_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2, Vector4, VecFloat (the transform accessors take these by value)

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
// The accessors bodied in BrnFlaptMovieClipRef.cpp are the navigation / lookup /
// playback forwarders PLUS the TRANSFORM-mutating accessors -- SetPosition,
// SetPositionY, SetColour, SetColourScale, SetSizeScale, SetRotation and
// GetPosition (the "class:BrnFlapt::MovieClipRef" TU). The latter take
// Vector2 / Vector4 / VecFloat by value and rewrite the four Vector4 rows of the
// referenced CgsGraphics::Im2dTransform (held in mpTransform). The X360 emitted
// those rewrites as VMX (vperm/vrlimi/vrsqrtefp/cos/sin); they are reconstructed
// in the .cpp as ordinary named-component math on the Im2dTransform rows. The
// math-param types are aliased in BrnCommonTypes.h; mpTransform stays an opaque
// void* in this widely-included declaration home and is cast to
// CgsGraphics::Im2dTransform* in the .cpp (which includes CgsIm2dTransform.h), so
// no Im2d/RenderWare cascade is forced on this header's many includers.
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
        // ---- validity ------------------------------------------------------
        // ADDITIVE GROW (BrnGui::BaseOverlayState::SetupOverlayComponents
        // @0x824B18B8, which inlines both: the `lwz 0(ref)` null-check behind the
        // "true == lpTransitionMovieClipRef->IsValid( )" assert text, and the
        // paired zero stores of BaseOverlayState's reset path; DWARF declares both
        // on MovieClipRef -- BrnFlaptMovieClipRef.h:15). A ref is valid when it
        // points at a live instance; SetInvalid clears both handles.
        bool IsValid() const   { return NULL != mpMovieClipInst; }
        void SetInvalid()      { mpMovieClipInst = NULL; mpTransform = NULL; }

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

        // GotoAndPlayLabel @ 0x8246F3E8 : hash lpcLabel, jump to it and play (the
        // string-keyed counterpart to the hash form above; bodied in its own sibling
        // TU). Used by FlaptAnimatorComponent::Run to play each controlled clip.
        void GotoAndPlayLabel(const char* lpcLabel) const;

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

        // ---- transform accessors -------------------------------------------
        // These mutate / read the referenced Im2dTransform (mpTransform). Each
        // asserts mpMovieClipInst and mpTransform non-null, then reads/writes the
        // named .x/.y/.z/.w lanes of the transform's Vector4 rows. Signatures are
        // taken from the DecFIGS DWARF (BrnFlaptMovieClipRef.h) and gated on the
        // X360 ledger. (X360 addresses noted per method.)

        // GetPosition @ 0x8241E340 : return the clip's origin position
        // (mOriginXYZ.x, mOriginXYZ.y) by value.
        Vector2 GetPosition() const;

        // SetPosition @ 0x8241DFC0 : set the origin position to lPosition
        // (mOriginXYZ.x/.y; the z/w lanes are cleared).
        void SetPosition(Vector2 lPosition);

        // SetPositionY @ 0x8241E228 : set only the origin's Y (mOriginXYZ.y),
        // preserving X; the z/w lanes are cleared.
        void SetPositionY(VecFloat lvfPositionY);

        // SetRotation @ 0x827DD618 : build the right/up basis (mRightUp) from the
        // rotation angle: Right=(cos,sin), Up=(-sin,cos).
        void SetRotation(f32 lfAngle);

        // SetSizeScale @ 0x82428518 : normalise the existing right/up basis to
        // unit length, then scale Right by lScale.x and Up by lScale.y (mRightUp).
        void SetSizeScale(Vector2 lScale);

        // SetColour @ 0x8241E0D0 : set a flat colour -- write lColour.rgb into the
        // additive colour shift (mColourShift) and zero the colour scale's rgb
        // (mColourScale); both alpha (w) lanes are preserved.
        void SetColour(Vector4 lColour);

        // SetColourScale @ 0x8240E588 : set the multiplicative colour scale
        // (mColourScale) to lColourScale (all four lanes).
        void SetColourScale(Vector4 lColourScale);

        BrnFlapt::MovieClipInstance* mpMovieClipInst;   // +0x00
        void*                        mpTransform;        // +0x04  Im2dTransform*
    };
}

#endif // BRN_FLAPT_MOVIE_CLIP_REF_H
