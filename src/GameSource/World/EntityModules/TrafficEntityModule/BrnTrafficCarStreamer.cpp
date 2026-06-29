// =============================================================================
// BrnTrafficCarStreamer.cpp
//
// BrnTraffic::TrafficCarStreamer -- the traffic system's streamed-vehicle-asset
// table and its load-state queries. Reconstructed from the X360 ARTIST build
// (BURNOUT_X360_ARTIST.XEX); the canonical path is baked into the class's own
// assert strings. See BrnTrafficCarStreamer.h for the offset map and the
// "no DWARF / offset-first" reconstruction note.
//
// Bodies recovered here (X360 addresses):
//   TrafficCarStreamer()           @ 0x827E3E98
//   AreAllAssetsLoaded()           @ 0x82706288
//   IsTrafficAssetLoaded(u32)      @ 0x82706160
//   GetBonusAssets(u8*, u32*)      @ 0x8274F8C0
// =============================================================================

#include "BrnTrafficCarStreamer.h"

namespace BrnTraffic
{
    // 0x827E3E98 -- zero the per-asset record table and seed each record's three
    // circular-list-head pointers at &self. The X360 walks the 64 records by a
    // 0x20-byte stride (`addi r11,r11,0x20`, loop count r9 = 0x3F down to 0).
    // The vtable install (`stw off_820D0F00,0(r3)`) and the +0x18 zero
    // (`stb 0,0x18(r3)`) are produced by the C++ object setup below.
    TrafficCarStreamer::TrafficCarStreamer()
        : mbField0018(false)
        , muNumKnownAssets(0)   // not written by the X360 ctor; constructed empty
    {
        for (u32 luAsset = 0; luAsset < KU_MAX_VEHICLE_ASSETS; ++luAsset)
        {
            AssetRecord& lrRecord = maAssetRecords[luAsset];
            lrRecord.muField0    = 0;
            lrRecord.muField1    = 0;
            lrRecord.muField2    = 0;
            lrRecord.mpHeadA     = &lrRecord;   // +0x0C
            lrRecord.mpHeadB     = &lrRecord;   // +0x10
            lrRecord.mpHeadC     = &lrRecord;   // +0x14
            lrRecord.muField6    = 0;
            // +0x1C (muReserved7) is intentionally left untouched, matching the X360.
        }
    }

    TrafficCarStreamer::~TrafficCarStreamer()
    {
    }

    // 0x82706160 -- an asset is "loaded" when its load-state byte equals 2.
    bool TrafficCarStreamer::IsTrafficAssetLoaded(u32 luAssetId) const
    {
        CGS_ASSERT(luAssetId < KU_MAX_VEHICLE_ASSETS, "luAssetId < KU_MAX_VEHICLE_ASSETS");
        CGS_ASSERT(luAssetId < muNumKnownAssets,
                   "Attempted to use a traffic car asset that is not known about");

        return mauAssetLoadState[luAssetId] == 2;
    }

    // 0x82706288 -- every requested asset is loaded. A slot counts as "requested"
    // when its usage low-2-bits are non-zero (`(flags & 3) != 0`); for each such
    // slot the asset must already be loaded, otherwise the table is not ready.
    bool TrafficCarStreamer::AreAllAssetsLoaded() const
    {
        for (u32 luAsset = 0; luAsset < muNumKnownAssets; ++luAsset)
        {
            if ((mauAssetUsageFlags[luAsset] & 3) != 0
                && !IsTrafficAssetLoaded(luAsset))
            {
                return false;
            }
        }
        return true;
    }

    // 0x8274F8C0 -- gather the indices of all assets currently flagged as bonus
    // (usage low-2-bits == 2) into lpauOutBonusAssets (byte indices), and write the
    // count to *lpuOutNumBonusAssets.
    void TrafficCarStreamer::GetBonusAssets(u8* lpauOutBonusAssets,
                                            u32* lpuOutNumBonusAssets) const
    {
        CGS_ASSERT(lpauOutBonusAssets != 0, "lpauOutBonusAssets");
        CGS_ASSERT(lpuOutNumBonusAssets != 0, "lpuOutNumBonusAssets");

        u32 luBonusAssetsUsed = 0;
        for (u32 luAsset = 0; luAsset < muNumKnownAssets; ++luAsset)
        {
            if ((mauAssetUsageFlags[luAsset] & 3) == 2)
            {
                CGS_ASSERT(luBonusAssetsUsed < KU_MAX_BONUS_STREAMED_ASSETS,
                           "luBonusAssetsUsed < KU_MAX_BONUS_STREAMED_ASSETS");
                lpauOutBonusAssets[luBonusAssetsUsed] = static_cast<u8>(luAsset);
                ++luBonusAssetsUsed;
            }
        }

        *lpuOutNumBonusAssets = luBonusAssetsUsed;
    }
}
