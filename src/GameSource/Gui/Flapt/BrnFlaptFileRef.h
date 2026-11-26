#ifndef BRN_FLAPT_FILE_REF_H
#define BRN_FLAPT_FILE_REF_H

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"   // BrnFlapt::MovieClipRef
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT_AT (Construct)

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
    struct FlaptFileInstance;

    struct FileRef
    {
        // DWARF BrnFlaptFileRef.h:53; no ARTIST copy (EventInfoComponent::Construct stores 0).
        void ConstructEmpty() { mpFileInst = 0; }

        // DWARF BrnFlaptFileRef.h:58, defined below; ARTIST has no copy (FlaptManager::GetFile
        // 0x824730C8 inlines it with its h:102 "lpFileInst" assert).
        void Construct(FlaptFileInstance* lpFileInst);

        // ARTIST 0x8246EC90 uses the hidden result buffer for the returned clip.
        MovieClipRef FindComponent(const char* lpcName) const;

        // GetRootMovieClip @ 0x8246B410 : return a MovieClipRef onto the referenced
        // file instance's root timeline, by value into lpOutRef.
        MovieClipRef GetRootMovieClip() const;

        // +0x00 : the referenced live FlaptFileInstance (DWARF h:73 mpFileInst).
        FlaptFileInstance* mpFileInst;   // +0x00
    };

    inline void FileRef::Construct(FlaptFileInstance* lpFileInst)
    {
        CGS_ASSERT_AT(lpFileInst, "lpFileInst", "..\\..\\..\\GameSource\\Gui/Flapt/BrnFlaptFileRef.h", 102);
        mpFileInst = lpFileInst;
    }
}

#endif // BRN_FLAPT_FILE_REF_H
