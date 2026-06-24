#ifndef BRN_FLAPT_MOVIE_CLIP_REF_H
#define BRN_FLAPT_MOVIE_CLIP_REF_H

#include "types.hpp"

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h
//
// BrnFlapt::MovieClipRef — a lightweight, returned-by-value handle onto a live
// MovieClipInstance. Reconstructed from BURNOUT_X360_ARTIST.XEX. The handle
// stores the referenced MovieClipInstance pointer in its first slot; a second
// slot is initialised to 0 when constructed by a parent accessor (e.g.
// FlaptFileInstance::GetRootMovieClip, which writes {clip, 0}). MovieClipRef
// methods (SetVisible, FindChildMovieClip, GetParent, ...) operate through that
// stored instance pointer.
//
// Only the two stored slots are modeled (the layout proven by the attested
// constructors/accessors). This is the single declaration home for the type so
// the various MovieClipRef TUs share one definition (no ODR fork).
// ============================================================================

namespace BrnFlapt
{
    struct MovieClipRef
    {
        // +0x00 : the referenced live MovieClipInstance (raw pointer; the X360
        // dereferences it as a byte-pointer in the inline accessors, so it is
        // modeled opaquely as void* here).
        void* mpClipInstance;   // +0x00
        u32   mu04;             // +0x04  (zeroed at construction by GetRootMovieClip)
    };
}

#endif // BRN_FLAPT_MOVIE_CLIP_REF_H
