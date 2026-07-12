#ifndef BRN_GUI_FLAPT_ICON_COMPONENT_H
#define BRN_GUI_FLAPT_ICON_COMPONENT_H

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"  // BrnFlaptComponent (base)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                        // MovieClipRef (embedded)

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h
//
// BrnGui::FlaptIconComponent       -- an apt-driven icon whose visible "state" is a
//                                     hashed timeline label; switching state plays /
//                                     stops the clip on the matching label.
// BrnGui::FlaptAnimatorComponent   -- a FlaptIconComponent that also drives up to
//                                     KI_MAX_ANIMATOR_CHILD_MOVIE_CLIPS named child
//                                     clips named by the clip's trigger parameters.
//
// Both derive (transitively) from BrnFlaptComponent. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; member names/types/enums/vtable shape from the DecFIGS
// DWARF (BrnGuiFlaptIconComponent.h), gated on the X360 ledger.
//
// LAYOUT (X360-attested by the Construct / Prepare / SetState / Run asm). These are
// POLYMORPHIC subclasses, so the vptr sits at +0x00 and the BrnFlaptComponent base
// subobject follows at +0x04:
//   FlaptIconComponent
//     +0x00  vptr
//     +0x04  BrnFlaptComponent base (mpStateInterface @ +0x04, mAptRef @ +0x08)
//     +0x10  muCurrentStateHash       (stw 0, 0x10(this))
//     sizeof 0x14
//   FlaptAnimatorComponent : FlaptIconComponent
//     +0x14  maChildMovieClips[4]     (MovieClipRef, 8 bytes each -> +0x14..+0x33)
//     +0x34  miNumChildMovieClips     (stw, 0x34(this))
//     sizeof 0x38
// All member access is BY NAME.
// ============================================================================

namespace BrnFlapt
{
    struct FileRef;
    struct MovieClipRef;
}

namespace BrnGui
{
    class FlaptIconComponent : public BrnFlaptComponent
    {
    public:
        // Construct @ 0x8241C1F0 -- bind the state interface, clear the movie-clip
        // handle, and reset the current-state hash. DWARF shape
        // Construct(const void*, StateInterface*, const void*); the body uses only
        // the state interface (r5).
        virtual void Construct(const void* lpDEBUGName,
                               CgsGui::StateInterface* lpStateInterface,
                               const void* lpcParentName);

        // Prepare @ 0x8241C250 -- assert the name, forward up to BrnFlaptComponent::
        // Prepare to bind the clip, then reset the current-state hash to 0.
        virtual void Prepare(const char* lacName,
                             const BrnFlapt::FileRef& lFile,
                             const char* lacParentName);

        // Prepare(const MovieClipRef*) @ 0x8241C2B8 -- adopt an already-resolved
        // clip: a bare tail-forward onto BrnFlaptComponent::Prepare(const
        // MovieClipRef*) @0x8240E740 (no state-hash reset). Bodied in this TU.
        virtual void Prepare(const BrnFlapt::MovieClipRef* lpMovieClipRef);

        // SetState @ 0x8241C2C0 -- hash lpcStateName; if it differs from the current
        // state, goto-and-PLAY that label on the icon clip and remember it.
        virtual void SetState(const char* lpcStateName);

        // GotoAndStopLabel @ 0x8241C3D8 -- hash lpcStateName; if it differs from the
        // current state, goto-and-STOP that label on the icon clip and remember it.
        virtual void GotoAndStopLabel(const char* lpcStateName);

    protected:
        u32 muCurrentStateHash;   // +0x10
    };

    class FlaptAnimatorComponent : public FlaptIconComponent
    {
    public:
        // Construct @ 0x8241C4E8 -- as the base icon, plus clear the child-clip array
        // and its count.
        virtual void Construct(const void* lpDEBUGName,
                               CgsGui::StateInterface* lpStateInterface,
                               const void* lpcParentName);

        // Prepare @ 0x8241C568 -- bind the icon clip, then for each name in the clip's
        // trigger parameters (capped at KI_MAX_ANIMATOR_CHILD_MOVIE_CLIPS) find the
        // named child clip under this clip's parent and store it; finally hide the clip.
        virtual void Prepare(const char* lacName,
                             const BrnFlapt::FileRef& lFile,
                             const char* lacParentName);

        // RefreshControlledMovieClips @ 0x8241C668 -- re-resolve the controlled child
        // clips against the clip's current frame (using the same trigger-parameter
        // name list).
        void RefreshControlledMovieClips();

        // Run @ 0x8241C710 -- goto-and-play lpcFrameName on every controlled child clip.
        void Run(const char* lpcFrameName);

    private:
        static const s32 KI_MAX_ANIMATOR_CHILD_MOVIE_CLIPS = 4;

        BrnFlapt::MovieClipRef maChildMovieClips[KI_MAX_ANIMATOR_CHILD_MOVIE_CLIPS]; // +0x14
        s32                    miNumChildMovieClips;                                 // +0x34
    };
}

#endif // BRN_GUI_FLAPT_ICON_COMPONENT_H
