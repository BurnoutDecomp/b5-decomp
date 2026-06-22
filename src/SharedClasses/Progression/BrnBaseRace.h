#ifndef BRN_BASE_RACE_H
#define BRN_BASE_RACE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

// =============================================================================
// BrnBaseRace.h  (OWNING HEADER for BrnProgression::BaseRace)
//
// DWARF home: SharedClasses/Progression/BrnBaseRace.h. BaseRace is the common base of
// BrnProgression::Race (and BrnProgression::SavedRace). This is a MINIMAL OWNING SLICE:
// the full DWARF member list (so derived layouts line up) plus only the methods the
// reconstructed callers in this batch touch. The remaining attested API (SetName, GetId,
// GetFlag, ... ) is declared-only; its bodies land with the BaseRace TU. Single owner --
// grow here, do not fork.
//
// LAYOUT is X360-faithful (DWARF BrnBaseRace.h:52). 48 bytes: a 32-byte name buffer, the
// 8-byte CgsID, three u8 fields and 5 bytes of trailing pad so the derived Race members
// start at byte 0x30.
// =============================================================================

namespace BrnProgression
{

struct BaseRace
{
    // DWARF BrnBaseRace.h:57. Race-type bit flags.
    enum Flag
    {
        E_FLAG_CUSTOM       = 1,
        E_FLAG_CRASHBREAKER = 2,
        E_FLAG_ELIMINATOR   = 4,
        E_FLAG_STUNT        = 8,
        E_FLAG_SURVIVAL     = 16,
    };

    // DWARF BrnBaseRace.h:55. Length of the embedded name buffer.
    static const s32 KI_NAME_LENGTH = 32;

    // ---- X360-attested API (bodies in the BaseRace TU; declaration-only here) ----
    void        Construct();
    void        SetName(const char* lpcName);
    const char* GetName() const;
    CgsID       GetId() const;
    void        SetId(CgsID lId);
    u8          GetLaps() const;
    bool        IsLoop() const;
    void        SetLaps(u8 luLaps);
    bool        GetFlag(Flag leFlag) const;
    u8          GetFlags() const;
    void        SetFlag(Flag leFlag);
    void        SetFlags(u8 luFlags);
    void        ClearFlags();
    u8          GetRank() const;
    void        SetRank(u8 luRank);

protected:
    // Derived classes (Race) initialise these in their own Construct, so they are reachable.
    char  macName[KI_NAME_LENGTH];   // 0x00 (DWARF BrnBaseRace.h:121)
    CgsID mId;                       // 0x20 (DWARF :122)
    u8    mxFlags;                   // 0x28 (DWARF :123)
    u8    muRank;                    // 0x29 (DWARF :124)
    u8    muLaps;                    // 0x2A (DWARF :125)
    u8    maPad[5];                  // 0x2B (DWARF :127)
};

}

#endif // BRN_BASE_RACE_H
