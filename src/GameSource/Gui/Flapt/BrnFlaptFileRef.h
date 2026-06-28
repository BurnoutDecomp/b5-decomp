#ifndef BRN_FLAPT_FILE_REF_H
#define BRN_FLAPT_FILE_REF_H

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"   // BrnFlapt::MovieClipRef

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptFileRef.h
//
// BrnFlapt::FileRef — a returned-by-value handle onto a live FlaptFileInstance,
// produced by FlaptManager::GetFile. Reconstructed from BURNOUT_X360_ARTIST.XEX.
// The handle stores the referenced FlaptFileInstance pointer in its first slot
// (the X360 GetFile writes `*out = &maFlaptFileInstances[index]`).
//
// Single declaration home for the type (shared by FlaptManager::GetFile and the
// FileRef::* accessors homed in BrnFlaptFileRef.cpp) so there is no ODR fork.
// ============================================================================

namespace BrnFlapt
{
    struct FileRef
    {
        // FindComponent @ 0x8246EC90 : hash lpcName and look the named component up
        // in the referenced FlaptFileInstance, returning a MovieClipRef by value
        // (modeled, per the module's house style, as a written-into out buffer).
        MovieClipRef* FindComponent(MovieClipRef* lpOutRef, const char* lpcName) const;

        // GetRootMovieClip @ 0x8246B410 : return a MovieClipRef onto the referenced
        // file instance's root timeline, by value into lpOutRef.
        MovieClipRef* GetRootMovieClip(MovieClipRef* lpOutRef) const;

        // +0x00 : the referenced live FlaptFileInstance (raw pointer; modeled as
        // void* — GetFile only stores the element address, and FileRef's own
        // accessors deref it via the FlaptFileInstance home).
        void* mpFileInstance;   // +0x00
    };
}

#endif // BRN_FLAPT_FILE_REF_H
