#ifndef BRN_FLAPT_FILE_REF_H
#define BRN_FLAPT_FILE_REF_H

#include "types.hpp"

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
        // +0x00 : the referenced live FlaptFileInstance (raw pointer; modeled as
        // void* — GetFile only stores the element address, and FileRef's own
        // accessors deref it via the FlaptFileInstance home).
        void* mpFileInstance;   // +0x00
    };
}

#endif // BRN_FLAPT_FILE_REF_H
