// BrnChallengeListComponent.cpp
// BrnGui::ChallengeListComponent -- the GUI "view challenges" list component over a runtime
// BrnResource::ChallengeList. See BrnChallengeListComponent.h for the recovered layout and
// the PATH NOTE (single owning home).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetChallengeStyle                  @ 0x8248FB58  (pre-existing)
//   Construct                          @ 0x8242D9A0
//   GetFilteredChallenge               @ 0x8242DA38
//   GetHighlightedChallengeID          @ 0x82435040
//   HandleEveryPlayerCompletionStatus  @ 0x8241B618
//   Setup                              @ 0x82441170
//
#include "GameSource/Gui/BrnChallengeListComponent.h"
#include "GameSource/Gui/BrnGuiCache.h"                     // BrnGui::GuiCache::GetFreeburnChallengeList
#include "SharedClasses/DataLists/ChallengeList.h"          // BrnResource::ChallengeList (complete)
#include "SharedClasses/DataLists/ChallengeListEntry.h"     // BrnResource::ChallengeListEntry (complete)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::SnPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT

#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"
#include <cstring>                                          // memcpy (X360 whole-block copy)

namespace BrnGui
{

namespace
{
    // X360 rodata format strings baked into Construct's SnPrintf loop
    // (aAptStateD / aAptTextD). DWARF: KAC_APT_STATE_FORMAT[13] / KAC_APT_STATE_TEXT_FORMAT[12].
    const char KAC_APT_STATE_FORMAT[]      = "apt_state_%d";
    const char KAC_APT_STATE_TEXT_FORMAT[] = "apt_text_%d";
}

// GetChallengeStyle @ 0x8248FB58
// Resolves lID to its entry through the embedded list and forwards to the entry's style
// accessor. Call signatures settled from the call-site register setup (Hex-Rays dropped the
// index arg of GetChallengeData; the asm leaves the GetChallengeIndex result live in r4):
//   0x8248FB68  lwz r31, 0x130(r3)              ; v2 = this->mpChallengeList
//   0x8248FB70  bl  ChallengeList::GetChallengeIndex   ; (v2, r4 == lID)
//   0x8248FB74  mr  r4, r3                       ; r4 = index
//   0x8248FB7C  blt cr6, loc_8248FB8C            ; index < 0 -> entry = 0
//   0x8248FB84  bl  ChallengeList::GetChallengeData    ; (v2, r4 == index)
//   0x8248FB90  bl  ChallengeListEntry::GetChallengeStyle   ; (entry)  -- @0x823542A0
s32 ChallengeListComponent::GetChallengeStyle( CgsID lID ) const
{
    const BrnResource::ChallengeList* lpList = mpChallengeList;

    const s32 liIndex = lpList->GetChallengeIndex( lID );

    const BrnResource::ChallengeListEntry* lpEntry;
    if ( liIndex < 0 )
    {
        lpEntry = 0;
    }
    else
    {
        lpEntry = lpList->GetChallengeData( liIndex );
    }

    return static_cast<s32>( lpEntry->GetChallengeStyle() );
}

// Construct @ 0x8242D9A0
// Chains the base GuiComponent::Construct (this + the same three args), fills the per-slot
// apt-state name buffers ("apt_state_%d" -> maacAptState, "apt_text_%d" -> maacAptStateText),
// clears the cache/list pointers and the window/dirty/show-button state, and Constructs the
// embedded every-player completion-status block.
void ChallengeListComponent::Construct( const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                        const char* lpacParentName )
{
    GuiComponent::Construct( lpacName, lpStateInterface, lpacParentName );

    for ( s32 liSlot = 0; liSlot < KI_MAX_DISPLAYABLE_CHALLENGES; ++liSlot )
    {
        CgsCore::SnPrintf( maacAptState[ liSlot ],     KI_MAX_STATE_STRING_LENGTH, KAC_APT_STATE_FORMAT,      liSlot );
        CgsCore::SnPrintf( maacAptStateText[ liSlot ], KI_MAX_STATE_STRING_LENGTH, KAC_APT_STATE_TEXT_FORMAT, liSlot );
    }

    mpGuiCache            = 0;
    mpChallengeList       = 0;
    miNumPlayers          = 0;
    miNumChallenges       = 0;
    miStartChallengeIndex = 0;
    miHighlightedIndex    = 0;
    mbDirty               = false;
    mbShowButton          = false;

    mEveryPlayerCompletionStatus.Construct();
}

// GetFilteredChallenge @ 0x8242DA38
// Walks the challenge list, keeping only challenges whose original player count matches
// miNumPlayers, and returns the liWantedIndex-th such challenge. When found it optionally
// reports the raw list index and returns the entry only if its content has been bought
// (otherwise 0). Returns 0 if fewer than liWantedIndex+1 matching challenges exist.
//
// (X360 reads the match key as `entry->muNumPlayers >> 4`, i.e. the ORIGINAL player-count
// nibble -- accessed here through GetOriginalNumPlayers(). FLAG: the >>4 accessor is
// GetOriginalNumPlayers rather than GetNumPlayers per the Set/Reset/original-vs-current
// nibble packing; both accessors are declared-only, so this maps the X360 shift to the
// original-count getter.)
const BrnResource::ChallengeListEntry*
ChallengeListComponent::GetFilteredChallenge( s32 liWantedIndex, s32* lpiOutIndex ) const
{
    const BrnResource::ChallengeList* lpList = mpChallengeList;
    const s32 liCount = lpList->GetChallengeCount();

    s32 liMatchCount = 0;
    for ( s32 liIndex = 0; liIndex < liCount; ++liIndex )
    {
        const BrnResource::ChallengeListEntry* lpEntry = lpList->GetChallengeData( liIndex );
        if ( lpEntry->GetOriginalNumPlayers() != miNumPlayers )
        {
            continue;
        }

        if ( liMatchCount != liWantedIndex )
        {
            ++liMatchCount;
            continue;
        }

        if ( lpiOutIndex )
        {
            *lpiOutIndex = liIndex;
        }

        if ( !lpList->IsChallengeContentBought( liIndex ) )
        {
            return 0;
        }
        return lpEntry;
    }

    return 0;
}

// GetHighlightedChallengeID @ 0x82435040
// The CgsID of the currently highlighted challenge, or 0 when the list is empty or the
// highlighted slot resolves to no bought challenge.
CgsID ChallengeListComponent::GetHighlightedChallengeID() const
{
    if ( miNumChallenges <= 0 )
    {
        return 0;
    }

    const BrnResource::ChallengeListEntry* lpEntry =
        GetFilteredChallenge( miStartChallengeIndex + miHighlightedIndex, 0 );
    if ( !lpEntry )
    {
        return 0;
    }

    return lpEntry->GetChallengeID();
}

// HandleEveryPlayerCompletionStatus @ 0x8241B618
// Whole-block copy of the incoming every-player completion status into the embedded block,
// then mark the view dirty. The X360 memcpy length (0x838 == 2104) is exactly
// sizeof the completion-status block.
void ChallengeListComponent::HandleEveryPlayerCompletionStatus(
    const BrnGameState::GameStateModuleIO::FburnChallengeEveryPlayerStatusData* lpStatus )
{
    memcpy( &mEveryPlayerCompletionStatus, lpStatus, sizeof( mEveryPlayerCompletionStatus ) );
    mbDirty = true;
}

// Setup @ 0x82441170
// Bind the component to a GuiCache's freeburn challenge list for a given player count. Counts
// how many challenges match liNumPlayers (their original player-count nibble), resets the
// window state, decides whether the select button shows, and refreshes the ticker.
void ChallengeListComponent::Setup( GuiCache* lpGuiCache, s32 liNumPlayers, bool lbShowButton )
{
    CGS_ASSERT( lpGuiCache != 0, "lpGuiCache" );

    miNumPlayers    = liNumPlayers;
    mpGuiCache      = lpGuiCache;
    mpChallengeList = lpGuiCache->GetFreeburnChallengeList();

    miNumChallenges       = 0;
    miStartChallengeIndex = 0;
    miHighlightedIndex    = 0;
    mbDirty               = true;

    const s32 liCount = mpChallengeList->GetChallengeCount();
    for ( s32 liIndex = 0; liIndex < liCount; ++liIndex )
    {
        const BrnResource::ChallengeListEntry* lpEntry = mpChallengeList->GetChallengeData( liIndex );
        if ( lpEntry->GetOriginalNumPlayers() == liNumPlayers )
        {
            ++miNumChallenges;
        }
    }

    mbShowButton = ( lbShowButton && miNumChallenges > 0 );

    ShowDescriptionInTicker();
}

namespace {
    template<class T> void Output(CgsGui::StateInterface* state, T& event)
    {
        CgsGui::GuiEventWrapper<T, 40> wrapper(event);
        state->GetOutputEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&wrapper), 40, sizeof(wrapper));
    }
    void NavigationAudio(CgsGui::StateInterface* state)
    {
        GuiAudioTriggerEvent event;
        event.Construct(7, "", "MenuToggleDefault");
        struct Record { s32 size, type, offset; GuiAudioTriggerWirePayload457 payload; } record = {100, 457, 12, {}};
        std::memcpy(&record.payload, event.macComponent, sizeof(record.payload));
        state->GetOutputEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&record), 40, sizeof(record));
    }
}

// ARTIST 0x8243D930, including the original four identical ticker posts.
void ChallengeListComponent::ShowDescriptionInTicker()
{
    GuiEventTickerClearMessages clear = {{0, 1}};
    Output(mpStateInterface, clear);
    s32 index;
    const auto* entry = GetFilteredChallenge(miStartChallengeIndex + miHighlightedIndex, &index);
    GuiEventTickerCustomMessage message = {};
    message.Construct(true, false, true, false);
    message.AddString("%1", 1);
    message.AddString(entry ? entry->GetDescriptionStringID() : "No Challenges Available", entry ? 2 : 1);
    for (s32 i = 0; i < 4; ++i) Output(mpStateInterface, message);
}

// ARTIST 0x82440F90.
bool ChallengeListComponent::HighlightPrevious()
{
    if (miStartChallengeIndex + miHighlightedIndex <= 0) return false;
    NavigationAudio(mpStateInterface);
    if (miHighlightedIndex != 0) --miHighlightedIndex;
    else --miStartChallengeIndex;
    mbDirty = true;
    ShowDescriptionInTicker();
    return true;
}

// ARTIST 0x82441078.
bool ChallengeListComponent::HighlightNext()
{
    if (miStartChallengeIndex + miHighlightedIndex >= miNumChallenges - 1) return false;
    NavigationAudio(mpStateInterface);
    if (miHighlightedIndex == 4) ++miStartChallengeIndex;
    else ++miHighlightedIndex;
    mbDirty = true;
    ShowDescriptionInTicker();
    return true;
}

// ARTIST 0x82434CC8. Skip unowned content while filling the five visible slots.
void ChallengeListComponent::Update()
{
    if (!mbDirty) return;
    char highlighted[16];
    CgsCore::SnPrintf(highlighted, sizeof(highlighted), "%d", miHighlightedIndex);
    AddOutputAptViewState("apt_HighlightedIndex", highlighted, false);
    AddOutputAptViewState("apt_show_button", mbShowButton ? "1" : "0", false);
    s32 examined = 0;
    for (s32 row = 0; row < KI_MAX_DISPLAYABLE_CHALLENGES; ++examined)
    {
        const s32 filteredIndex = miStartChallengeIndex + examined;
        if (filteredIndex >= miNumChallenges)
        {
            AddOutputAptViewState(maacAptState[row++], "invisible", false);
            continue;
        }
        s32 index;
        const auto* entry = GetFilteredChallenge(filteredIndex, &index);
        if (entry == 0) continue;
        CGS_ASSERT(static_cast<u32>(index) < 2000, "Challenge index out of range");
        const auto* completion = mEveryPlayerCompletionStatus.GetLocalPlayerCompletionStatus();
        const bool done = (completion->maxBits[index / 64] & (u64(1) << (index % 64))) != 0;
        AddOutputAptViewState(maacAptState[row], done ? "completed" : "unCompleted", false);
        char textId[64], number[64];
        CgsCore::SnPrintf(textId, sizeof(textId), "$CHALLENGE_COMP_DESC_%d", row);
        CgsCore::SnPrintf(number, sizeof(number), "%d", filteredIndex + 1);
        textId[63] = number[63] = 0;
        auto* language = mpStateInterface->GetLanguageManager();
        language->FormatAndAddText(textId + 1, language->GetCurrentLanguage() == 10 ? "%1 : %2" : "%1: %2",
            CgsLanguage::LanguageManager::E_FORMAT_TEXT, 2,
            number, CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
            entry->GetTitleStringID(), CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
        AddOutputAptViewState(maacAptStateText[row], textId, false);
        ++row;
    }
    mbDirty = false;
}

} // namespace BrnGui
