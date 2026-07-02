#include "GameSource/Gui/SatNav/BrnEventIconManager.h"

#include <cstring>   // std::memcpy (the icon-table adopt)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnGui::EventIconManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions, DWARF primary file
// GameSource/Gui/SatNav/BrnEventIconManager.cpp):
//   EventIconManager::Update2DIcons          @0x824F5938  (CrashNavIconRenderer::RenderIcons)
//   EventIconManager::GetEventIconPositions  @0x824F59C8  (the MapIconManager icon walks)
//   EventIconManager::GetEventIDForIconIndex @0x824F5A78  (MapIconManager::GetEventIDAtIndex)
//
// All three are straight table operations over the 175-slot {x, y, id} bank; every
// assert is a non-gating tripwire (the X360 falls through after firing).

namespace BrnGui
{
    // @ 0x824F5938
    void EventIconManager::Update2DIcons(const EventIcon2D* lpaEventIcons, s32 liNumIcons)
    {
        CGS_ASSERT(liNumIcons <= KI_MAX_2DEVENTICONS, "liNumIcons <= KI_MAX_2DEVENTICONS");
        CGS_ASSERT(lpaEventIcons != NULL, "lpaEventIcons");

        std::memcpy(ma2DEventIcons, lpaEventIcons, sizeof(EventIcon2D) * liNumIcons);
        miNumEventIcons = liNumIcons;
    }

    // @ 0x824F59C8
    void EventIconManager::GetEventIconPositions(Vector2* lv2IconPositions, s32* lpiNumIcons)
    {
        CGS_ASSERT(lpiNumIcons != NULL, "lpiNumIcons");

        // One 16-byte lane store per icon: {x, y, 0, 0} (the X360 zeroes the zw pair
        // before the lvx/stvx copy -- Vector2::Set semantics).
        for (s32 liIconIndex = 0; liIconIndex < miNumEventIcons; ++liIconIndex)
        {
            lv2IconPositions[liIconIndex] =
                Vector2{ ma2DEventIcons[liIconIndex].mfEventIconPosX,
                         ma2DEventIcons[liIconIndex].mfEventIconPosY, 0.0f, 0.0f };
        }

        // The X360 re-checks the out-pointer before the count store (the assert above
        // does not gate).
        if (lpiNumIcons != NULL)
            *lpiNumIcons = miNumEventIcons;
    }

    // @ 0x824F5A78
    u32 EventIconManager::GetEventIDForIconIndex(s32 liIconIndex) const
    {
        CGS_ASSERT(liIconIndex >= 0, "liIconIndex >= 0");
        CGS_ASSERT(liIconIndex < miNumEventIcons, "liIconIndex < miNumEventIcons");

        return ma2DEventIcons[liIconIndex].muEventID;
    }
}
