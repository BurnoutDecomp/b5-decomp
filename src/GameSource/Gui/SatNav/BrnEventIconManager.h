#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"   // BrnGui::GuiEventDrawEventIcons (EIconDisplayType... via GuiEventEnableSatNavIcons)

namespace CgsGui { struct StateInterface; }

// BrnGui::EventIconManager - the 2D event-icon bank behind the sat-nav/crash-nav map:
// a fixed 175-slot {x, y, event-id} table the icon renderers fill and query. DWARF home
// BrnEventIconManager.h:44. This TU bodies Update2DIcons / GetEventIconPositions /
// GetEventIDForIconIndex; Construct/Prepare/Update/ReleaseResources/Destruct are their
// own ledger functions (declaration-only here).
namespace BrnGui
{
    class GuiCache;

    struct EventIconManager
    {
        // DWARF BrnEventIconManager.h:55.
        static const s32 KI_MAX_2DEVENTICONS = 175;

        // DWARF h:48 -- one 2D icon record (12 bytes; the X360 indexes at stride 12).
        struct EventIcon2D
        {
            f32 mfEventIconPosX;   // h:50
            f32 mfEventIconPosY;   // h:51
            u32 muEventID;         // h:52
        };

        // DWARF h:59/h:69/h:73/h:96/h:100 -- declaration-only (their own ledger
        // functions). Prepare/ReleaseResources drive the GuiEventDrawEventIcons view
        // state; the display-type enum lives with the sat-nav event defs.
        void Construct();
        void Prepare(CgsGui::StateInterface* lpStateInterface, GuiCache* lpGuiCache,
                     f32 lfOptionalFadeDuration, s32 leNewEventIconType,
                     u32* lpuIconsToIgnore, s32 liNumIconsToIgnore);
        void Update();
        void ReleaseResources(CgsGui::StateInterface* lpStateInterface,
                              f32 lfOptionalFadeDuration);
        void Destruct();

        // @0x824F5938 (this TU, DWARF h:79) -- adopt a new icon table (bounded copy).
        void Update2DIcons(const EventIcon2D* lpaEventIcons, s32 liNumIcons);

        // @0x824F59C8 (this TU, DWARF h:85) -- copy the icon positions out as Vector2s
        // and report the count.
        void GetEventIconPositions(Vector2* lv2IconPositions, s32* lpiNumIcons);

        // @0x824F5A78 (this TU, DWARF h:90) -- the event id of one icon (bounds-asserted).
        u32 GetEventIDForIconIndex(s32 liIconIndex) const;

    private:
        // DWARF h:104-107 (X360: the table at +0, the count at +0x834 == 175*12).
        EventIcon2D ma2DEventIcons[KI_MAX_2DEVENTICONS];
        s32         miNumEventIcons;
        GuiCache*   mpGuiCache;
    };
}
