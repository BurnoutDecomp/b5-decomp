#ifndef BRN_FLAPT_MANAGER_H
#define BRN_FLAPT_MANAGER_H

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"          // BrnFlapt::FileRef
#include "GameSource/Gui/Flapt/BrnFlaptFileInstance.h"     // BrnFlapt::FlaptFileInstance (52-byte stride)

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptManager.h
//
// BrnFlapt::FlaptManager — owns the table of live FlaptFileInstances.
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Attested layout (this group):
//   +0x08  maFlaptFileInstances[]   (X360 GetFile: element = this + 8 + 52*index)
//          element stride 0x34 (52 bytes) == sizeof(FlaptFileInstance).
//
// Only the array base proven by GetFile is named; the leading 8 bytes and the
// array length are not attested here, so the array is modeled as a declared-only
// element block reached via the attested base offset. Grow this header additively
// as the other FlaptManager TUs (Construct / Prepare / Update / Render /
// RegisterFlaptFile / Release / Destruct / SetSoundTriggerHandler) land.
// ============================================================================

namespace BrnFlapt
{
    struct FlaptManager
    {
        // GetFile @ 0x82473078 : index maFlaptFileInstances[luFile], assert the
        // entry IsActive(), and return a FileRef {&entry} by value into the
        // caller-provided out buffer.
        FileRef* GetFile(FileRef* lpOutRef, u32 luFile);

        u8                mau8Opaque00[8];          // +0x00..0x07  (not attested here)
        FlaptFileInstance maFlaptFileInstances[1];  // +0x08  (true length not attested; >=1)
    };
}

#endif // BRN_FLAPT_MANAGER_H
