#pragma once

// ===================================================================================
// BrnGui::EATraxMenuComponent  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnEATraxMenuComponent.h
//
// THE EA TRAX TRACK LIST. The scrolling five-row list on the pause menu's EA Trax tab
// (CN_TRAX): one row per song, each showing its number, name, artist, album and per-track
// play state, plus the play-order caption and the two scroll-bar anchors.
//
// ⭐⭐ WHY THIS HEADER GREW (2026-09-16). It previously declared exactly two methods --
// GetTrackMode / SetTrackMode -- over a `u8 maComponentHeadReserved[0xA0]` stand-in for
// "the GUI-component head and the fields preceding the bit sets", with the note "not
// reconstructed by this TU". Everything in that reserved span is now recovered and named,
// and the class derives from CgsGui::GuiComponent as the DWARF says it does
// (BrnEATraxMenuComponent.h:125) -- which is what makes the component's own name reachable
// (Construct passes macName as the TextField's parent prefix, and CrashNavTrax::UpdateLoading
// registers GetName() as the screen's one expected apt component).
//
// LAYOUT, measured from the X360 bodies and closing exactly against the DWARF member list:
//   +0x0000  CgsGui::GuiComponent base (vptr, macName[128], muHashedName, mpStateInterface
//            @+0x88 -- the `v2[34]` HandleInput hands to OutputGuiEvent)
//   +0x008C  mEATraxHelper                     (GetNumSongs(this+140); the helper is
//                                               STATELESS, hence the 4-byte slot)
//   +0x0090  mTotalNumberTracks                (Initialize's `*(a1+144) = NumSongs`)
//   +0x0094  mCurrentHighlightedIndex          (HandleInput's a1[37])
//   +0x0098  mCurrentTopIndex                  (HandleInput's a1[38])
//   +0x00A0  mEATraxEnabledInFreeBurnBitfield  (FastBitArray<128>, 16 bytes)
//   +0x00B0  mEATraxEnabledInEventsBitfield    (FastBitArray<128>, 16 bytes)
//   +0x00C0  mbDrawSendPending                 (Update's gate)
//   +0x00C1  mbSettingsChanged                 (`stb r11, 0xC1(r31)`: cleared by Construct
//                                               @0x824196C8, set by UpdatePlayOrderMode
//                                               @0x82419858 and the three HandleInput
//                                               state-change arms)
//   +0x00C4  mPlayMode                         (BrnGui::TextField, 0x128 bytes)
// The two bit sets encode a 2-BIT PER-TRACK MODE, and they encode it INVERTED:
//   mode 0 -> high=set,   low=set        GetTrackMode = (high ? 0 : 2) + (low ? 0 : 1)
//   mode 1 -> high=set,   low=clear
//   mode 2 -> high=clear, low=set
//   mode 3 -> high=clear, low=clear
// "high" is mEATraxEnabledInEventsBitfield (+0xB0) and "low" is the FreeBurn one (+0xA0),
// which is why the four modes read ENABLED / EVENTONLY / FREEBURNONLY / DISABLED: a track
// is "enabled" exactly when it is allowed in BOTH contexts.
//
// ⚠️ THE SIX APT ID TABLES ARE CLASS STATICS, NOT PER-INSTANCE (DWARF :240..:245, and the
// X360 writes them to fixed .bss at 0x82FB2790 / 0x82FB0070 / 0x82FB00D0 / 0x82FB2740 /
// 0x82FB0120 / 0x82FB26F0). BuildStringIDs re-formats all six every Initialize.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"      // FastBitArray<128>
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent (base)
#include "GameSource/Gui/Events/BrnGuiEventAudioTrax.h"   // EATraxArrayType / ETraxPlayOrderMode
#include "GameSource/Gui/BrnGuiTextField.h"                         // TextField (mPlayMode)
#include "GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h"  // EaTraxHelper

namespace BrnGui
{
    struct EATraxMenuComponent : public CgsGui::GuiComponent
    {
        // BrnEATraxMenuComponent.h:281 - "(liTrackIndex >= 0) && (liTrackIndex < KI_MAX_TRAX_IN_GAME)".
        static const s32 KI_MAX_TRAX_IN_GAME = 128;

        // DWARF :128/:129. The list shows five rows; KF_ is the same number as an f32,
        // used by the scroll-bar arithmetic (X360 .rdata flt_82F25640 == 5.0f).
        static const s32 KI_NUM_VISIBLE_TRACKS = 5;
        static const f32 KF_NUM_VISIBLE_TRACKS;

        // DWARF :131 -- what CrashNavTrax::HandleControllerInput asks this component to do.
        enum InputCommand
        {
            E_INPUTCOMMAND_HIGHLIGHT_PREVIOUS_TRACK               = 0,
            E_INPUTCOMMAND_HIGHLIGHT_NEXT_TRACK                   = 1,
            E_INPUTCOMMAND_CHANGE_CURRENT_TRACK_STATE_TO_PREVIOUS = 2,
            E_INPUTCOMMAND_CHANGE_CURRENT_TRACK_STATE_TO_NEXT     = 3,
            E_INPUTCOMMAND_CHANGE_ALL_TRACKS_TO_NEXT              = 4,
        };

        // DWARF :141 -- the per-track mode, and the index into KAPC_TRAX_PLAY_STATE.
        enum ETraxPlayState
        {
            E_TRAX_PLAY_STATE_ENABLED      = 0,
            E_TRAX_PLAY_STATE_EVENTONLY    = 1,
            E_TRAX_PLAY_STATE_FREEBURNONLY = 2,
            E_TRAX_PLAY_STATE_DISABLED     = 3,
            E_TRAX_PLAY_STATE_COUNT        = 4,
        };

        // @0x82419680 -- run the base component Construct, then build the play-order
        // caption field as a CHILD of this component (parent prefix = macName) and clear
        // the settings-changed latch.
        // ⚠️ NOT virtual: the DWARF declares it plain and the X360 call site
        // (CrashNavTrax::OnEnter) calls it directly. The 4th argument is the DWARF's
        // (BrnEATraxMenuComponent.h:157) and the X360 body never reads it.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, s32 liUnused);

        // @0x8242D790 -- adopt the profile's two bit fields, ask the song list how many
        // tracks exist, rebuild the apt id tables and arm the first draw.
        void Initialize(const GuiEventAudioTraxUpdate::EATraxArrayType* lpTraxEnabledInFreeBurn,
                        const GuiEventAudioTraxUpdate::EATraxArrayType* lpTraxEnabledInEvents);

        // @0x8242D858 -- push a redraw when one is pending, and only then.
        void Update();

        // @0x8243BC40 -- move the highlight / change one track's state / change them all.
        void HandleInput(InputCommand leCommand);

        // @0x824197F0 -- set the play-order caption and latch "settings changed".
        void UpdatePlayOrderMode(GuiEventAudioTraxPlayOrder::ETraxPlayOrderMode lePlayOrderMode);

        // DWARF :261/:267/:273/:348/:356 -- inline accessors; no X360 symbols (the console
        // inlines each at its CrashNavTrax call site).
        const GuiEventAudioTraxUpdate::EATraxArrayType* GetTraxEnabledInFreeBurnBitfieldPtr() const
        {
            return &mEATraxEnabledInFreeBurnBitfield;
        }
        const GuiEventAudioTraxUpdate::EATraxArrayType* GetTraxEnabledInEventsBitfieldPtr() const
        {
            return &mEATraxEnabledInEventsBitfield;
        }
        s32  GetHighlightedTrackIndex() const { return mCurrentHighlightedIndex; }
        bool SettingsChanged() const          { return mbSettingsChanged; }
        void ResetSettingsChanged()           { mbSettingsChanged = false; }

    private:
        void BuildStringIDs();                            // @0x824196E8
        void SendDrawInformationToApt();                  // @0x82427560

        // DWARF :206/:210 -- inline either side of mbDrawSendPending.
        bool IsDrawRequestPending() const { return mbDrawSendPending; }
        void SetSendDrawInformationPending(bool lbPending) { mbDrawSendPending = lbPending; }

        // @0x82426A60 / @0x82426DA0 -- the inverted 2-bit codec described in the banner.
        ETraxPlayState GetTrackMode(s32 liTrackIndex) const;
        void           SetTrackMode(s32 liTrackIndex, ETraxPlayState leMode);

        // DWARF :237/:240..:248/:251/:253/:254 -- class statics, see the banner.
        static const u32 KU_MAX_APT_COMMS_STRING_ID_SIZE = 16;

        static char msaacVisibleIDs  [KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE];
        static char msaacSongIndexIDs[KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE];
        static char msaacNameIDs     [KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE];
        static char msaacArtistIDs   [KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE];
        static char msaacAlbumIDs    [KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE];
        static char msaacEnabledIDs  [KI_NUM_VISIBLE_TRACKS][KU_MAX_APT_COMMS_STRING_ID_SIZE];

        static const char* mspacBarStartID;          // .rdata off_82F25320 == "BarStart"
        static const char* mspacBarEndID;            // .rdata off_82F25324 == "BarEnd"
        static const char  KAC_PLAY_ORDER_MODE_NAME[12];      // "PlayMode_mc"
        static const char* KAPC_PLAY_ORDER_MODES[2];          // .rdata off_82F25328
        static const char* KAPC_TRAX_PLAY_STATE[E_TRAX_PLAY_STATE_COUNT];   // off_82F25330

        BrnSound::Module::Io::EaTraxHelper mEATraxHelper;      // +0x008C
        s32 mTotalNumberTracks;                                // +0x0090
        s32 mCurrentHighlightedIndex;                          // +0x0094
        s32 mCurrentTopIndex;                                  // +0x0098

        GuiEventAudioTraxUpdate::EATraxArrayType mEATraxEnabledInFreeBurnBitfield;      // +0x00A0 ("low" bit)
        GuiEventAudioTraxUpdate::EATraxArrayType mEATraxEnabledInEventsBitfield;        // +0x00B0 ("high" bit)
        bool mbDrawSendPending;                                // +0x00C0
        bool mbSettingsChanged;                                // +0x00C1
        TextField mPlayMode;                                   // +0x00C4
    };
}
