// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_EventFlowGuiEvents_wZ_00.cpp
//
// The free-roam arms of BrnGame::BrnGameModule::TranslateGameActionsToGuiEvents: junkyard entry,
// drive-thru discovery, refusals and closing, landmark areas, the super-jump name and failure, and
// the sat-nav switch. Reached from the `default:` of TranslateEventFlowGameActionToGuiEvent
// (GameBridgeGameStateToX_EventFlowGuiEvents.cpp); returns true when an arm consumed the action.
//
// Every GUI record posted here is the canonical type from BrnGuiDemangledEventTypes.h except the
// bare CgsGui::GuiEvent<315> tag, whose one-byte wire record is local (the shared GuiEvent<N>
// header is 12 bytes and would post the wrong size).
//
// FLAG: the two action payloads read here (actions 104 and 109) have no shared home yet; their
// producers build them as file-local records (BrnDriveThruManager.cpp, BrnTriggerQueryManager.cpp),
// so each is mirrored below by the producer's own field names and read by name.
// ============================================================================

#include "GameSource/Game/GameBridgeGameStateToX.h"                // PushGuiEvent<T>
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"               // the posted GUI records
#include "GameSource/GameState/BrnGameActions.h"                    // action ids + DriveThruJunkYardAction
#include "SharedClasses/Trigger/BrnTriggerBase.h"                   // TriggerRegion::E_TYPE_LANDMARK
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                      // CgsID
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint
#include <stdlib.h>                                                 // getenv (the diag guard)
#include <cstring>                                                  // memcpy

namespace BrnGame
{
namespace
{
    // The drive-thru "discovered" action (104, 12 bytes). Its producer,
    // DriveThruManager::Update, writes the three words as the drive-thru type, the number of
    // drive-thrus of that type, and how many of them the profile has discovered.
    struct DriveThruDiscoveredActionMirror
    {
        s32 meDriveThruType;                   // +0x00  BrnTrigger::GenericRegion::Type
        s32 miTotalDriveThrusOfThisType;       // +0x04
        s32 miNumDriveThrusOfThisTypeFound;    // +0x08
    };
    static_assert(sizeof(DriveThruDiscoveredActionMirror) == 12, "action 104 is posted with size 12");

    // The player-trigger action (109, 24 bytes), one per trigger region the player car is in,
    // every frame. Posted by TriggerQueryManager::PreWorldUpdate's fan-out.
    struct PlayerTriggerActionMirror
    {
        CgsID mId;                             // +0x00  TriggerRegion::GetId()
        s32   miRegionType;                    // +0x08  TriggerRegion::Type
        s32   miGenericType;                   // +0x0C  GenericRegion::Type (generic regions only)
        s32   miRegionIndex;                   // +0x10  the region's index in the trigger data
        u8    mbFirstFrame;                    // +0x14
        u8    mauPad[3];                       // +0x15
    };
    static_assert(sizeof(PlayerTriggerActionMirror) == 24, "action 109 is posted with size 24");

    // id 315 size 1 -- AddGuiEvent<CgsGui::GuiEvent<315>>. The arm posts a stack byte it never
    // writes; the consumer (HudMessageAnalyzer case 315 -> "EvryDrveThru") reads only the id.
    struct AllDriveThrusFoundWire315
    {
        u8 mu8Unused;                          // +0x00  never written by the arm
        s32 GetEventType() const { return 315; }
    };
    static_assert(sizeof(AllDriveThrusFoundWire315) == 1, "id 315 size 1");

    static_assert(sizeof(BrnGui::GuiEnteredJunkyard) == 1,              "id 79 size 1");
    static_assert(sizeof(BrnGui::GuiEventEnterLandmarkArea) == 2,       "id 165 size 2");
    static_assert(sizeof(BrnGui::GuiEventHideDriveThru) == sizeof(CgsID), "id 201 size 8");
    static_assert(sizeof(BrnGui::GuiEventMiniMapSwitch) == 1,           "id 205 size 1");
    static_assert(sizeof(BrnGui::GuiEventJumpStarted) == sizeof(CgsID), "id 216 size 8");
    static_assert(sizeof(BrnGui::GuiEventDriveThruDiscovered) == 12,    "id 314 size 12");
    static_assert(sizeof(BrnGui::GuiEventSuperJumpFailed) == 1,         "id 549 size 1");
    static_assert(sizeof(BrnGui::GuiEventCantPaintCar) == 1,            "id 551 size 1");
    static_assert(sizeof(BrnGui::GuiEventMustFixCarFirst) == 1,         "id 552 size 1");

    // [FLAG PC witness] NOT IN THE CONSOLE. Opt-in (BRN_FREEROAM_DIAG), one line per translated
    // action, capped per arm by the caller's counter.
    void FreeRoamGuiWitness(s32 liActionType, s32 liGuiEventId, s32 liDetail, s32& riLinesLeft)
    {
        static const bool sbDiag = (getenv("BRN_FREEROAM_DIAG") != 0);
        if (sbDiag && riLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            --riLinesLeft;
            *CgsDev::Log::gpDebugPrint
                << "[freeroam-gui] action " << liActionType << " -> gui " << liGuiEventId
                << " (" << liDetail << ")\n";
        }
    }
}   // anonymous namespace

bool TranslateFreeRoamGameActionToGuiEvent(
    s32 liActionType,
    const CgsModule::Event* lpAction,
    CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
{
    switch (liActionType)
    {
    // ---- 46  the drive-thru CLOSE (8 bytes) ---------------------------------------------------
    // The closed drive-thru's CgsID (the record's doubleword) straight into GuiEventHideDriveThru
    // (id 201, size 8). GuiCache's case-201 arm hides that drive-thru's map row.
    case 46:
    {
        const CgsID lDriveThruId = *reinterpret_cast<const CgsID*>(lpAction);

        BrnGui::GuiEventHideDriveThru lEvent;
        std::memcpy(lEvent.maData, &lDriveThruId, sizeof(lDriveThruId));
        PushGuiEvent(lEvent, lpGuiInput);

        static s32 siLinesLeft = 8;
        FreeRoamGuiWitness(liActionType, lEvent.GetEventType(),
                           static_cast<s32>(static_cast<u32>(lDriveThruId)), siLinesLeft);
        return true;
    }

    // ---- 57  E_ACTION_SHOW_JUMP_NAME (8 bytes) ------------------------------------------------
    // Null assert on the record, then its doubleword (the jump's stunt-element key) into
    // GuiEventJumpStarted (id 216, size 8).
    case BrnGameState::GameStateModuleIO::E_ACTION_SHOW_JUMP_NAME:
    {
        CGS_ASSERT(lpAction != 0, "lpJumpNameAction != NULL");
        const CgsID lJumpKey = *reinterpret_cast<const CgsID*>(lpAction);

        BrnGui::GuiEventJumpStarted lEvent;
        std::memcpy(lEvent.maData, &lJumpKey, sizeof(lJumpKey));
        PushGuiEvent(lEvent, lpGuiInput);

        static s32 siLinesLeft = 8;
        FreeRoamGuiWitness(liActionType, lEvent.GetEventType(), 0, siLinesLeft);
        return true;
    }

    // ---- 99  E_ACTION_DRIVE_THRU_JUNK_YARD (1 byte) -------------------------------------------
    // The in-junkyard byte straight into GuiEnteredJunkyard (id 79, size 1).
    case BrnGameState::GameStateModuleIO::E_ACTION_DRIVE_THRU_JUNK_YARD:
    {
        const BrnGameState::GameStateModuleIO::DriveThruJunkYardAction* lpJunkYard =
            reinterpret_cast<const BrnGameState::GameStateModuleIO::DriveThruJunkYardAction*>(lpAction);

        BrnGui::GuiEnteredJunkyard lEvent;
        lEvent.maData[0] = lpJunkYard->mbIsInJunkYard ? 1u : 0u;
        PushGuiEvent(lEvent, lpGuiInput);

        static s32 siLinesLeft = 8;
        FreeRoamGuiWitness(liActionType, lEvent.GetEventType(), lEvent.maData[0], siLinesLeft);
        return true;
    }

    // ---- 104  the drive-thru DISCOVERED (12 bytes) --------------------------------------------
    // The three words straight across into GuiEventDriveThruDiscovered (id 314, size 12).
    case 104:
    {
        const DriveThruDiscoveredActionMirror* lpDiscovered =
            reinterpret_cast<const DriveThruDiscoveredActionMirror*>(lpAction);

        BrnGui::GuiEventDriveThruDiscovered lEvent;
        lEvent.meDriveThruType = lpDiscovered->meDriveThruType;
        lEvent.miNumTotal      = lpDiscovered->miTotalDriveThrusOfThisType;
        lEvent.miNumDiscovered = lpDiscovered->miNumDriveThrusOfThisTypeFound;
        PushGuiEvent(lEvent, lpGuiInput);

        static s32 siLinesLeft = 8;
        FreeRoamGuiWitness(liActionType, lEvent.GetEventType(), lEvent.meDriveThruType, siLinesLeft);
        return true;
    }

    // ---- 105  ALL drive-thrus found (1 byte, unread) ------------------------------------------
    // No load off the action: the bare CgsGui::GuiEvent<315> tag.
    case 105:
    {
        AllDriveThrusFoundWire315 lEvent;
        lEvent.mu8Unused = 0;
        PushGuiEvent(lEvent, lpGuiInput);

        static s32 siLinesLeft = 8;
        FreeRoamGuiWitness(liActionType, lEvent.GetEventType(), 0, siLinesLeft);
        return true;
    }

    // ---- 109  the PLAYER TRIGGER (24 bytes) ---------------------------------------------------
    // Null assert on the record; only a LANDMARK region (type word at +0x08 == 0) posts, and it
    // posts the region index (+0x10) as the halfword of GuiEventEnterLandmarkArea (id 165,
    // size 2). Every other region type is consumed with no post.
    case 109:
    {
        CGS_ASSERT(lpAction != 0, "lpTriggerAction != NULL");
        const PlayerTriggerActionMirror* lpTrigger =
            reinterpret_cast<const PlayerTriggerActionMirror*>(lpAction);

        if (lpTrigger->miRegionType == BrnTrigger::TriggerRegion::E_TYPE_LANDMARK)
        {
            const u16 lu16LandmarkIndex = static_cast<u16>(lpTrigger->miRegionIndex);
            BrnGui::GuiEventEnterLandmarkArea lEvent;
            std::memcpy(lEvent.maData, &lu16LandmarkIndex, sizeof(lu16LandmarkIndex));
            PushGuiEvent(lEvent, lpGuiInput);

            // The action repeats every frame the car is in the region: one line per new index.
            static s32 siLinesLeft     = 8;
            static s32 siLastLandmark  = -1;
            if (static_cast<s32>(lu16LandmarkIndex) != siLastLandmark)
            {
                siLastLandmark = static_cast<s32>(lu16LandmarkIndex);
                FreeRoamGuiWitness(liActionType, lEvent.GetEventType(), siLastLandmark, siLinesLeft);
            }
        }
        return true;
    }

    // ---- 190  the SAT-NAV switch (1 byte) -----------------------------------------------------
    // The action's byte straight into GuiEventMiniMapSwitch (id 205, size 1).
    case 190:
    {
        BrnGui::GuiEventMiniMapSwitch lEvent;
        lEvent.maData[0] = *reinterpret_cast<const u8*>(lpAction);
        PushGuiEvent(lEvent, lpGuiInput);

        static s32 siLinesLeft = 8;
        FreeRoamGuiWitness(liActionType, lEvent.GetEventType(), lEvent.maData[0], siLinesLeft);
        return true;
    }

    // ---- 263 / 264 / 265  the drive-thru refusals and the failed super jump (1 byte) ----------
    // No load off the action and no store into the frame: each arm posts a stack byte it never
    // writes, and each consumer (HudMessageAnalyzer 551 "CantPaintCar", 552 "NeedToFixCar",
    // 549 "JumpFailed") reads only the id.
    case 263:
    {
        BrnGui::GuiEventCantPaintCar lEvent;
        lEvent.maData[0] = 0;
        PushGuiEvent(lEvent, lpGuiInput);

        static s32 siLinesLeft = 8;
        FreeRoamGuiWitness(liActionType, lEvent.GetEventType(), 0, siLinesLeft);
        return true;
    }
    case 264:
    {
        BrnGui::GuiEventMustFixCarFirst lEvent;
        lEvent.maData[0] = 0;
        PushGuiEvent(lEvent, lpGuiInput);

        static s32 siLinesLeft = 8;
        FreeRoamGuiWitness(liActionType, lEvent.GetEventType(), 0, siLinesLeft);
        return true;
    }
    case 265:
    {
        BrnGui::GuiEventSuperJumpFailed lEvent;
        lEvent.maData[0] = 0;
        PushGuiEvent(lEvent, lpGuiInput);

        static s32 siLinesLeft = 8;
        FreeRoamGuiWitness(liActionType, lEvent.GetEventType(), 0, siLinesLeft);
        return true;
    }

    default:
        return false;
    }
}
} // namespace BrnGame
