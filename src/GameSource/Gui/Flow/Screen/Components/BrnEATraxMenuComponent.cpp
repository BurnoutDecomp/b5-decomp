// ===================================================================================
// BrnGui::EATraxMenuComponent -- the EA Trax track list on the pause menu's CN_TRAX tab.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct     @0x82419680   BuildStringIDs @0x824196E8   UpdatePlayOrderMode @0x824197F0
//   GetTrackMode  @0x82426A60   SetTrackMode   @0x82426DA0   SendDrawInformationToApt @0x82427560
//   Initialize    @0x8242D790   Update         @0x8242D858   HandleInput @0x8243BC40
//
// Until 2026-09-16 this TU held only GetTrackMode/SetTrackMode and was never mounted, so
// the whole EA Trax screen had no component to drive. See BrnEATraxMenuComponent.h for the
// recovered layout and the inverted 2-bit mode encoding.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnEATraxMenuComponent.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"                  // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiAudioTriggerEvent

#include <cstring>   // std::memcpy

namespace BrnGui
{
    // ---- class statics -------------------------------------------------------------
    // X360 .rdata flt_82F25640 == 5.0f, the float the scroll-bar arithmetic uses.
    const f32 EATraxMenuComponent::KF_NUM_VISIBLE_TRACKS = 5.0f;

    // The six apt id tables are STATIC on the console too -- fixed .bss at 0x82FB2790 /
    // 0x82FB0070 / 0x82FB00D0 / 0x82FB2740 / 0x82FB0120 / 0x82FB26F0, five 16-byte slots
    // each. BuildStringIDs fills them.
    char EATraxMenuComponent::msaacVisibleIDs  [KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE] = {};
    char EATraxMenuComponent::msaacSongIndexIDs[KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE] = {};
    char EATraxMenuComponent::msaacNameIDs     [KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE] = {};
    char EATraxMenuComponent::msaacArtistIDs   [KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE] = {};
    char EATraxMenuComponent::msaacAlbumIDs    [KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE] = {};
    char EATraxMenuComponent::msaacEnabledIDs  [KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE] = {};

    const char* EATraxMenuComponent::mspacBarStartID = "BarStart";   // .rdata off_82F25320
    const char* EATraxMenuComponent::mspacBarEndID   = "BarEnd";     // .rdata off_82F25324

    // The play-order caption's own clip, built as a child of this component.
    const char EATraxMenuComponent::KAC_PLAY_ORDER_MODE_NAME[12] = "PlayMode_mc";

    // .rdata off_82F25328 -- indexed by GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode, so
    // SEQUENTIAL is 0 and RANDOM is 1 (which is also what UpdatePlayOrderMode's
    // `liPlayOrderMode == RANDOM || liPlayOrderMode == SEQUENTIAL` assert allows: 0 and 1).
    const char* EATraxMenuComponent::KAPC_PLAY_ORDER_MODES[2] =
    {
        "$EATRAX_PLAY_ORDER_SEQUENTIAL",
        "$EATRAX_PLAY_ORDER_RANDOM",
    };

    // .rdata off_82F25330 -- indexed by ETraxPlayState.
    const char* EATraxMenuComponent::KAPC_TRAX_PLAY_STATE[E_TRAX_PLAY_STATE_COUNT] =
    {
        "$EATRAX_MENU_TRAX_ENABLED",
        "$EATRAX_MENU_TRAX_EVENTONLY",
        "$EATRAX_MENU_TRAX_FREEBURNONLY",
        "$EATRAX_MENU_TRAX_DISABLED",
    };

    namespace
    {
        const char* const KAC_EMPTY = "";
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The menu cue this component raises when the list SCROLLS (see HandleInput).
        // The X360 posts it through OutputGuiEvent<GuiAudioTriggerEvent> @0x82436890,
        // whose queued record is { size 100, type 457, offset 12 } + the native
        // { component[32], action, label[32], movie[32] } payload. Spelled with the same
        // hand-built record BrnChallengeListComponent.cpp:215 and
        // BrnCrashNavOptions.cpp::PostAudioTrigger already use, because this tree's
        // GuiAudioTriggerEvent carries the PC's GuiEvent<201> header rather than the
        // console's wire id 457.
        void PostAudioTrigger(CgsGui::StateInterface* lpStateInterface,
                              s32 liAction, const char* lpacLabel)
        {
            GuiAudioTriggerEvent lEvent;
            lEvent.Construct(liAction, KAC_EMPTY, lpacLabel);

            struct Record
            {
                s32 miSize;
                s32 miType;
                s32 miOffset;
                GuiAudioTriggerWirePayload457 mPayload;
            } lRecord = { 100, 457, 12, {} };
            std::memcpy(&lRecord.mPayload, lEvent.macComponent, sizeof(lRecord.mPayload));

            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lRecord)));
        }
    }

    // ---- Construct @0x82419680 -----------------------------------------------------
    // Base component construct, then the play-order caption as a CHILD of this component
    // (`addi r6, r31, 4` -- macName is the parent prefix), then clear the changed latch.
    // The X360 dispatches the TextField's Construct through its vtable slot 0
    // (`lwz r10, 0xC4(r31)` / `lwz r11, 0(r10)` / `bctrl`), which is what a plain call to
    // the virtual does here. liUnused is the DWARF's 4th parameter; the console's body
    // never reads it (CrashNavTrax::OnEnter passes -1).
    void EATraxMenuComponent::Construct(const char* lpacName,
                                        CgsGui::StateInterface* lpStateInterface,
                                        const char* lpacParentName, s32 /*liUnused*/)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
        mPlayMode.Construct(KAC_PLAY_ORDER_MODE_NAME, lpStateInterface, GetName());
        mbSettingsChanged = false;
    }

    // ---- BuildStringIDs @0x824196E8 ------------------------------------------------
    // Format the six per-row apt ids once. The X360 walks the six .bss tables with the
    // same 16-byte stride and the same SPrintf capacity (16).
    void EATraxMenuComponent::BuildStringIDs()
    {
        for (s32 liRow = 0; liRow < KI_NUM_VISIBLE_TRACKS; ++liRow)
        {
            CgsCore::SPrintf(msaacVisibleIDs[liRow],   KU_MAX_APT_COMMS_STRING_ID_SIZE, "VisibleState%d", liRow);
            CgsCore::SPrintf(msaacSongIndexIDs[liRow], KU_MAX_APT_COMMS_STRING_ID_SIZE, "SongIndex%d",    liRow);
            CgsCore::SPrintf(msaacNameIDs[liRow],      KU_MAX_APT_COMMS_STRING_ID_SIZE, "Name%d",         liRow);
            CgsCore::SPrintf(msaacArtistIDs[liRow],    KU_MAX_APT_COMMS_STRING_ID_SIZE, "Artist%d",       liRow);
            CgsCore::SPrintf(msaacAlbumIDs[liRow],     KU_MAX_APT_COMMS_STRING_ID_SIZE, "Album%d",        liRow);
            CgsCore::SPrintf(msaacEnabledIDs[liRow],   KU_MAX_APT_COMMS_STRING_ID_SIZE, "Enabled%d",      liRow);
        }
    }

    // ---- Initialize @0x8242D790 ----------------------------------------------------
    // Adopt the profile's two bit fields, ask the song list how many tracks there are,
    // rebuild the apt ids and arm the first draw. The two asserts are the console's own
    // (BrnEATraxMenuComponent.cpp:209 / :210).
    void EATraxMenuComponent::Initialize(const GuiEventAudioTraxUpdate::EATraxArrayType* lpTraxEnabledInFreeBurn,
                                         const GuiEventAudioTraxUpdate::EATraxArrayType* lpTraxEnabledInEvents)
    {
        mEATraxEnabledInFreeBurnBitfield = *lpTraxEnabledInFreeBurn;
        mEATraxEnabledInEventsBitfield   = *lpTraxEnabledInEvents;

        mCurrentHighlightedIndex = 0;
        mCurrentTopIndex         = 0;
        mTotalNumberTracks       = mEATraxHelper.GetNumSongs();

        CGS_ASSERT(mTotalNumberTracks <= KI_MAX_TRAX_IN_GAME,
                   "mTotalNumberTracks <= KI_MAX_TRAX_IN_GAME");
        CGS_ASSERT(mTotalNumberTracks > 0, "mTotalNumberTracks > 0");

        mCurrentHighlightedIndex = 0;   // the console stores it a second time
        BuildStringIDs();
        SetSendDrawInformationPending(true);
    }

    // ---- Update @0x8242D858 --------------------------------------------------------
    void EATraxMenuComponent::Update()
    {
        if (IsDrawRequestPending())
        {
            SendDrawInformationToApt();
            SetSendDrawInformationPending(false);
        }
    }

    // ---- HandleInput @0x8243BC40 ---------------------------------------------------
    // ⚠️ THE MENU CUE IS GATED ON THE LIST SCROLLING, not on the highlight moving. Both
    // highlight arms store the new index first and only raise "EaTraxMenu" when the move
    // pushed the window (`ble cr6, 0x8243BE10` @0x8243BCA4 / the mirrored test in the next
    // arm). Every arm, including the ones that do nothing, ends by arming a redraw.
    void EATraxMenuComponent::HandleInput(InputCommand leCommand)
    {
        switch (leCommand)
        {
        case E_INPUTCOMMAND_HIGHLIGHT_PREVIOUS_TRACK:
            if (mCurrentHighlightedIndex > 0)
            {
                --mCurrentHighlightedIndex;
                if (mCurrentTopIndex > mCurrentHighlightedIndex)
                {
                    mCurrentTopIndex = mCurrentHighlightedIndex;
                    PostAudioTrigger(mpStateInterface, 7, "EaTraxMenu");
                }
            }
            break;

        case E_INPUTCOMMAND_HIGHLIGHT_NEXT_TRACK:
            if (mCurrentHighlightedIndex < mTotalNumberTracks - 1)
            {
                ++mCurrentHighlightedIndex;
                if (mCurrentHighlightedIndex > mCurrentTopIndex + (KI_NUM_VISIBLE_TRACKS - 1))
                {
                    ++mCurrentTopIndex;
                    PostAudioTrigger(mpStateInterface, 7, "EaTraxMenu");
                }
            }
            break;

        case E_INPUTCOMMAND_CHANGE_CURRENT_TRACK_STATE_TO_PREVIOUS:
        {
            const s32 liMode = static_cast<s32>(GetTrackMode(mCurrentHighlightedIndex)) - 1;
            SetTrackMode(mCurrentHighlightedIndex,
                         static_cast<ETraxPlayState>(liMode < 0 ? liMode + E_TRAX_PLAY_STATE_COUNT
                                                                : liMode));
            mbSettingsChanged = true;
            break;
        }

        case E_INPUTCOMMAND_CHANGE_CURRENT_TRACK_STATE_TO_NEXT:
        {
            const s32 liMode = static_cast<s32>(GetTrackMode(mCurrentHighlightedIndex)) + 1;
            SetTrackMode(mCurrentHighlightedIndex,
                         static_cast<ETraxPlayState>(liMode > E_TRAX_PLAY_STATE_DISABLED
                                                         ? liMode - E_TRAX_PLAY_STATE_COUNT
                                                         : liMode));
            mbSettingsChanged = true;
            break;
        }

        case E_INPUTCOMMAND_CHANGE_ALL_TRACKS_TO_NEXT:
        {
            // The NEXT state of the HIGHLIGHTED track, applied to every track.
            const s32 liMode = static_cast<s32>(GetTrackMode(mCurrentHighlightedIndex)) + 1;
            const ETraxPlayState leMode =
                static_cast<ETraxPlayState>(liMode > E_TRAX_PLAY_STATE_DISABLED
                                                ? liMode - E_TRAX_PLAY_STATE_COUNT
                                                : liMode);
            for (s32 liTrack = 0; liTrack < mTotalNumberTracks; ++liTrack)
            {
                SetTrackMode(liTrack, leMode);
            }
            mbSettingsChanged = true;
            break;
        }

        default:
            break;
        }

        SetSendDrawInformationPending(true);
    }

    // ---- UpdatePlayOrderMode @0x824197F0 -------------------------------------------
    // Set the caption and latch "settings changed". ⚠️ It does NOT arm a redraw -- that is
    // the console's own store pattern (`stb r11, 0xC1` only, @0x82419858).
    void EATraxMenuComponent::UpdatePlayOrderMode(
        GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode lePlayOrderMode)
    {
        CGS_ASSERT(lePlayOrderMode == GuiEventAudioTraxPlayOrder::E_TRAX_PLAY_ORDER_MODE_RANDOM ||
                   lePlayOrderMode == GuiEventAudioTraxPlayOrder::E_TRAX_PLAY_ORDER_MODE_SEQUENTIAL,
                   "liPlayOrderMode == GuiEventAudioTraxPlayOrder::E_TRAX_PLAY_ORDER_MODE_RANDOM || "
                   "liPlayOrderMode == GuiEventAudioTraxPlayOrder::E_TRAX_PLAY_ORDER_MODE_SEQUENTIAL");

        mPlayMode.SetText(KAPC_PLAY_ORDER_MODES[static_cast<s32>(lePlayOrderMode)]);
        mbSettingsChanged = true;
    }

    // ---- SendDrawInformationToApt @0x82427560 --------------------------------------
    // Push the five visible rows and the two scroll-bar anchors.
    // ⚠️ A ROW OUTSIDE THE TRACK RANGE POSTS ONLY ITS VISIBILITY. The console's
    // out-of-range arm sets the shared (name, view-state) pair to
    // (msaacVisibleIDs[row], "Invisible") and jumps straight to the single trailing
    // AddOutputAptViewState -- it does not blank the other five fields.
    void EATraxMenuComponent::SendDrawInformationToApt()
    {
        for (s32 liRow = 0; liRow < KI_NUM_VISIBLE_TRACKS; ++liRow)
        {
            const s32 liTrack = mCurrentTopIndex + liRow;

            if (liTrack < 0 || liTrack >= mTotalNumberTracks)
            {
                AddOutputAptViewState(msaacVisibleIDs[liRow], "Invisible", false);
                continue;
            }

            AddOutputAptViewState(msaacVisibleIDs[liRow],
                                  liTrack == mCurrentHighlightedIndex ? "Highlighted"
                                                                      : "Unhighlighted",
                                  false);

            char lacIndex[8];
            CgsCore::SPrintf(lacIndex, sizeof(lacIndex), "%d", liTrack + 1);
            AddOutputAptViewState(msaacSongIndexIDs[liRow], lacIndex, false);

            AddOutputAptViewState(msaacNameIDs[liRow],   mEATraxHelper.GetSongName(liTrack),   false);
            AddOutputAptViewState(msaacArtistIDs[liRow], mEATraxHelper.GetArtistName(liTrack), false);
            AddOutputAptViewState(msaacAlbumIDs[liRow],  mEATraxHelper.GetAlbumName(liTrack),  false);
            AddOutputAptViewState(msaacEnabledIDs[liRow],
                                  KAPC_TRAX_PLAY_STATE[GetTrackMode(liTrack)], false);
        }

        // The scroll bar, as two integer percentages. The console's own arithmetic
        // (0x824276E8..0x82427788): the denominator is mTotalNumberTracks + 4, i.e. the
        // track count plus one window minus one, and the reciprocal is computed ONCE and
        // reused for both ends -- reproduced in that order so the truncation matches.
        const f32 lfStep = 1.0f / static_cast<f32>(mTotalNumberTracks +
                                                   (KI_NUM_VISIBLE_TRACKS - 1));
        const f32 lfHighlighted = static_cast<f32>(mCurrentHighlightedIndex);

        char lacBarStart[8];
        CgsCore::SPrintf(lacBarStart, sizeof(lacBarStart), "%d",
                         static_cast<s32>((lfHighlighted * 100.0f) * lfStep));

        char lacBarEnd[8];
        CgsCore::SPrintf(lacBarEnd, sizeof(lacBarEnd), "%d",
                         static_cast<s32>(((KF_NUM_VISIBLE_TRACKS + lfHighlighted) * lfStep)
                                          * 100.0f));

        AddOutputAptViewState(mspacBarStartID, lacBarStart, false);
        AddOutputAptViewState(mspacBarEndID,   lacBarEnd,   false);
    }

    // ---- GetTrackMode @0x82426A60 --------------------------------------------------
    // Decode the 2-bit mode for liTrackIndex. The X360 asserts the index against
    // KI_MAX_TRAX_IN_GAME first (BrnEATraxMenuComponent.h:281), then reads the high bit
    // (the EVENTS set at +0xB0) and the low bit (the FREEBURN set at +0xA0). Both
    // contribute INVERTED:  result = (high ? 0 : 2) + (low ? 0 : 1)
    // (the X360's repeated 128-bound checks are FastBitArray's own inlined bounds asserts,
    // owned by the committed container; not duplicated here).
    EATraxMenuComponent::ETraxPlayState
    EATraxMenuComponent::GetTrackMode(s32 liTrackIndex) const
    {
        CGS_ASSERT(liTrackIndex >= 0 && liTrackIndex < KI_MAX_TRAX_IN_GAME,
                   "(liTrackIndex >= 0) && (liTrackIndex < KI_MAX_TRAX_IN_GAME)");

        const u32 luIndex = static_cast<u32>(liTrackIndex);
        const bool lbHigh = mEATraxEnabledInEventsBitfield.IsBitSet(luIndex);
        const bool lbLow  = mEATraxEnabledInFreeBurnBitfield.IsBitSet(luIndex);

        const s32 liHighPart = lbHigh ? 0 : 2;
        const s32 liLowPart  = lbLow  ? 0 : 1;
        return static_cast<ETraxPlayState>(liHighPart + liLowPart);
    }

    // ---- SetTrackMode @0x82426DA0 --------------------------------------------------
    // Encode leMode into the two bit sets (inverted, see GetTrackMode). Each X360 case
    // writes the events set (+0xB0) then the freeburn set (+0xA0); an out-of-range mode
    // fires "Invalid mode" (BrnEATraxMenuComponent.h:341).
    void EATraxMenuComponent::SetTrackMode(s32 liTrackIndex, ETraxPlayState leMode)
    {
        const u32 luIndex = static_cast<u32>(liTrackIndex);

        switch (leMode)
        {
        case E_TRAX_PLAY_STATE_ENABLED:        // allowed everywhere: both bits set
            mEATraxEnabledInEventsBitfield.SetBit(luIndex);
            mEATraxEnabledInFreeBurnBitfield.SetBit(luIndex);
            break;
        case E_TRAX_PLAY_STATE_EVENTONLY:
            mEATraxEnabledInEventsBitfield.SetBit(luIndex);
            mEATraxEnabledInFreeBurnBitfield.UnSetBit(luIndex);
            break;
        case E_TRAX_PLAY_STATE_FREEBURNONLY:
            mEATraxEnabledInEventsBitfield.UnSetBit(luIndex);
            mEATraxEnabledInFreeBurnBitfield.SetBit(luIndex);
            break;
        case E_TRAX_PLAY_STATE_DISABLED:       // allowed nowhere: both bits clear
            mEATraxEnabledInEventsBitfield.UnSetBit(luIndex);
            mEATraxEnabledInFreeBurnBitfield.UnSetBit(luIndex);
            break;
        default:
            CGS_ASSERT(false, "Invalid mode");
            break;
        }
    }
}
