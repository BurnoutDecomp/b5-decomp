// CgsGui::SaveLoadSystem - wave-B partfile 01 (locale / option plumbing).
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The class is homed in CgsSaveLoadX360.h
// (its assert sites cite gui/CgsSaveLoadX360.cpp); the twelve base functions live in the
// committed sibling CgsSaveLoadPS3.cpp. This partfile bodies three attested wave-B members:
//
//   LocaleGetStrCallback        @ 0x8284C1B0  (static locale-id lookup)
//   MessageChoiceForOptionIndex @ 0x8284BF50  (static option-index -> Realmc choice code)
//   HandleMemcardOption         @ 0x8284C6D8  (member; dispatches MemcardInterface::MessageChoice)
//
// Named-member accessors come from the frozen headers; no raw offset arithmetic.

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"   // MUST be first (fwd-decls ::LanguageManager/::SystemUserProfile at global scope)

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "SDKs/Realmc/RealmcMemcardInterface.h"           // RealmcIface::MemcardInterface::MessageChoice

namespace
{
    // ---- Locale string-id table (X360 off_82F331B0, idat-dumped) --------------------------------
    // maRealmemcardStringIDs[28] -- LocaleGetStrCallback maps a 0..27 string id onto the localisation
    // key the Realmc memory-card layer looks up. File-scope static per the keystone spec (A3); the
    // table is this partfile's private rodata.
    static const char* const maRealmemcardStringIDs[28] =
    {
        /*  0 */ "SAVELOAD_NO_STORAGE_SELECTED",
        /*  1 */ "MCM_MEMCARD_MESSAGES_VERSION",
        /*  2 */ "MCM_COMPATIBLE_REALMEMCARD_VERSION",
        /*  3 */ "SAVELOAD_CONTINUE",
        /*  4 */ "SAVELOAD_CONTINUE_WITHOUT_SAVING",
        /*  5 */ "SAVELOAD_SELECT_A_DEVICE",
        /*  6 */ "GENERAL_OPTION_YES",
        /*  7 */ "GENERAL_OPTION_NO",
        /*  8 */ "SAVELOAD_CANCEL",
        /*  9 */ "SAVELOAD_NO_DEVICE_SELECTED",
        /* 10 */ "SAVELOAD_LOAD_CORRUPT",
        /* 11 */ "SAVELOAD_LOAD_FAILED",
        /* 12 */ "SAVELOAD_LOAD_FAILED_CARD_REMOVED",
        /* 13 */ "TCR_FILE_EXISTS_OK_TO_PROCEED",
        /* 14 */ "SAVELOAD_SAVE_OVERWRITE_PLAYER_DATA",
        /* 15 */ "SAVELOAD_SAVE_WARNING",
        /* 16 */ "SAVELOAD_SAVE_FAILED",
        /* 17 */ "SAVELOAD_SAVE_FAILED_CARD_REMOVED",
        /* 18 */ "SAVELOAD_SAVE_DELETE_CONFIRM",
        /* 19 */ "TCR_DELETE_WARNING",
        /* 20 */ "TCR_DELETE_FAILED",
        /* 21 */ "TCR_DELETE_FAILED_CARD_REMOVED",
        /* 22 */ "SAVELOAD_SAVE_INSUFFICIENT_SPACE",
        /* 23 */ "SAVELOAD_NOT_SIGNED_IN",
        /* 24 */ "SAVELOAD_DEVICE_REMOVED_OR_CHANGED",
        /* 25 */ "SAVELOAD_SAVE_OVERWRITE_CORRUPT_PLAYER_DATA",
        /* 26 */ "TCR_NOT_SIGNED_IN_A",
        /* 27 */ "SAVELOAD_NO_DATA",
    };
}

namespace CgsGui
{
    // X360 0x8284C1B0. The locale string-id lookup callback handed to the Realmc memory card.
    // Range-asserts the id, then returns the table entry. The X360 indexes the table even on a
    // failed assert (no clamp) -- reproduced faithfully.
    const char* SaveLoadSystem::LocaleGetStrCallback(u32 luStringID)
    {
        CGS_ASSERT(luStringID < (sizeof(maRealmemcardStringIDs) / sizeof(maRealmemcardStringIDs[0])),
                   "liStringID < M_ARRAY_SIZE(maRealmemcardStringIDs)");
        return maRealmemcardStringIDs[luStringID];
    }

    // X360 0x8284BF50 (body originally header-defined at CgsSaveLoadX360.h:745). Map a MessageDisplay
    // option index 0..3 onto the Realmc message-choice code 1..4; any other index fires the streamed
    // assert (folded to CGS_ASSERT) and returns 0.
    s32 SaveLoadSystem::MessageChoiceForOptionIndex(u32 luOptionIndex)
    {
        s32 liResult;

        switch (luOptionIndex)
        {
        case 0:
            liResult = 1;
            break;
        case 1:
            liResult = 2;
            break;
        case 2:
            liResult = 3;
            break;
        case 3:
            liResult = 4;
            break;
        default:
            CGS_ASSERT(false, "SaveLoad: unexpected option index: ");
            liResult = 0;
            break;
        }

        return liResult;
    }

    // X360 0x8284C6D8. Translate the chosen option index and forward it to the memory-card interface
    // (MessageChoice, vtable slot +0x14).
    void SaveLoadSystem::HandleMemcardOption(u32 luOption)
    {
        mpMemcardInterface->MessageChoice(MessageChoiceForOptionIndex(luOption));
    }
}
