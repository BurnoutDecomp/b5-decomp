#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"  // the REAL record
#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT

// Split out of BrnGuiCache.cpp for the same reason as BrnGuiCache_GetNumEventStarts.cpp:
// that TU types the cache's embedded OptionsDataProfile via BrnGuiOptionsDataProfile.h, whose
// COMPILE-ONLY network/game-state slices clash with the real BrnNetwork types this body needs
// (its own header says so at the top). GuiCache::maPlayerInfo is byte storage precisely
// because BrnGuiCache.h can only forward-declare InGamePlayerStatusData; this TU has the
// complete type, so every field access below is BY NAME.

namespace BrnGui
{
    // The record model and the cache's byte-storage lane must agree, or the walk below steps
    // off the end of maPlayerInfo. The 312 is X360-authoritative (the console indexes the bank
    // with `312 * idx`), and BrnNetworkModuleInGamePlayerStatusInterface.h pins the struct to
    // it with an explicit trailing pad -- checked here rather than assumed.
    static_assert(sizeof(BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData) == 312,
                  "InGamePlayerStatusData must match the GuiCache maPlayerInfo lane stride");

    // GuiCache::GetOnlinePlayerInfoFromPlayerId -- X360 @0x82482738, exported UNNAMED as
    // sub_82482738. Its identity is settled by its own assert's __FILE__:
    // "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID" fired from
    // "..\\..\\..\\GameSource\\Gui/BrnGuiCache.h" -- a GuiCache header-inline, which is also why
    // IDA never named it. Declared (bodyless) in BrnGuiCache.h since the wave-B analyzer grow;
    // b5-decomp fd0925f4's BrnCarSelectLivery_wJ_01.cpp is the first MOUNTED caller, so the
    // exe stopped linking until this body existed.
    //
    // Body, store for store:
    //   * assert the id is not the invalid sentinel (-1);
    //   * walk the eight-record bank from `this + 44432` in 78-DWORD (312-byte) steps,
    //     comparing the DWORD at that address against the requested id. 44432 - 44160 == 272
    //     == InGamePlayerStatusData::mNetworkPlayerID, and 44160 is maPlayerInfo's own base,
    //     so the walk is `maPlayerInfo[i]->mNetworkPlayerID` and is written that way;
    //   * bail with NULL once eight records have been tried (`if (++v4 >= 8) return 0`);
    //   * otherwise return `312 * i + this + 44160`, i.e. &maPlayerInfo[i].
    // Returning NULL is IN CONTRACT -- every caller asserts non-null before dereferencing.
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
    GuiCache::GetOnlinePlayerInfoFromPlayerId(s32 liPlayerId) const
    {
        // CgsNetwork::K_INVALID_PLAYER_ID has no committed home yet; the console compares
        // against -1 (`cmpwi a2, -1`), which is that constant's value.
        CGS_ASSERT(liPlayerId != -1, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        // The console's bail-out counter is a literal 8; taken from the lane array itself so the
        // two can never drift apart.
        const s32 liNumRecords = static_cast<s32>(sizeof(maPlayerInfo) / sizeof(maPlayerInfo[0]));

        for (s32 liIndex = 0; liIndex < liNumRecords; ++liIndex)
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpRecord =
                reinterpret_cast<const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*>(
                    maPlayerInfo[liIndex]);

            if (lpRecord->mNetworkPlayerID == liPlayerId)
            {
                return lpRecord;
            }
        }

        return 0;
    }
}
