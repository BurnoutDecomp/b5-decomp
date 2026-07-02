#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                             // CgsID, Vector2
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"     // CgsGui::GuiComponent (second base)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"        // BrnGui::Selectable (first base)

// BrnGui::RivalTableCell - one cell of the car-select rival table: a selectable
// bound to a rival's car id, mirroring its rivalry/wrecked/driven state into apt
// view states. Class shape / member names / method set verbatim from the DecFIGS
// DWARF (BrnRivalTableCell.h:48/:106-:116). The X360 layout pins the usual MI
// bases (Selectable @+0x00, flags @+0x0C; GuiComponent @+0x18) with
// meRivalryStage @+0xA4, mCarID @+0xA8, mbDriven @+0xB0, mbWrecked @+0xB1,
// mbEmpty @+0xB2. This TU bodies Construct/Update/SetWrecked/SetScreenPosition;
// the rest of the surface is declared-only (own ledger functions / X360 inlines).
namespace BrnGui
{
    // BrnGui::ERivalryStage (namespace scope per the GuiCache DWARF references).
    // Enumerators from the DWARF's RivalryOverviewAction::ERivalryStage twin
    // (identical name/value set); FLAG: move to the enum's own canonical home when
    // it is recovered.
    enum ERivalryStage
    {
        E_RIVALRY_STAGE_UNKNOWN = 0,
        E_RIVALRY_STAGE_DRIVER  = 1,
        E_RIVALRY_STAGE_RIVAL   = 2,
        E_RIVALRY_STAGE_TARGET  = 3,
        E_RIVALRY_STAGE_WRECKED = 4,
        E_RIVALRY_STAGE_INVALID = 5,
        E_RIVALRY_STAGE_COUNT   = 6,
    };

    struct RivalTableCell : public Selectable, public CgsGui::GuiComponent
    {
        // @0x82418E80 (this TU, DWARF cpp:48) -- both base Constructs (invalid id),
        // clear the rival binding, reset the selectable gates, mark empty.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);

        // @0x82418F48 (this TU, DWARF cpp:78) -- consume the dirty flag and push the
        // rival-car / rivalry-state apt views.
        virtual void Update();

        // @0x82419060 (this TU, DWARF cpp:154) -- latch the wrecked flag and (when
        // the cell is live) push the transition state.
        void SetWrecked(bool lbWrecked);

        // @0x824268B8 (this TU, DWARF cpp:130) -- push the cell's screen X and dirty it.
        void SetScreenPosition(Vector2 lv2ScreenPosition);

        // DWARF h:135-:233 -- declared-only (own ledger functions / X360 inlines).
        virtual void Select();
        void SetRivalryStage(ERivalryStage leStage);
        void SetCarID(CgsID lCarID);
        ERivalryStage GetRivalryStage() const;
        CgsID GetCarID() const;
        void SetEmpty(bool lbEmpty);
        void SetDriven(bool lbDriven);

    private:
        // DWARF cpp:24/:25 -- the apt variable names (declared-only statics; their
        // consumers are the not-yet-bodied Set* methods).
        static const char KAC_RIVALRY_STAGE_VAR[17];
        static const char KAC_RIVAL_VEHICLE_VAR[18];

        ERivalryStage meRivalryStage;   // :106 (X360 +0xA4)
        CgsID         mCarID;           // :107 (X360 +0xA8)
        bool          mbDriven;         // :108 (X360 +0xB0)
        bool          mbWrecked;        // :109 (X360 +0xB1)
        bool          mbEmpty;          // :116 (X360 +0xB2)
    };
}
