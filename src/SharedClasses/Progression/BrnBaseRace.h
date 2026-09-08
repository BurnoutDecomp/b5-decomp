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

// ---- [progression wave: lifecycle, 2026-09-06] ADDITIVE: two bodies, NO layout change --------
// SetName / SetId are the two BaseRace setters the console INLINES into
// BrnProgression::ProgressionManager::HACK_SetupRaces @0x82366968, which is the first
// reconstructed caller (Prepare2 -> ProcessLoadedPresetRaces -> HACK_SetupRaces). Both were
// declaration-only above; they are bodied HERE, in the owning header, because the console has no
// standalone symbol for either -- they exist only inlined, so there is no TU to put them in. The
// asm emits them per race as exactly:
//     0x823669D8  li  r5, 0x20 ; bl strncpy   -- strncpy(macName, "Hack 0N", KI_NAME_LENGTH)
//     0x82366A08  stb r28, 0x1F(race)         -- macName[KI_NAME_LENGTH - 1] = '\0'
//     0x823669EC  std r10, 0x20(race)         -- mId = 0x6E5D8 + N, a full-width 64-bit store
// GROW this header when a BaseRace TU lands; do not fork.
inline void
BaseRace::SetName(const char* lpcName)
{
    for (s32 liChar = 0; liChar < KI_NAME_LENGTH; ++liChar)          // strncpy(dst, src, 32)
    {
        macName[liChar] = lpcName[liChar];
        if (lpcName[liChar] == '\0')
        {
            // strncpy zero-pads the remainder of the 32-byte field.
            for (s32 liPad = liChar; liPad < KI_NAME_LENGTH; ++liPad)
            {
                macName[liPad] = '\0';
            }
            break;
        }
    }
    macName[KI_NAME_LENGTH - 1] = '\0';                              // `stb 0, 0x1F(race)`
}

inline void
BaseRace::SetId(CgsID lId)
{
    mId = lId;                                                       // `std r10, 0x20(race)`
}

inline CgsID
BaseRace::GetId() const
{
    return mId;
}

// ---- [p0 map-event wave, 2026-09-08] ADDITIVE: one body, NO layout change ---------------------
// SetFlag has no standalone symbol either -- it exists only inlined. The first reconstructed
// caller is BrnGui::CrashNavMapEvent::HandleSelect, whose E_CREATE_EVENT_NEW_PANEL arm emits a
// read-modify-write OR of the flag bit into mxFlags (+0x28) over the newly constructed race,
// with the bit set being 1 == E_FLAG_CUSTOM. GROW this header when a BaseRace TU lands; do not
// fork.
inline void
BaseRace::SetFlag(Flag leFlag)
{
    mxFlags = static_cast<u8>(mxFlags | static_cast<u8>(leFlag));
}

}

#endif // BRN_BASE_RACE_H
