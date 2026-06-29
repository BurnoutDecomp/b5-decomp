// Bodies for the Live Revenge debug-menu component, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct              @ 0x82584D48
//   RegisterRelationship   @ 0x82584D80
//   UnregisterRelationship @ 0x82584F18
//   AddPlayer              @ 0x82584FD8
//   RemovePlayer           @ 0x825850C8
//   GetName                @ 0x82585148 -> "Live Revenge"
//   UploadToServer         @ 0x82585158
//   UnregisterAll          @ 0x82591318
//   RegisterAll            @ 0x82593880
//   OnActivate             @ 0x82594708
//
// The component publishes a relationship's stats into the debug menu, one menu group per rival
// (grouped under the rival's name). AddPlayer registers the two summary variables (current revenge
// status / total events), three per-relationship action functions, then RegisterRelationship's 16
// overall-stat variables; RemovePlayer/UnregisterRelationship tear (most of) that back down. The
// three OnActivate actions (register all / unregister all / force upload) drive the whole table:
// RegisterAll first UnregisterAll's then AddPlayer's every entry of the manager's profile table.
//
// Decompiler symbol map (verified against the assembly, not just the pseudocode):
//   sub_8282D560 == DebugComponent::RegisterVariable(s32*, group, name)   (grouped overload)
//   sub_8282D5D0 == DebugComponent::RegisterVariable(u32*, group, name)   (grouped overload)
//   sub_8282F720 == DebugComponent::RegisterFunction(callback, userData, name)  (leaf overload)
//   BaseCollisionGenerator::Destruct(this) in Construct == CgsDev::DebugComponent::Construct()
//       (the X360 image folds the trivial base two-phase-init body onto one shared address; same
//        documented fold as CgsLanguageManagerDebugComponent::Construct).
//   BrnNetwork::LiveRevengeRelations(&table, index) == Array<...>::operator[](index) (inlined).
//
// AddPlayer's `lpRelationship->GetRivalName()` returns a CgsNetwork::PlayerName at relationship+88;
// the asm asserts that pointer is non-null, then registers ONLY when the name is non-empty
// (first char != 0). The rival-name C-string is reused as the menu group for every variable/function
// the call registers. (The relationship's own header owns the +88 name region; this TU calls
// GetRivalName() rather than poking the offset.)

#include "GameSource/Network/Debug Components/BrnNetworkLiveRevengeDebugComponent.h"

#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"  // LiveRevengeManager / LiveRevengeProfile (profile + table)
#include "GameSource/GameState/BrnCgsPlayerName.h"                     // CgsNetwork::PlayerName::GetPlayerName
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

namespace BrnNetwork
{
    // ----------------------------------------------------------------------------------------------
    // Lifecycle
    // ----------------------------------------------------------------------------------------------

    // @ 0x82584D48. Cache the owning manager, run the base two-phase init, register with the menu.
    void LiveRevengeDebugComponent::Construct(LiveRevengeManager* lpLiveRevengeManager)
    {
        mpLiveRevengeManager = lpLiveRevengeManager;
        CgsDev::DebugComponent::Construct();
        Register();
    }

    // @ 0x82585148.
    const char* LiveRevengeDebugComponent::GetName() const
    {
        return "Live Revenge";
    }

    // ----------------------------------------------------------------------------------------------
    // Per-relationship registration
    // ----------------------------------------------------------------------------------------------

    // @ 0x82584D80. Register the 16 "overall stats" menu variables (player + rival side of each
    // counter), all grouped under the rival's name. lpStats is the relationship's mOverallStats block.
    void LiveRevengeDebugComponent::RegisterRelationship(CommonRelationship* lpStats, const char* lpcRivalName)
    {
        RegisterVariable(&lpStats->mPlayerStats.miTakedowns,       lpcRivalName, "Number Of Takedowns By Player");
        RegisterVariable(&lpStats->mRivalStats.miTakedowns,        lpcRivalName, "Number Of Takedowns By Rival");
        RegisterVariable(&lpStats->mPlayerStats.miLongestStreak,   lpcRivalName, "Longest Streak By player");
        RegisterVariable(&lpStats->mRivalStats.miLongestStreak,    lpcRivalName, "Longest Streak By Rival");
        RegisterVariable(&lpStats->mPlayerStats.miWins,            lpcRivalName, "Wins by player");
        RegisterVariable(&lpStats->mRivalStats.miWins,             lpcRivalName, "Wins by rival");
        RegisterVariable(&lpStats->mPlayerStats.miScoresSettled,   lpcRivalName, "Scores Settled by player");
        RegisterVariable(&lpStats->mRivalStats.miScoresSettled,    lpcRivalName, "Scores Settled by rival");
        RegisterVariable(&lpStats->mPlayerStats.miMarks,           lpcRivalName, "Marks by player");
        RegisterVariable(&lpStats->mRivalStats.miMarks,            lpcRivalName, "Marks by rival");
        RegisterVariable(&lpStats->mPlayerStats.miScalps,          lpcRivalName, "Scalps by player");
        RegisterVariable(&lpStats->mRivalStats.miScalps,           lpcRivalName, "Scalps by rival");
        RegisterVariable(&lpStats->mPlayerStats.miPaybacksDealt,   lpcRivalName, "Paybacks dealt by player");
        RegisterVariable(&lpStats->mRivalStats.miPaybacksDealt,    lpcRivalName, "Paybacks dealt by rival");
        RegisterVariable(&lpStats->mPlayerStats.miPaybacksScored,  lpcRivalName, "Paybacks scored by player");
        RegisterVariable(&lpStats->mRivalStats.miPaybacksScored,   lpcRivalName, "Paybacks scored by rival");
    }

    // @ 0x82584F18. Unregister the first 12 of the overall-stat variables. NOTE (faithful to the
    // X360): this stops after "Scalps by rival" and does NOT unregister the four paybacks variables
    // that RegisterRelationship added -- the asymmetry is in the binary, not a reconstruction gap.
    void LiveRevengeDebugComponent::UnregisterRelationship(CommonRelationship* lpStats)
    {
        UnregisterVariable(&lpStats->mPlayerStats.miTakedowns);
        UnregisterVariable(&lpStats->mRivalStats.miTakedowns);
        UnregisterVariable(&lpStats->mPlayerStats.miLongestStreak);
        UnregisterVariable(&lpStats->mRivalStats.miLongestStreak);
        UnregisterVariable(&lpStats->mPlayerStats.miWins);
        UnregisterVariable(&lpStats->mRivalStats.miWins);
        UnregisterVariable(&lpStats->mPlayerStats.miScoresSettled);
        UnregisterVariable(&lpStats->mRivalStats.miScoresSettled);
        UnregisterVariable(&lpStats->mPlayerStats.miMarks);
        UnregisterVariable(&lpStats->mRivalStats.miMarks);
        UnregisterVariable(&lpStats->mPlayerStats.miScalps);
        UnregisterVariable(&lpStats->mRivalStats.miScalps);
    }

    // @ 0x82584FD8. Add one rival's full debug-menu group. The whole registration is skipped unless
    // the rival has a (non-empty) name.
    void LiveRevengeDebugComponent::AddPlayer(LiveRevengeRelationship* lpRelationship)
    {
        const CgsNetwork::PlayerName* lpRivalName = lpRelationship->GetRivalName();
        CGS_ASSERT(lpRivalName != nullptr, "lpRelationship->GetRivalName()");

        const char* lpcRivalName = lpRivalName->GetPlayerName();
        if (lpcRivalName[0] != '\0')
        {
            // Two summary variables (status is signed; total-events is unsigned), grouped by rival.
            RegisterVariable(&lpRelationship->miCurrentScoreForPlayersPointOfView, lpcRivalName,
                             "Revenge Status from my point of view");
            RegisterVariable(&lpRelationship->miTotalEvents, lpcRivalName, "Total events");

            // Three per-relationship action callbacks (the relationship is the callback user-data).
            RegisterFunction(&LiveRevengeRelationship::DEBUGResetTimeStamp,   lpRelationship, lpcRivalName,
                             "FUNCTION: Reset relationship timestamp");
            RegisterFunction(&LiveRevengeRelationship::DEBUGSetTimeStampOld,  lpRelationship, lpcRivalName,
                             "FUNCTION: Set relationship timestamp to old");
            RegisterFunction(&LiveRevengeRelationship::DEBUGClearRelationship, lpRelationship, lpcRivalName,
                             "FUNCTION: Reset live revenge relationship");

            // The 16 overall-stat variables (mOverallStats is at relationship +0).
            RegisterRelationship(&lpRelationship->mOverallStats, lpcRivalName);
        }
    }

    // @ 0x825850C8. Tear down a rival's debug-menu group: the two summary variables, the relationship
    // stat variables, and two of the three action callbacks. NOTE (faithful to the X360): only the
    // ResetTimeStamp / SetTimeStampOld functions are unregistered -- ClearRelationship is left
    // registered, mirroring the binary.
    void LiveRevengeDebugComponent::RemovePlayer(LiveRevengeRelationship* lpRelationship)
    {
        UnregisterVariable(&lpRelationship->miCurrentScoreForPlayersPointOfView);
        UnregisterVariable(&lpRelationship->miTotalEvents);
        UnregisterRelationship(&lpRelationship->mOverallStats);
        UnregisterFunction(&LiveRevengeRelationship::DEBUGResetTimeStamp,  lpRelationship);
        UnregisterFunction(&LiveRevengeRelationship::DEBUGSetTimeStampOld, lpRelationship);
    }

    // ----------------------------------------------------------------------------------------------
    // Top-level menu actions (registered in OnActivate; the void* user-data is this component)
    // ----------------------------------------------------------------------------------------------

    // @ 0x82594708.
    void LiveRevengeDebugComponent::OnActivate()
    {
        RegisterFunction(&LiveRevengeDebugComponent::RegisterAll,   this, "Register All Relationships");
        RegisterFunction(&LiveRevengeDebugComponent::UnregisterAll, this, "Unregister All Relationships");
        RegisterFunction(&LiveRevengeDebugComponent::UploadToServer, this, "Force upload to Server");
    }

    // @ 0x82593880. Re-register every relationship in the profile table: clear the existing menu
    // groups (UnregisterAll) then AddPlayer each table entry.
    void LiveRevengeDebugComponent::RegisterAll(void* lpUserData)
    {
        LiveRevengeDebugComponent* lpDebugComponent = static_cast<LiveRevengeDebugComponent*>(lpUserData);

        UnregisterAll(lpDebugComponent);

        CGS_ASSERT(lpDebugComponent != nullptr, "lpDebugComponent");
        CGS_ASSERT(lpDebugComponent->mpLiveRevengeManager != nullptr, "lpDebugComponent->mpLiveRevengeManager");
        CGS_ASSERT(lpDebugComponent->mpLiveRevengeManager->GetProfile() != nullptr,
                   "lpDebugComponent->mpLiveRevengeManager->mpLiveRevengeProfile");

        LiveRevengeProfile* lpProfile = lpDebugComponent->mpLiveRevengeManager->GetProfile();
        const s32 liCount = static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            lpDebugComponent->AddPlayer(&lpProfile->maRelationshipTable[liIndex]);
        }
    }

    // @ 0x82591318. Unregister every relationship's menu group across the whole profile table.
    void LiveRevengeDebugComponent::UnregisterAll(void* lpUserData)
    {
        LiveRevengeDebugComponent* lpDebugComponent = static_cast<LiveRevengeDebugComponent*>(lpUserData);

        CGS_ASSERT(lpDebugComponent != nullptr, "lpDebugComponent");
        CGS_ASSERT(lpDebugComponent->mpLiveRevengeManager != nullptr, "lpDebugComponent->mpLiveRevengeManager");
        CGS_ASSERT(lpDebugComponent->mpLiveRevengeManager->GetProfile() != nullptr,
                   "lpDebugComponent->mpLiveRevengeManager->mpLiveRevengeProfile");

        LiveRevengeProfile* lpProfile = lpDebugComponent->mpLiveRevengeManager->GetProfile();
        const s32 liCount = static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            LiveRevengeRelationship* lpRelationship = &lpProfile->maRelationshipTable[liIndex];
            lpDebugComponent->UnregisterVariable(&lpRelationship->miCurrentScoreForPlayersPointOfView);
            lpDebugComponent->UnregisterVariable(&lpRelationship->miTotalEvents);
            lpDebugComponent->UnregisterRelationship(&lpRelationship->mOverallStats);
            lpDebugComponent->UnregisterFunction(&LiveRevengeRelationship::DEBUGResetTimeStamp,  lpRelationship);
            lpDebugComponent->UnregisterFunction(&LiveRevengeRelationship::DEBUGSetTimeStampOld, lpRelationship);
        }
    }

    // @ 0x82585158. Force a server upload of the rival list.
    void LiveRevengeDebugComponent::UploadToServer(void* lpUserData)
    {
        LiveRevengeDebugComponent* lpDebugComponent = static_cast<LiveRevengeDebugComponent*>(lpUserData);

        CGS_ASSERT(lpDebugComponent != nullptr, "lpDebugComponent");
        CGS_ASSERT(lpDebugComponent->mpLiveRevengeManager != nullptr, "lpDebugComponent->mpLiveRevengeManager");

        lpDebugComponent->mpLiveRevengeManager->SendLiveRevengeRivalsToServer();
    }
}
