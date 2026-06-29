#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                        // CgsID, Vector3
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent (real base)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"       // CgsDev::DebugUI::StringList

// BrnGameState::ResetPlayerDebugComponent - the in-game "Reset Player Car" debug menu. Derives from
// the real CgsDev::DebugComponent and registers a set of debug-menu variables + action callbacks that
// let a developer teleport the player car to any of the track's named regions, and swap its
// vehicle / livery / wheel selection live.
//
// Base/member types + declaration order are authoritative from the DecFIGS DWARF
// (GameState/BrnResetPlayerDebugComponent.h:56). Each menu list is held as two parallel arrays: a
// CgsDev::DebugUI::StringList[] mapping the option index -> its display name (what SetOptions()
// consumes), plus a fixed char[][64] backing store the names point into. The X360 object layout is
// reproduced by NAMED members in DWARF order; exact inter-member byte offsets are intentionally NOT
// modelled with padding (pointer/CgsID widths differ on the PC x64 build, so byte-for-byte parity is
// neither achievable nor required -- semantic parity by named member is the project rule). The asm
// only fixes which member each access touches, which is what the declaration order below captures.
//
// Method set is gated on the X360 ledger: the nine functions the X360 build attests for this class
// (Construct/Destruct... GetName/OnActivate, the teleport/change-car actions + their menu callbacks,
// the car-filter/selection change handlers) are declared here. The DWARF additionally lists
// OnRegister/RenderHUD/TeleportCar/OnChangeCarSelection methods; OnChangeCarSelection is reached by
// the attested OnChangeCarFilter/...SelectionCallback path and is declared, while the render/register
// surface that the X360 ledger does not attest for this class is left out of this pass.

namespace BrnGameState
{
    class GameStateModule;   // pointer member; full definition in BrnGameStateModule.h (the .cpp includes it)

    class ResetPlayerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // X360 @ 0x82357940. Bind the component to its owning game-state module and reset the menu
        // selection state. Called by BrnGameState::GameStateModule::Construct.
        void Construct(GameStateModule* lpGameStateModule);
        void Destruct();

    protected:
        // X360 @ 0x823579C8. The menu label for this component.
        const char* GetName() const override;

        // X360 @ 0x82390F50. Build every menu list (locations, car filter, cars, car versions, wheels)
        // off the loaded track + vehicle/wheel resources and register the variables + action callbacks
        // with the debug UI.
        void OnActivate() override;

    private:
        // ---- action callbacks (registered with the debug menu) -------------------------------------
        // X360 @ 0x82382B20 / 0x82382BC0. Broadcast the chosen wheel / teleport-position to the game
        // via the owning module's output GUI event queue.
        void TeleportCar();
        void ChangeCar();

        // X360 @ 0x82382BC0 / 0x82382C28. Static menu-callback trampolines (the void* the menu passes
        // back is this component).
        static void TeleportCarCallback(void* lpData);   // @ 0x82382BC0
        static void ChangeCarCallback(void* lpData);     // @ 0x82382C28

        // X360 @ 0x82382C30. Rebuild the car list filtered by the currently-selected car-filter, then
        // re-run the selection. @ 0x82382E10 is the menu-change trampoline.
        void        OnChangeCarFilter();
        static void OnChangeCarFilterCallback(void* lpData, void* lpUserData);   // @ 0x82382E10

        // The selection (car/version) change handler + its menu-change trampoline. OnChangeCarSelection
        // is reached through the attested ...SelectionCallback (@ 0x82377230); its body lands with the
        // dedicated reconstruction (only the callback forwarder is attested here).
        void        OnChangeCarSelection();
        static void OnChangeCarSelectionCallback(void* lpData, void* lpUserData);   // @ 0x82377230

        // ---- DWARF sizes (BrnResetPlayerDebugComponent.h) ------------------------------------------
        static const s32 KI_CAR_FILTER_COUNT            = 10;    // :114
        static const s32 KI_LOCATION_TEXT_LENGTH        = 64;    // :117
        static const s32 KI_CAR_TEXT_LENGTH             = 64;    // :118
        static const s32 KI_MAX_CAR_NAME_COUNT          = 96;    // :119
        static const s32 KI_MAX_CAR_VERSION_NAME_COUNT  = 16;    // :120
        static const s32 KI_MAX_WHEEL_NAME_COUNT        = 128;   // :121
        static const s32 KI_MAX_TELEPORT_LOCATION_COUNT = 128;   // :122

        // ---- members (DWARF order, BrnResetPlayerDebugComponent.h:124-146) -------------------------
        GameStateModule* mpGameStateModule;                                            // :124  (+0x0C)

        CgsDev::DebugUI::StringList maLocationNames[KI_MAX_TELEPORT_LOCATION_COUNT];    // :126
        CgsDev::DebugUI::StringList maCarFilterNames[KI_CAR_FILTER_COUNT];              // :127
        CgsDev::DebugUI::StringList maCarNames[KI_MAX_CAR_NAME_COUNT];                  // :128
        CgsDev::DebugUI::StringList maCarVersionNames[KI_MAX_CAR_VERSION_NAME_COUNT];   // :129
        CgsDev::DebugUI::StringList maWheelNames[KI_MAX_WHEEL_NAME_COUNT];              // :130

        CgsID maCarIds[KI_MAX_CAR_NAME_COUNT];                                          // :131
        CgsID maCarVersionIds[KI_MAX_CAR_VERSION_NAME_COUNT];                           // :132

        char maLocationStrings[KI_MAX_TELEPORT_LOCATION_COUNT][KI_LOCATION_TEXT_LENGTH]; // :134

        Vector3 maLocationPositions[KI_MAX_TELEPORT_LOCATION_COUNT];                    // :135
        Vector3 maLocationDirections[KI_MAX_TELEPORT_LOCATION_COUNT];                   // :136

        char maCarStrings[KI_MAX_CAR_NAME_COUNT][KI_CAR_TEXT_LENGTH];                   // :137
        char maCarVersionStrings[KI_MAX_CAR_VERSION_NAME_COUNT][KI_CAR_TEXT_LENGTH];    // :138

        s32  miCurrentLocationIndex;     // :140  (+0x5CA0)
        s32  miCurrentCarFilter;         // :141  (+0x5CA4)
        s32  miCurrentCarIndex;          // :142  (+0x5CA8)
        s32  miCurrentCarVersionIndex;   // :143  (+0x5CAC)
        s32  miCurrentWheelIndex;        // :144  (+0x5CB0)

        bool mbShowCarInfo;              // :146  (+0x5CB4)
    };
}
