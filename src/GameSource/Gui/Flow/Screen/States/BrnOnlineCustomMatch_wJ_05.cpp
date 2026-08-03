// ===================================================================================
// BrnGui::OnlineCustomMatch -- wave-J partfile 05.
//   HandleInGameFailedEvent  @0x82497F50  (assert cpp:1032)
//   CheckForCompletedLoads   @0x824A34A0  (assert cpp:432)
//   HandleGuiCacheEvent      @0x82497C50
//
// All three bodies compile against the grown headers: BrnTable.h now declares
// Table::HasData() (h:190) and Table::SetupTable(TableDataSet*, bool, bool) (h:185), and
// BrnGuiCache.h now carries `friend struct OnlineCustomMatch;` (h:803) over the two
// mbOnlineMatch* bytes the ticker-line choice reads.
//
// ===================================================================================
// WHAT HandleInGameFailedEvent DOES  (@0x82497F50)
// -----------------------------------------------
// The custom-match screen observes network event id 51 ("in-game start failed"). When it
// arrives the screen tells the overlay system that the "entering game" wait overlay is
// finished (so the spinner comes down), then rewinds itself to its initial screen.
//
// MEASURED FROM THE ASM (rung 1), not from Hex-Rays
// ------------------------------------------------
//  * The assert message is a COPY-PASTE in the ORIGINAL: it names
//    "OnlineSelectRoute::HandleInGameEvent" while living in BrnOnlineCustomMatch.cpp
//    (line 1032 == `li r5, 0x408` at 0x82497FCC). Kept verbatim -- it is what the X360
//    image carries. The assert is non-fatal (BeginAssert / FireAssert / EndAssert with no
//    early-out at 0x82497F68..0x82497FDC), so a null event falls straight into the body;
//    nothing below dereferences it, which is why that is harmless.
//  * The posted record is built on the stack at 0x82497FF0..0x82498020 as
//        +0x00  8      payload byte count   (`li r11, 8`   / stw var_40)
//        +0x04  188    event id             (`li r11, 0xBC`/ stw var_3C)
//        +0x08  16     payload offset       (`li r11, 0x10`/ stw var_38)
//        +0x10  the 8-byte CgsID            (`ld` from the Construct out-param, `std`)
//    and published with AddEvent(queue, record, 40 /*channel*/, 0x18 /*24 bytes*/).
//    NOTE the payload-offset word is 16, NOT the usual 12: the payload is an 8-byte
//    aligned CgsID, so it starts at +0x10 and the record is 24 bytes. On the host the
//    identical alignment falls out of `CgsID mOverlayId` following the 12-byte
//    CgsGui::GuiEvent<188> base -- both numbers below are host expressions (offsetof /
//    sizeof), never the console literals, and are pinned by static_assert.
//  * The queue is `mpStateInterface + 12` on the console (`lwz r11,0x1C(r28)` +
//    `addi r3,r11,0xC`) == &StateInterface::mOutEventQueue == GetOutputEventQueue().
//  * No float compares anywhere in this body, so there is no PPC NaN-polarity decision.
//
// THE ONE DE-INLINE THIS BODY MAKES, AND WHY
// ------------------------------------------
// The X360 `bl`s BrnGui::GuiOverlayWaitFinishRequest::Construct @0x823B1D80 with the
// record's payload slot and "CNOnlEntGame". That callee IS homed and defined in the tree:
// GameSource/Gui/BrnGuiOverlaysDirector.h:36 inline-defines it as exactly
//     mOverlayId = CgsIDCompress(lpcOverlayName);   // then `return this`
// (six committed siblings already call it -- BrnPauseScreen.cpp, BrnInGame.cpp,
// BrnCarSelectMain_wG_02.cpp, BrnCrashNavEnterOnline_wI_07.cpp and the two
// BrnOnlineGameRoomPlayerInfo wave-H partfiles). It is unreachable FROM THIS TU only
// because BrnGuiOverlaysDirector.h and BrnGuiDemangledEventTypes.h are mutually exclusive
// -- both define GuiOverlayWaitFinishRequest and GuiOverlayShowingNotification (see the
// note at BrnGuiDemangledEventTypes.h:281-284) -- and BrnOnlineCustomMatch.h:7 pulls in the
// demangled one for the two mSearchResults / mLastSearchParams member types. The mirror it
// carries (BrnGuiDemangledEventTypes.h:226) is a payload-only `u8 maData[8]` with no
// Construct and no named id field, and poking a CgsID into maData would be the
// offset-hack failure mode. So the wire record below spells its single 8-byte payload word
// as a named `CgsID mOverlayId` and fills it with CgsIDCompress -- the same two statements
// the homed Construct runs. The assert Construct skips is on a compile-time string literal
// that can never be null.
// (NOT the same symbol: the Construct in GameSource/Game/GameBridgeNetworkToX.h:103 is a
// DIFFERENT function -- the static Construct(void*, const char*) @0x823E0E98 on that
// header's own 24-byte record, whose body memsets. It is unrelated to 0x823B1D80.)
// FLAG: this is the one place this file departs from a call-for-call transliteration.
//
// LINK NOTES (`cl /c` cannot see any of these -- do not go hunting)
// -----------------------------------------------------------------
//  * BrnGui::OnlineCustomMatch::ShowInitialScreen -- declared BrnOnlineCustomMatch.h:91,
//    body owned by the wave-J group-1 partfile; no body in the tree yet.
//  * CgsIDCompress @0x82815A20 -- declared CgsID.h:29 and DEFINED in CgsID.cpp; NOT a gap.
//  * GuiOverlayWaitFinishRequest::Construct is NOT a gap either (homed + defined at
//    BrnGuiOverlaysDirector.h:36); it is simply not called from here.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include <cstddef>                                                       // offsetof (wire pins)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / the out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent

namespace BrnGui
{
    namespace
    {
        // The out-queue channel selector (X360 `li r5, 0x28` at 0x82498000). Shared with
        // the other wave-J partfiles of this TU -- keep exactly one copy on merge.
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The overlay this screen raised when it began joining a game and now dismisses
        // (X360 rodata aCnonlentgame, loaded at 0x82497FE0/0x82497FE8). The '1' arm of
        // HandleControllerInputSelectGame raises the very same overlay id, so that
        // partfile carries its own copy -- keep exactly one on merge.
        const char KAC_ENTER_GAME_OVERLAY_ID[] = "CNOnlEntGame";

        // ---- out-queue wire record -----------------------------------------------------
        // The id-188 "this wait overlay has finished" request. Its payload is the single
        // compressed overlay id. The REAL home of that payload type is
        // BrnGuiOverlaysDirector.h:29 (`struct GuiOverlayWaitFinishRequest { CgsID
        // mOverlayId; ... }`), which this TU cannot include (see the de-inline note above);
        // the mirror reachable from here, BrnGuiDemangledEventTypes.h:226, is `u8
        // maData[8]` -- byte-aligned, so embedding IT would seat the payload at +0x0C where
        // the console seats it at +0x10. The payload is a CgsID (`std r3, 0(r27)` in the
        // console's Construct), so it is typed as one here and the 8-byte alignment
        // reproduces the console's +0x10 / 24-byte record on the host.
        struct GuiOverlayWaitFinishRequestWire : public CgsGui::GuiEvent<188>
        {
            CgsID mOverlayId;   // +0x10 -- CgsIDCompress("CNOnlEntGame")

            explicit GuiOverlayWaitFinishRequestWire(CgsID lOverlayId)
                : CgsGui::GuiEvent<188>(
                      static_cast<u32>(sizeof(CgsID)),
                      static_cast<u32>(offsetof(GuiOverlayWaitFinishRequestWire, mOverlayId)))
                , mOverlayId(lOverlayId)
            {
            }
        };

        // Layout pins: the console publishes payload-size 8, payload-offset 16, record 24.
        typedef char KAC_ASSERT_WAIT_FINISH_PAYLOAD_SIZE[sizeof(CgsID) == 8 ? 1 : -1];
        typedef char KAC_ASSERT_WAIT_FINISH_WIRE_SIZE[
            sizeof(GuiOverlayWaitFinishRequestWire) == 24 ? 1 : -1];
        typedef char KAC_ASSERT_WAIT_FINISH_PAYLOAD_OFFSET[
            offsetof(GuiOverlayWaitFinishRequestWire, mOverlayId) == 16 ? 1 : -1];
    }

    // ------------------------------------------------- HandleInGameFailedEvent @0x82497F50
    void OnlineCustomMatch::HandleInGameFailedEvent(const CgsModule::Event* lpEvent)
    {
        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out). The message text
        // is the ORIGINAL's copy-paste from the online-select-route screen -- kept verbatim.
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleInGameEvent");   // cpp:1032

        // Take down the "entering game" wait overlay.
        const GuiOverlayWaitFinishRequestWire lRequest(CgsIDCompress(KAC_ENTER_GAME_OVERLAY_ID));

        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lRequest)));   // X360 record size 24

        // ...and put the screen back to where it started.
        ShowInitialScreen();
    }
}

// WHAT THE FUNCTION DOES  (@0x824A34A0)
// -------------------------------------
// The screen's two-step load pump, driven from Update(). Step 0 waits for the custom-match
// screen's static resource list to finish loading, then starts its apt movie. Step 1 waits
// for every apt component the movie declares to finish initialising, then binds the
// found-games table to its row-data set, drops the now-satisfied expected-component list,
// and hands over to the initial screen. Every other sub-state does nothing.
//
// MEASURED FROM THE ASM (rung 1), not from Hex-Rays
// ------------------------------------------------
//  * The leading assert is non-fatal (BeginAssert / FireAssert / EndAssert, no early-out,
//    0x824A34C0..0x824A34DC): "mpGuiCache", cpp:432 (`li r5, 0x1B0`). The sub-state-0 arm
//    then dereferences mpGuiCache WITHOUT re-checking it, while the sub-state-1 arm DOES
//    re-check it (`cmplwi cr6, r3, 0 / beq` @0x824A354C) -- both reproduced as written.
//  * ⭐ THE DOSSIER PSEUDOCODE DROPS TWO ARGUMENTS. Hex-Rays prints
//    `GuiCache::EnsureResourcesAreLoaded(*(v1 + 17424))`, but the asm @0x824A34EC..0x824A34FC
//    sets up THREE registers before the `bl`:
//        lis   r11, unk_8205E77C@ha        ; &maResourceTuplesToLoad
//        lwz   r3,  0x4410(r31)            ; this->mpGuiCache
//        li    r5,  1                      ; the tuple count
//        addi  r4,  r11, unk_8205E77C@l
//    which is exactly the committed two-arg declaration (BrnGuiCache.h:212). The count is
//    written as miNumResourcesToLoad (== 1, BrnScreenStatesDataLinkStubs.cpp:167), never
//    as the console literal.
//  * The return value is consumed as a BYTE (`clrlwi r11, r3, 24` @0x824A3500 and again at
//    0x824A355C) -- both callees really do return bool, as declared.
//  * PlayAptMovie's movie name is a POINTER loaded from the screen-flow's per-screen
//    name table (`lwz r4, (off_82F27B9C - 0x82F278E0)(r11)` @0x824A351C -> "ON_CUSTM"),
//    with level 3 (`li r5, 3`); the table is not this TU's static, so the literal is
//    file-local below.
//  * `lwz r11, 0x76D0(r31)` @0x824A3568 is this+30416 == mTable(17432) + 0x32B8 ==
//    Table::mpData (SelectableGroup base + maRows[16] * 0x308 = 0x238 + 0x3080 = 0x32B8),
//    i.e. the INLINED Table::HasData(); the branch is taken when it is non-null, so the
//    guard is !mTable.HasData().
//  * SetupTable's second argument is `addis r4, r31, 1 / addi r4, r4, -0x655C` ==
//    this + 0x10000 - 0x655C == this + 39588 == &mTableData; r5 and r6 are both `li 0`.
//    Reached by member name here -- no console displacement survives in the code.
//  * ClearExpectedAptComponentList and AreAllAptComponentsInitialised both take `li r4, 0`
//    == E_GUIFLOW_SCREEN (BrnGuiEventTypeDefs.h:74).
//  * The console's flat `if (substate) { if (substate == 1) ... } else ...` is the compiler's
//    rendering of the source switch; written back as a switch over ESubState.
//  * No float compares anywhere in this body, so there is no PPC NaN-polarity decision.
// =============================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

namespace BrnGui
{
    namespace
    {
        // The custom-match screen's apt movie. The X360 does not hold this string as a
        // static of this class: PlayAptMovie is handed the pointer parked at 0x82F27B9C,
        // one slot of the screen-flow's per-screen movie-name table (0x82F27B90..).
        const char KAC_APT_MOVIE_NAME[] = "ON_CUSTM";

        // The apt movie's layer/level argument (X360 `li r5, 3` @0x824A3514). FLAG: the
        // parameter is `s32 liLevelNum` (CgsGuiStateInterface.h:136); nothing in the image
        // names the 3, so it stays a bare measured constant.
        const s32 KI_APT_MOVIE_LEVEL = 3;
    }

    // ------------------------------------------------- CheckForCompletedLoads @0x824A34A0
    void OnlineCustomMatch::CheckForCompletedLoads()
    {
        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out).
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:432

        switch (meSubState)
        {
        case E_SUBSTATE_LOADING_SCREEN:
            // Nothing can be shown until the screen's own resource list is resident; the
            // moment it is, kick the apt movie off and move to waiting on its components.
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie(KAC_APT_MOVIE_NAME, KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
            break;

        case E_SUBSTATE_LOADING_COMPONENTS:
            // Wait for every expected apt component; bind the table exactly once (the
            // HasData test is what makes this a one-shot), then start the screen proper.
            if (mpGuiCache != 0 &&
                mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN) &&
                !mTable.HasData())
            {
                mTable.SetupTable(&mTableData, false, false);
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                ShowInitialScreen();
            }
            break;

        default:
            break;
        }
    }
}

// WHAT THE FUNCTION DOES  (@0x82497C50)
// -------------------------------------
// The one-shot arrival of the GUI cache pointer. On the FIRST cache event (and only the
// first -- a second one returns immediately because mpGuiCache is already latched) the
// screen latches the pointer, publishes the online ticker line that matches how the match
// was entered, and then either registers every apt component this page must wait on, or --
// if the profile is not allowed to play multiplayer at all -- backs straight out, lifting
// the network suspension first if this screen owns the pending online start.
//
// MEASURED FROM THE ASM (rung 1), not from Hex-Rays
// ------------------------------------------------
//  * The in-queue hands handlers the HEADER-STRIPPED payload, so the incoming cache
//    pointer is the payload's first word (`lwz r11, 0(r25)` @0x82497C6C, re-read as
//    `lwz r30, 0(r25)` @0x82497CFC). Same file-local view the wave-I/H twins carry.
//  * Both asserts are non-fatal (BeginAssert / FireAssert / EndAssert, no early-out):
//    "Invalid cache in HandleGuiCacheEvent::Update" at cpp:955 (`li r5, 0x3BB`) -- the
//    "::Update" is a copy-paste in the ORIGINAL, kept verbatim -- and a second bare
//    "mpGuiCache" at cpp:1340 (`li r5, 0x53C`) after the ticker post.
//  * TICKER PAYLOAD SEEDS, measured payload-relative at 0x82497D08..0x82497D20:
//        +0x810 = 0   mi8NumStrings
//        +0x811 = 1   maFlags[0]   <-- this producer's distinguishing seed
//        +0x812 = 0   maFlags[1]
//        +0x813 = 1   maFlags[2]
//        +0x814 = 0   maFlags[3]
//    with maiStringTypes (+0x00..+0x0F) zeroed by two `std` @0x82497D30/D34 and the
//    0x800-byte string block zeroed by the memset @0x82497D28. A whole-struct memset plus
//    the two flag stores is identical. (Seeds {1,0,1,0}; BrnGui::CarSelectVehicle::SetTicker
//    seeds {0,0,1,0}, so flag 0 is what distinguishes the two ticker kinds.)
//  * The console builds the payload in a stack scratch and memcpy's it into the record
//    payload (@0x82497D7C..D88); built directly in the record here -- the same bytes on
//    the wire, and the precedent BrnCarSelectVehicle_Input.cpp / wI_09 both do this.
//  * The ticker record is { 0x818, 537, 12 } + the 0x818-byte payload, channel 40, 0x824
//    bytes (@0x82497D8C..DB8). All three numbers are host `sizeof` expressions below.
//  * The nine name registrations are `sub_824F87C0(cache, 0, component + 4)`. +4 is
//    CgsGui::GuiComponent::macName, i.e. the component's GetName(); sub_824F87C0 is the
//    name-taking entry of GuiCache::AppendExpectedAptComponent (decl BrnGuiCache.h:233).
//    Order measured from the `addi r5, r31, <disp>` chain @0x82497E74..0x82497ED0:
//        0x41C4 = 16836 = mMessageText            + 4
//        0x42EC = 17132 = mNumGamesFoundText      + 4
//        0x0040 =    64 = mMessageAnimation       + 4
//        0x00CC =   204 = mMessageButtonsAnimation+ 4
//        0x0158 =   344 = mSearchParamsAnimation  + 4
//        0x01E4 =   484 = mFoundGamesAnimation    + 4
//        0x0270 =   624 = mButtonPromptAnimation  + 4
//    then the two loops (@0x82497EE8 stride 0x128 base 0x76DC == maTextFields[i].macName,
//    bound 0x14 == 20; @0x82497F1C stride 0x94 base 0x8DFC == maIcons[j].macName, bound
//    0xF == 15). NO CONSOLE LITERAL SURVIVES BELOW: every one of those displacements is
//    reached by member name and the numbers live only in this comment.
//  * The loop counters are held as bytes (`addi`/`extsb` @0x82497F00/F04 and F34/F38) --
//    a console codegen detail, not a behaviour; written as ordinary s32 loops.
//  * The suspension event is posted onto the out-queue DIRECTLY rather than through
//    CgsGui::StateInterface::OutputGuiEvent, whose committed body passes the event id (45)
//    to AddEvent as the channel where the X360 passes 40. Same documented accommodation
//    BrnOnlineGameOptions_wI_09.cpp makes.
//  * No float compares anywhere in this body, so there is no PPC NaN-polarity decision.
// =============================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include <cstring>                                                       // memset / strncpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // GuiComponent::GetName
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (friend)

namespace BrnGui
{
    namespace
    {
        // KI_CHANNEL_GUI_OUT (== 40, X360 `li r5, 0x28`) is already declared in this
        // partfile's HandleInGameFailedEvent section above -- one copy per TU.

        // The ticker string's format/kind selector (X360 `li r5, 2` @0x82497D70) -- the same
        // word BrnGui::CarSelectVehicle::SetTicker and OnlineGameOptions pass to AddString.
        const s32 KI_TICKER_STRING_TYPE = 2;

        // ---- the three ticker lines (rodata literals @0x82497D44/D5C/D68) ---------------
        const char KAC_RANKED_TICKER_TEXT[]   = "ONLINE_RANKED_TICKER_TEXT";
        const char KAC_FREEBURN_TICKER_TEXT[] = "ONLINE_FREEBURN_TICKER_TEXT";
        const char KAC_UNRANKED_TICKER_TEXT[] = "ONLINE_UNRANKED_TICKER_TEXT";

        // ---- the two state-machine events this handler can send -------------------------
        const char KAC_STATE_EVENT_GO_BACK[]      = "GO_BACK";
        const char KAC_STATE_EVENT_GO_BACK_EASY[] = "GO_BACK_EASY";

        // ---- in-queue payload view -----------------------------------------------------
        // The DWARF types the parameter const GuiEventCache*, whose home header is one of
        // the mutually-exclusive event-type-def pair, so this is the same file-local view
        // the wave-H/I twins (BrnOnlineGameRoomPlayerInfo_wH_18.cpp,
        // BrnOnlineGameOptions_wI_09.cpp) carry.
        struct GuiEventCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;   // +0x00
        };

        // ---- out-queue wire record -----------------------------------------------------
        // The custom ticker message payload (0x818 bytes). Layout recovered store-for-store
        // from BrnGui::GuiEventTickerCustomMessage::AddString @0x823A6940, whose asserts bake
        // "GameSource/Gui/BrnGuiEventTypeDefs.h" lines 390/391/392:
        //   +0x000  s32  maiStringTypes[4]
        //   +0x010  char maacStrings[4][512]
        //   +0x810  s8   mi8NumStrings          (`lbz`/`extsb`, bounded < 4)
        //   +0x811..+0x814  four flag bytes
        // Kept TU-LOCAL rather than promoted: the homed twin in BrnGuiDemangledEventTypes.h:182
        // is an opaque `GuiEvent<537> + u8 maPayload[2060]` whose 2072-byte total treats the
        // payload size as the whole record, so it cannot carry these fields; a second
        // definition of the real name would be a live ODR fork. Identical to the view
        // BrnCarSelectVehicle_Input.cpp and BrnOnlineGameOptions_wI_09.cpp carry.
        struct GuiTickerCustomMessagePayload
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;     // AddString's bound (h:391)
            static const s32 KI_MAX_STRING_LENGTH = 512;   // AddString's strncpy count

            s32  maiStringTypes[KI_MAX_NUM_STRINGS];                       // +0x000
            char maacStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH];    // +0x010
            s8   mi8NumStrings;                                            // +0x810
            // FLAG: four flag bytes at +0x811..+0x814 whose roles are not recovered.
            u8   maFlags[4];                                               // +0x811
            u8   maPad815[3];                                              // +0x815 (sizeof == 0x818)

            // @0x823A6940 -- copy lpString into the next free 512-byte slot and record its
            // format type. The count is read as a SIGNED byte (X360 `lbz` + `extsb`).
            void AddString(const char* lpString, s32 liType)
            {
                CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");                   // h:390
                CGS_ASSERT(mi8NumStrings < KI_MAX_NUM_STRINGS,
                           "mi8NumStrings < KI_MAX_NUM_STRINGS");                       // h:391
                CGS_ASSERT(lpString != 0, "lpString");                                  // h:392

                std::strncpy(maacStrings[mi8NumStrings], lpString,
                             static_cast<size_t>(KI_MAX_STRING_LENGTH));
                maiStringTypes[mi8NumStrings] = liType;
                ++mi8NumStrings;
            }
        };

        // { 0x818, 537, 12, <the message> }, channel 40, 0x824 bytes -- all three written as
        // host expressions.
        struct GuiTickerCustomMessageWire : public CgsGui::GuiEvent<537>
        {
            GuiTickerCustomMessagePayload mMessage;   // +0x0C

            GuiTickerCustomMessageWire()
                : CgsGui::GuiEvent<537>(static_cast<u32>(sizeof(GuiTickerCustomMessagePayload)),
                                        static_cast<u32>(sizeof(CgsGui::GuiEvent<537>)))
            {
                std::memset(&mMessage, 0, sizeof(mMessage));
                mMessage.maFlags[0] = 1;   // +0x811 (this producer's distinguishing seed)
                mMessage.maFlags[2] = 1;   // +0x813
            }
        };

        // Layout pins: the console posts 0x824 bytes with payload-size word 0x818 and
        // payload-offset word 12.
        typedef char KAC_ASSERT_TICKER_PAYLOAD_SIZE[
            sizeof(GuiTickerCustomMessagePayload) == 0x818 ? 1 : -1];
        typedef char KAC_ASSERT_TICKER_WIRE_SIZE[
            sizeof(GuiTickerCustomMessageWire) == 0x824 ? 1 : -1];
    }

    // ---------------------------------------------------- HandleGuiCacheEvent @0x82497C50
    void OnlineCustomMatch::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiEventCachePayload* lpCacheEvent =
            reinterpret_cast<const GuiEventCachePayload*>(lpEvent);

        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out) -- the X360 falls
        // straight through, so a null cache would be latched as-is. "::Update" is the
        // ORIGINAL's copy-paste, kept verbatim.
        CGS_ASSERT(lpCacheEvent->mpGuiCache != 0,
                   "Invalid cache in HandleGuiCacheEvent::Update");   // cpp:955

        // ---- first arrival only: the whole page setup is a one-shot -------------------
        if (mpGuiCache != 0)
        {
            return;
        }

        mpGuiCache = lpCacheEvent->mpGuiCache;

        // ---- the ticker line ----------------------------------------------------------
        // Which of the three online blurbs runs along the ticker depends on how the match
        // was entered: ranked, free-burn, or plain unranked.
        {
            const char* lpacTickerText;
            if (mpGuiCache->mbOnlineMatchRanked)             // GuiCache +0x4B51
            {
                lpacTickerText = KAC_RANKED_TICKER_TEXT;
            }
            else if (mpGuiCache->mbOnlineMatchUnranked)      // GuiCache +0x4B52
            {
                lpacTickerText = KAC_FREEBURN_TICKER_TEXT;
            }
            else
            {
                lpacTickerText = KAC_UNRANKED_TICKER_TEXT;
            }

            GuiTickerCustomMessageWire lTicker;
            lTicker.mMessage.AddString(lpacTickerText, KI_TICKER_STRING_TYPE);

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lTicker), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lTicker)));   // X360 record size 0x824
        }

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1340

        // ---- either arm the page, or back straight out --------------------------------
        if (mpGuiCache->IsMultiplayerAllowed())
        {
            // Tell the cache which apt components this page has to wait on before it can
            // report the screen flow ready. The two composite components have their own
            // helper; everything else is registered by name.
            mSearchParms.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);
            mMessageButtons.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);

            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageText.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mNumGamesFoundText.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageAnimation.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMessageButtonsAnimation.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mSearchParamsAnimation.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mFoundGamesAnimation.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mButtonPromptAnimation.GetName());

            // ...then every cell component of the found-games table.
            for (s32 liTextField = 0; liTextField < KI_NUM_ONLINE_FOUND_GAMES_TEXT_FIELDS;
                 ++liTextField)
            {
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                       maTextFields[liTextField].GetName());
            }

            for (s32 liIcon = 0; liIcon < KI_NUM_ONLINE_FOUND_GAMES_ICONS; ++liIcon)
            {
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                       maIcons[liIcon].GetName());
            }
        }
        else if (mpGuiCache->IsOnlineStartPending())        // GuiCache +0x4B53
        {
            // No multiplayer privilege, and this screen owns the pending online start:
            // lift the network suspension it armed, then back out on the quiet path.
            CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lNetworkSuspension)));   // X360 record size 16

            mpGuiCache->SetOnlineStartPending(false);
            SendStateEvent(KAC_STATE_EVENT_GO_BACK_EASY);
        }
        else
        {
            SendStateEvent(KAC_STATE_EVENT_GO_BACK);
        }
    }
}
