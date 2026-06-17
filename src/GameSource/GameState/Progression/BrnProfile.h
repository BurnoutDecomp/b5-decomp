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

// Minimal owning slice for BrnProgression::MugshotInfo -- the element type of
// Array<BrnProgression::MugshotInfo, 20>. The DWARF in this batch shows the *PS3*
// instantiation Array<MugshotInfo,30u> (N=30) but the X360 BINARY IS AUTHORITATIVE and
// proves N=20 and a 56-byte stride:
//   * MugshotInfo,20>::GetItem @0x8235ED28 returns `56 * index + this` -> 56-byte stride
//   * the live-count word lands at byte +1120 == 20 * 56 -> capacity N == 20
//   * MugshotInfo,20>::IsFull @0x823679B8 returns `count == 20`
//   * Append/Erase copy 7 qwords (7 * 8 == 56 bytes) per element -> 56-byte stride
// So the X360 game ships Array<MugshotInfo,20> (the DWARF "30u" is PS3 drift; noted).
//
// Only the shape needed to give the element a real 56-byte size is sliced here. The full
// internal layout (incl. the nested MugshotInfo::UniquePlayerID referenced by
// BrnNetworkLiveRevengeRelationship.h:326) is NOT yet reverse-engineered; modelled as an
// opaque 56-byte buffer so the container stride/count offsets are exact without fabricating
// member names. The owning TU of MugshotInfo's real members must replace this pad.
struct MugshotInfo
{
    // Nested unique-player identifier referenced cross-subsystem (DWARF
    // BrnNetworkLiveRevengeRelationship.h:326 "MugshotInfo::UniquePlayerID mUniqueID").
    // Declared-only: its real layout is not recovered here.
    struct UniquePlayerID;

private:
    // Opaque 56-byte body (X360-proven stride). Replace with the real named members
    // (incl. UniquePlayerID) when MugshotInfo's own TU is reconstructed.
    u8 mPad_Body[56];   // sizeof(MugshotInfo) == 56 (X360 Array<MugshotInfo,20>::GetItem stride)
};
}

#endif // BRN_PROFILE_H
