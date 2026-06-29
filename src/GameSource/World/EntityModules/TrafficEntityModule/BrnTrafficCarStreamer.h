#pragma once

// =============================================================================
// BrnTrafficCarStreamer.h  (NEW OWNING HEADER -- partial)
//
// Canonical home recovered from the X360 assert strings baked into this class's
// own bodies (BURNOUT_X360_ARTIST.XEX):
//   "...code\\gamesource\\world\\entitymodules\\trafficentitymodule\\BrnTrafficCarStreamer.h"
//   "...code\\gamesource\\unity\\../World/EntityModules/TrafficEntityModule/BrnTrafficCarStreamer.cpp"
// so this header mirrors that path under b5-decomp/src.
//
// NO DecFIGS DWARF entry was found for this class, and only 4 of its functions are
// in the X360 ledger (TrafficCarStreamer ctor, AreAllAssetsLoaded,
// IsTrafficAssetLoaded, GetBonusAssets). The class is therefore reconstructed
// OFFSET-FIRST from the ASM: members the 4 bodies actually touch are named at their
// proven byte offsets, and the (large) gaps between them are reserved with explicit
// padding buffers so the named members stay pinned. Future slices that recover more
// of this class MUST grow this header additively / replace padding with named
// members -- never move a proven offset.
//
// PROVEN OFFSETS (all from the ASM in this TU's postmortem packet):
//   +0x0000        vtable pointer (ctor stores off_820D0F00 at +0)         -> polymorphic
//   +0x0018 (24)   a byte zeroed by the ctor (`stb 0,0x18(r3)`)            -> mbAssetsRegistered
//   +0x2578 (9592) u8[64] asset-usage flags; low 2 bits read via `&3`
//                  (AreAllAssetsLoaded skips ==0, GetBonusAssets selects ==2)
//   +0x25B8 (9656) u8[64] asset load-state; IsTrafficAssetLoaded returns (byte==2)
//                  (0x25B8 - 0x2578 == 64 == KU_MAX_VEHICLE_ASSETS, so the two
//                   byte arrays are adjacent and each spans the whole asset table)
//   +0x27F8 (10232) array[64] of 32-byte per-asset records; the ctor zero-inits
//                  each and seeds three self-pointers at +0xC/+0x10/+0x14 of every
//                  record (an embedded circular list head). 64*32 == 0x800 ends at
//                  exactly +0x2FF8.
//   +0x2FF8 (12280) u32 live count of known assets (read as `*(this+12280)` /
//                  `v4[3070]`); loop bounds in all three query bodies.
// =============================================================================

#include "types.hpp"                                    // u8/u32/s32
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT

namespace BrnTraffic
{
    // The traffic car asset streamer: owns the fixed table of streamed vehicle
    // assets (graphics/wheel specs) the traffic system can draw from, tracks which
    // are requested / loaded, and answers load-state queries for the entity module.
    class TrafficCarStreamer
    {
    public:
        // KU_MAX_VEHICLE_ASSETS -- IsTrafficAssetLoaded asserts `luAssetId < 0x40`
        // (cmplwi r28,0x40 @ 0x82706178). The two per-asset byte arrays are exactly
        // 0x40 apart, confirming the table capacity.
        static const u32 KU_MAX_VEHICLE_ASSETS = 64;

        // KU_MAX_BONUS_STREAMED_ASSETS -- GetBonusAssets asserts
        // `luBonusAssetsUsed < KU_MAX_BONUS_STREAMED_ASSETS` against the literal 2
        // (cmplwi r30,2 @ 0x8274F958), and the caller's out-array is sized for it.
        static const u32 KU_MAX_BONUS_STREAMED_ASSETS = 2;

        // ctor @ 0x827E3E98 (the only body that writes the +0x27F8 record table).
        TrafficCarStreamer();

        // Polymorphic: the ctor installs a vtable pointer at +0 (off_820D0F00).
        virtual ~TrafficCarStreamer();

        // 0x82706288 -- every requested asset (usage low-2-bits != 0) is loaded.
        bool AreAllAssetsLoaded() const;

        // 0x82706160 -- load-state byte for an asset == 2 (loaded).
        bool IsTrafficAssetLoaded(u32 luAssetId) const;

        // 0x8274F8C0 -- collect the indices of the assets currently flagged as
        // bonus (usage low-2-bits == 2) into lpauOutBonusAssets, writing the count
        // into *lpuOutNumBonusAssets. lpauOutBonusAssets must hold at least
        // KU_MAX_BONUS_STREAMED_ASSETS bytes (the indices are stored as bytes:
        // `stbx r31,r30,r25` @ 0x8274F978).
        void GetBonusAssets(u8* lpauOutBonusAssets, u32* lpuOutNumBonusAssets) const;

    private:
        // One streamed-asset record (32 bytes). The ctor (0x827E3E98) zero-fills the
        // record and points three pointer slots back at the record itself -- an empty
        // circular list head. Only the slots the ctor writes are named; the trailing
        // dword the ctor leaves untouched is reserved.
        struct AssetRecord
        {
            u32  muField0;     // +0x00  ctor: 0
            u32  muField1;     // +0x04  ctor: 0
            u32  muField2;     // +0x08  ctor: 0
            void* mpHeadA;     // +0x0C  ctor: &self (circular list head)
            void* mpHeadB;     // +0x10  ctor: &self
            void* mpHeadC;     // +0x14  ctor: &self
            u32  muField6;     // +0x18  ctor: 0
            u32  muReserved7;  // +0x1C  not written by ctor
        };

        // +0x0000 vtable occupies the first pointer slot (supplied by `virtual`).

        // +0x0004 .. +0x0017 -- members not touched by any reconstructed body.
        u8 mPad_0004[0x18 - 0x04];

        // +0x0018 -- ctor zeroes this byte (`stb 0,0x18(r3)`). Modelled as a flag.
        bool mbField0018;

        // +0x0019 .. +0x2577 -- not touched by any reconstructed body.
        u8 mPad_0019[0x2578 - 0x19];

        // +0x2578 (9592) -- per-asset usage flags. The query bodies only ever read
        // the low 2 bits (`clrlwi r11,r11,30`): 0 == slot unused, 2 == bonus-in-use.
        u8 mauAssetUsageFlags[KU_MAX_VEHICLE_ASSETS];

        // +0x25B8 (9656) -- per-asset load state. IsTrafficAssetLoaded returns
        // (byte == 2). Adjacent to the usage array (9656 - 9592 == 64).
        u8 mauAssetLoadState[KU_MAX_VEHICLE_ASSETS];

        // +0x25F8 .. +0x27F7 -- not touched by any reconstructed body.
        u8 mPad_25F8[0x27F8 - 0x25F8];

        // +0x27F8 (10232) -- the per-asset record table; ctor seeds all 64 entries.
        AssetRecord maAssetRecords[KU_MAX_VEHICLE_ASSETS];

        // +0x2FF8 (12280) -- live count of known assets (loop bound in every query).
        u32 muNumKnownAssets;
    };
}
