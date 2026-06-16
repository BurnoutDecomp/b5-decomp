#ifndef BRN_PROFILE_H
#define BRN_PROFILE_H

#include "types.hpp"

// Minimal owning slice for BrnProgression::ProfileEvent -- the element type of
// Array<BrnProgression::ProfileEvent,175>. DWARF home is BrnProfile.h:312.
// 8-byte stride is PROVEN by the X360 Array<ProfileEvent,175>::GetItem body
// (`return 8 * index + this`) and the count word landing at +1400 == 175*8:
//   muEventID(4) + muFlags(2) + 2 pad = 8.
// Only the shape needed to give the element a real 8-byte size is sliced here.

namespace BrnProgression
{
struct ProfileEvent
{
    // DWARF BrnProfile.h:296 (Flags enum).
    enum Flags
    {
        E_FLAG_UNDISCOVERED             = 0,
        E_FLAG_DISCOVERED               = 1,
        E_FLAG_FINISHED                 = 2,
        E_FLAG_RANK_WIN                 = 4,
        E_FLAG_NON_RANK_WIN             = 8,
        E_FLAG_WON_SPECIAL_EVENT_BEFORE = 16,
        E_FLAG_WON_EVENT_BEFORE         = 32
    };

    void Construct(u32 luEventID);
    u32  GetID() const;
    u16  GetFlags() const;
    void SetFlags(u16 lu16Flags);
    bool IsFlagSet(Flags leFlag) const;
    void EnableFlags(u16 lu16Flags);
    void ClearFlags(u16 lu16Flags);

private:
    u32 muEventID;   // BrnProfile.h:335  (+0)
    u16 muFlags;     // BrnProfile.h:336  (+4)
    // 2 bytes trailing pad -> 8-byte stride (X360 GetItem: 8*index)
};
}

#endif // BRN_PROFILE_H
