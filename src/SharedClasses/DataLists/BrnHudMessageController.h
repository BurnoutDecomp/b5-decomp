#pragma once

// BrnHudMessageController.h
// BrnResource::HudMessageEvent + BrnResource::HudMessageController -- the loaded
// HUD-message table (a GuiHudMessageResource bundle) and the wire event the
// controller formats out of it for the HUD-message director.
//
// Shapes / member names / method sets verbatim from the DecFIGS DWARF
// (SharedClasses/DataLists/BrnHudMessageController.h:56/:88); gated on the X360
// ledger. This TU bodies GetIndexFromMessageHash; every other method is
// declared-only (its own ledger function).

#include "types.hpp"                                                 // s32/u32/f32
#include "BrnCommonTypes.h"                                          // CgsID (u64)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::ResourcePtr<T>
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessage.h" // CgsGui::HudMessageParameter / HudMessageParamTypes / HudMessageGroups / GuiHudMessageResource

namespace BrnGui { struct GuiHudMessage; }                           // GameSource/Gui/BrnGuiEventTypeDefs.h
namespace CgsResource { namespace Events { struct AcquireResourceResponse; } } // CgsResourceIOEvents.h

namespace BrnResource
{
    // BrnHudMessageController.h:56 -- the formatted HUD-message wire event the
    // controller builds from a GuiHudMessage + the loaded message table.
    struct HudMessageEvent
    {
        static const s32 KI_FLASH_FRAME_LABEL_LENGTH = 32;   // :59
        static const s32 KI_STRING_ID_LENGTH         = 64;   // :60
        static const s32 KI_MAX_PARAMS_PER_STRING    = 4;    // :61
        static const s32 KI_MAX_NUM_STRINGS          = 3;    // :62

        CgsID mHudMessageId;                                                // :64
        s32   miPriority;                                                   // :65
        s32   miForceRemoveThreshold;                                       // :66
        s32   miGroup;                                                      // :67
        f32   mfDuration;                                                   // :68
        char  maacString[KI_MAX_NUM_STRINGS][KI_STRING_ID_LENGTH];          // :70
        CgsGui::HudMessageParameter
              maacStringParam[KI_MAX_NUM_STRINGS][KI_MAX_PARAMS_PER_STRING];// :71
        s32   maiStringParamCount[KI_MAX_NUM_STRINGS];                      // :73
        char  macMessageStyle[KI_FLASH_FRAME_LABEL_LENGTH];                 // :75
        char  macIcon[KI_FLASH_FRAME_LABEL_LENGTH];                         // :76
    };

    // BrnHudMessageController.h:88 -- wraps the loaded HUD-message resource bundle.
    struct HudMessageController
    {
        static const u32 KU_MAX_NUMBER_OF_MESSAGES = 300;   // :91

        // DWARF :95-:156 -- declared-only here except GetIndexFromMessageHash
        // (each is its own ledger function, bodied in this TU as reconstructed).
        void   Construct();                                                            // :95
        bool   GetMessage(const BrnGui::GuiHudMessage* lpInMessage,
                          HudMessageEvent* lpOutMessage) const;                        // :101
        void   AddMessages(const CgsResource::Events::AcquireResourceResponse* lpResource); // :105
        s32    GetIndexFromMessageHash(CgsID lId) const;                               // :110 (@0x8267D4C8, this TU)
        f32    GetMessageTimeToWaitFromIndex(s32 liIndex) const;                       // :114
        u32    GetMessageAvailabilityBitset(s32 liIndex) const;                        // :118
        bool   IsMessageAHigherPriority(CgsID lAToTest, CgsID lBCurrent) const;        // :122
        CgsID  GetMessageHashFromIndex(s32 liIndex) const;                             // :131
        s32    GetMessageParamCount(s32 liMessageIndex, s32 liString) const;           // :137
        CgsGui::HudMessageParamTypes
               GetMessageParamType(s32 liMessageIndex, s32 liString, s32 liParam) const; // :144
        s32    GetMessageLoadedCount() const;                                          // :148
        // (the DWARF spells the group param type `CgsGui::HudMessageGroup`; the
        //  committed enum home CgsGuiHudMessage.h names it HudMessageGroups)
        s32    GetNextMessageIdInGroup(s32 liCurrentId, CgsGui::HudMessageGroups leGroup) const;     // :152
        s32    GetPreviousMessageIdInGroup(s32 liCurrentId, CgsGui::HudMessageGroups leGroup) const; // :156

    private:
        bool mbMessagesUsed;                                                // :161 @+0
        CgsResource::ResourcePtr<CgsGui::GuiHudMessageResource> mMessagesPtr; // :163 @+4 (X360)
    };
}
